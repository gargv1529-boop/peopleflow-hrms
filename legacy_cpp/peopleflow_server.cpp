// PeopleFlow HRMS — educational, local-only C++17 HTTP + SQLite server.
// Dependencies: SQLite3 and OpenSSL; see README before using any real data.
#include <sqlite3.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#ifndef _WIN32
#include <csignal>
#endif
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using socket_t = SOCKET;
static constexpr socket_t invalid_socket = INVALID_SOCKET;
static void close_socket(socket_t s) { closesocket(s); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
static constexpr socket_t invalid_socket = -1;
static void close_socket(socket_t s) { close(s); }
#endif
namespace fs = std::filesystem;
using Vars = std::map<std::string,std::string>;
static sqlite3* db=nullptr;
static int server_port=8765;
static std::string web_root="web";

static void sql(const std::string& statement) {
  char* err=nullptr;
  if(sqlite3_exec(db,statement.c_str(),nullptr,nullptr,&err)!=SQLITE_OK){
    std::string message=err?err:"SQLite error"; sqlite3_free(err); throw std::runtime_error(message);
  }
}
struct Stmt {
  sqlite3_stmt* s=nullptr;
  explicit Stmt(const std::string& query){
    if(sqlite3_prepare_v2(db,query.c_str(),-1,&s,nullptr)!=SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
  }
  ~Stmt(){ sqlite3_finalize(s); }
  Stmt(const Stmt&)=delete; Stmt& operator=(const Stmt&)=delete;
  void bind(int index,const std::string& value){sqlite3_bind_text(s,index,value.c_str(),-1,SQLITE_TRANSIENT);}
  void bind(int index,int value){sqlite3_bind_int(s,index,value);}
  void bind(int index,double value){sqlite3_bind_double(s,index,value);}
  bool step(){int x=sqlite3_step(s);if(x==SQLITE_ROW)return true;if(x==SQLITE_DONE)return false;throw std::runtime_error(sqlite3_errmsg(db));}
  int id(){return (int)sqlite3_last_insert_rowid(db);}
};
static std::string value(sqlite3_stmt* s,int col){auto p=sqlite3_column_text(s,col);return p?(const char*)p:"";}
static int scalar(const std::string& query){Stmt s(query);return s.step()?sqlite3_column_int(s.s,0):0;}
static std::string escape(const std::string& x){
  std::ostringstream out; out<<'"';
  for(unsigned char c:x){switch(c){
    case '"':out<<"\\\"";break;case '\\':out<<"\\\\";break;case '\n':out<<"\\n";break;
    case '\r':out<<"\\r";break;case '\t':out<<"\\t";break;
    default: if(c<32){out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<(int)c<<std::dec;} else out<<(char)c;
  }} out<<'"'; return out.str();
}
static std::string json_rows(Stmt& stmt){
  std::string j="[";bool first=true;
  while(stmt.step()){
    if(!first)j+=",";
    first=false;j+="{";
    int count=sqlite3_column_count(stmt.s);
    for(int i=0;i<count;i++){
      if(i)j+=",";
      j+=escape(sqlite3_column_name(stmt.s,i));j+=":";
      int typ=sqlite3_column_type(stmt.s,i);
      if(typ==SQLITE_NULL)j+="null";
      else if(typ==SQLITE_INTEGER || typ==SQLITE_FLOAT){j+=(const char*)sqlite3_column_text(stmt.s,i);}
      else j+=escape(value(stmt.s,i));
    }
    j+="}";
  }
  return j+"]";
}
static std::string hex(const unsigned char* bytes,size_t n){
  const char* table="0123456789abcdef";std::string s;s.reserve(n*2);
  for(size_t i=0;i<n;i++){s.push_back(table[bytes[i]>>4]);s.push_back(table[bytes[i]&15]);}return s;
}
static std::vector<unsigned char> unhex(const std::string& s){
  if(s.size()%2)throw std::runtime_error("Invalid hash");
  std::vector<unsigned char> b;
  for(size_t i=0;i<s.size();i+=2){int a=std::stoi(s.substr(i,2),nullptr,16);b.push_back((unsigned char)a);}return b;
}
static std::string random_hex(int n){std::vector<unsigned char> x((size_t)n);if(RAND_bytes(x.data(),n)!=1)throw std::runtime_error("Secure random failure");return hex(x.data(),x.size());}
static std::string hash_token(const std::string& token){unsigned char digest[SHA256_DIGEST_LENGTH];SHA256((const unsigned char*)token.data(),token.size(),digest);return hex(digest,SHA256_DIGEST_LENGTH);}
static std::string password_hash(const std::string& password){
  std::string salt=random_hex(16);auto bytes=unhex(salt);unsigned char digest[32];
  if(PKCS5_PBKDF2_HMAC(password.c_str(),(int)password.size(),bytes.data(),(int)bytes.size(),200000,EVP_sha256(),32,digest)!=1)
    throw std::runtime_error("PBKDF2 failed");
  return "pbkdf2$200000$"+salt+"$"+hex(digest,32);
}
static bool verify_password(const std::string& password,const std::string& encoded){
  size_t p1=encoded.find('$'),p2=encoded.find('$',p1==std::string::npos?0:p1+1),p3=encoded.find('$',p2==std::string::npos?0:p2+1);
  if(p1==std::string::npos||p2==std::string::npos||p3==std::string::npos)return false;
  if(encoded.substr(0,p1)!="pbkdf2")return false;
  int rounds=0;try{rounds=std::stoi(encoded.substr(p1+1,p2-p1-1));}catch(...){return false;}
  if(rounds<100000||rounds>1000000)return false;
  try{
    auto salt=unhex(encoded.substr(p2+1,p3-p2-1));auto actual=unhex(encoded.substr(p3+1));
    if(actual.size()!=32)return false;
    unsigned char digest[32];
    if(PKCS5_PBKDF2_HMAC(password.c_str(),(int)password.size(),salt.data(),(int)salt.size(),rounds,EVP_sha256(),32,digest)!=1)return false;
    unsigned char diff=0;for(int i=0;i<32;i++)diff|=(unsigned char)(digest[i]^actual[(size_t)i]);return diff==0;
  }catch(...){return false;}
}
static long long now(){return (long long)std::time(nullptr);}
static std::string trim(std::string s){size_t a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");return a==std::string::npos?"":s.substr(a,b-a+1);}
static std::string lower(std::string s){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return (char)std::tolower(c);});return s;}
static bool valid_email(const std::string& s){return s.size()<=254 && s.find(' ') == std::string::npos && s.find('@')!=std::string::npos && s.find('.',s.find('@'))!=std::string::npos;}
static bool valid_date(const std::string& s){
  if(!std::regex_match(s,std::regex("[0-9]{4}-[0-9]{2}-[0-9]{2}")))return false;
  int y=std::stoi(s.substr(0,4)),m=std::stoi(s.substr(5,2)),d=std::stoi(s.substr(8,2));
  if(y<1900||y>2100||m<1||m>12)return false;
  int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  if(m==2 && (y%400==0||(y%4==0&&y%100!=0)))days[1]=29;
  return d>=1&&d<=days[m-1];
}
static bool nonempty(const std::string& s,size_t max=100){return !trim(s).empty()&&s.size()<=max;}
static std::string get(const Vars& v,const std::string& k){auto it=v.find(k);return it==v.end()?"":it->second;}
static int to_id(const std::string& s){try{size_t pos=0;int n=std::stoi(s,&pos);return pos==s.size()&&n>0?n:0;}catch(...){return 0;}}
static bool salary_value(const std::string& s,double& v){try{size_t n=0;v=std::stod(s,&n);return n==s.size()&&std::isfinite(v)&&v>=0&&v<=1000000000;}catch(...){return false;}}
static void audit(int actor,const std::string& action,const std::string& entity,int entity_id){
  Stmt q("INSERT INTO audit_log(actor_id,action,entity,entity_id,created_at) VALUES(?,?,?,?,datetime('now'))");
  q.bind(1,actor);q.bind(2,action);q.bind(3,entity);q.bind(4,entity_id);q.step();
}
static void initialize(){
  sql("PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA busy_timeout=3000;");
  sql("CREATE TABLE IF NOT EXISTS employees(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,email TEXT NOT NULL UNIQUE COLLATE NOCASE,department TEXT NOT NULL,job_title TEXT NOT NULL,annual_salary REAL NOT NULL CHECK(annual_salary>=0),joined_on TEXT NOT NULL,status TEXT NOT NULL DEFAULT 'Active' CHECK(status IN ('Active','On Leave','Inactive')));");
  sql("CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,email TEXT NOT NULL UNIQUE COLLATE NOCASE,pass_hash TEXT NOT NULL,role TEXT NOT NULL CHECK(role IN ('Admin','HR','Employee')),employee_id INTEGER UNIQUE REFERENCES employees(id) ON DELETE SET NULL);");
  sql("CREATE TABLE IF NOT EXISTS sessions(token_hash TEXT PRIMARY KEY,user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,csrf TEXT NOT NULL,expires_at INTEGER NOT NULL);");
  sql("CREATE TABLE IF NOT EXISTS attendance(id INTEGER PRIMARY KEY AUTOINCREMENT,employee_id INTEGER NOT NULL REFERENCES employees(id) ON DELETE CASCADE,day TEXT NOT NULL,status TEXT NOT NULL CHECK(status IN ('Present','Absent','Remote')),marked_by INTEGER REFERENCES users(id) ON DELETE SET NULL,UNIQUE(employee_id,day));");
  sql("CREATE TABLE IF NOT EXISTS leave_requests(id INTEGER PRIMARY KEY AUTOINCREMENT,employee_id INTEGER NOT NULL REFERENCES employees(id) ON DELETE CASCADE,start_date TEXT NOT NULL,end_date TEXT NOT NULL,reason TEXT NOT NULL,status TEXT NOT NULL DEFAULT 'Pending' CHECK(status IN ('Pending','Approved','Rejected')),created_by INTEGER REFERENCES users(id) ON DELETE SET NULL,reviewed_by INTEGER REFERENCES users(id) ON DELETE SET NULL,created_at TEXT NOT NULL DEFAULT (datetime('now')));");
  sql("CREATE TABLE IF NOT EXISTS audit_log(id INTEGER PRIMARY KEY AUTOINCREMENT,actor_id INTEGER REFERENCES users(id) ON DELETE SET NULL,action TEXT NOT NULL,entity TEXT NOT NULL,entity_id INTEGER,created_at TEXT NOT NULL);");
  sql("CREATE TABLE IF NOT EXISTS login_failures(email TEXT PRIMARY KEY,attempts INTEGER NOT NULL,locked_until INTEGER NOT NULL);");
  sql("DELETE FROM sessions WHERE expires_at < CAST(strftime('%s','now') AS INTEGER);");
  if(scalar("SELECT COUNT(*) FROM users")>0)return;
  std::string pass=std::getenv("PEOPLEFLOW_DEMO_PASSWORD")?std::getenv("PEOPLEFLOW_DEMO_PASSWORD"):"Demo@123";
  if(pass.size()<8)throw std::runtime_error("PEOPLEFLOW_DEMO_PASSWORD must be at least 8 characters");
  sql("BEGIN IMMEDIATE;");
  try{
    const char* people[][6]={{"Aarav Sharma","aarav@peopleflow.demo","Engineering","Software Engineer","780000","2025-08-12"},
    {"Riya Singh","riya@peopleflow.demo","People Ops","HR Specialist","640000","2025-02-01"},
    {"Meera Das","meera@peopleflow.demo","Design","Product Designer","720000","2025-05-21"},
    {"Kabir Jain","kabir@peopleflow.demo","Engineering","QA Engineer","580000","2026-01-10"},
    {"Sana Khan","sana@peopleflow.demo","Marketing","Growth Associate","620000","2026-03-01"}};
    for(const auto& p:people){Stmt s("INSERT INTO employees(name,email,department,job_title,annual_salary,joined_on) VALUES(?,?,?,?,?,?)");
      for(int k=0;k<4;k++)s.bind(k+1,p[k]);
      s.bind(5,std::stod(p[4]));s.bind(6,p[5]);s.step();}
    const char* roles[][4]={{"Admin User","admin@peopleflow.demo","Admin",""},{"HR Manager","hr@peopleflow.demo","HR",""},{"Aarav Sharma","employee@peopleflow.demo","Employee","1"}};
    for(const auto& p:roles){Stmt s("INSERT INTO users(name,email,pass_hash,role,employee_id) VALUES(?,?,?,?,?)");
      s.bind(1,p[0]);s.bind(2,p[1]);s.bind(3,password_hash(pass));s.bind(4,p[2]);
      if(*p[3])s.bind(5,std::stoi(p[3]));else sqlite3_bind_null(s.s,5);s.step();}
    sql("INSERT INTO attendance(employee_id,day,status,marked_by) VALUES(1,date('now'),'Present',1),(2,date('now'),'Remote',1),(3,date('now'),'Present',1);");
    sql("INSERT INTO leave_requests(employee_id,start_date,end_date,reason,created_by) VALUES(1,date('now','+7 days'),date('now','+8 days'),'Family occasion',3);");
    sql("COMMIT;");
  }catch(...){sql("ROLLBACK;");throw;}
}
static int from_hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static std::string decode(const std::string& s){std::string r;for(size_t i=0;i<s.size();i++){
  if(s[i]=='+' )r+=' ';else if(s[i]=='%'&&i+2<s.size()&&from_hex(s[i+1])>=0&&from_hex(s[i+2])>=0){r+=(char)(from_hex(s[i+1])*16+from_hex(s[i+2]));i+=2;}else r+=s[i];}return r;}
static Vars parse_vars(const std::string& query){Vars v;std::istringstream stream(query);std::string item;
  while(std::getline(stream,item,'&')){auto p=item.find('=');v[decode(item.substr(0,p))]=p==std::string::npos?"":decode(item.substr(p+1));}return v;
}
struct Request{std::string method,path,body;Vars headers,params;};
struct Response{int status=200;std::string content="{}",mime="application/json; charset=utf-8",cookie="";};
static Response good(const std::string& data){return {200,"{\"ok\":true,\"data\":"+data+"}"};}
static Response error(int code,const std::string& reason){return {code,"{\"ok\":false,\"error\":"+escape(reason)+"}"};}
static int route_id(const std::string& path,const std::string& prefix){if(path.rfind(prefix,0)!=0)return 0;return to_id(path.substr(prefix.size()));}
struct Auth{int id=0,employee_id=0;std::string name,email,role,csrf,token;bool logged_in()const{return id>0;}};
static Auth authorize(const Request& req){
  auto it=req.headers.find("cookie");if(it==req.headers.end())return {};
  std::string cookies=it->second,token;std::istringstream ss(cookies);std::string part;
  while(std::getline(ss,part,';')){part=trim(part);if(part.rfind("pf_session=",0)==0){token=part.substr(11);break;}}
  if(token.size()!=64||!std::all_of(token.begin(),token.end(),[](char x){return std::isxdigit((unsigned char)x)!=0;}))return {};
  Stmt s("SELECT u.id,u.name,u.email,u.role,COALESCE(u.employee_id,0),s.csrf FROM sessions s JOIN users u ON u.id=s.user_id WHERE s.token_hash=? AND s.expires_at>?");
  s.bind(1,hash_token(token));s.bind(2,(int)now());
  if(!s.step())return {};
  Auth a;a.id=sqlite3_column_int(s.s,0);a.name=value(s.s,1);a.email=value(s.s,2);a.role=value(s.s,3);a.employee_id=sqlite3_column_int(s.s,4);a.csrf=value(s.s,5);a.token=token;return a;
}
static bool can_manage(const Auth& a){return a.role=="Admin"||a.role=="HR";}
static std::string user_json(const Auth& a){return "{\"id\":"+std::to_string(a.id)+",\"name\":"+escape(a.name)+",\"email\":"+escape(a.email)+",\"role\":"+escape(a.role)+",\"employee_id\":"+(a.employee_id?std::to_string(a.employee_id):"null")+"}";}
static std::string csv_field(const std::string& x){
  std::string safe=x;
  if(!safe.empty() && (safe[0]=='='||safe[0]=='+'||safe[0]=='-'||safe[0]=='@'))safe.insert(safe.begin(),'\'');
  std::string s="\"";for(char c:safe){if(c=='\"')s+="\"\"";else if(c=='\r'||c=='\n')s+=' ';else s+=c;}return s+'"';}
static Response export_csv(Stmt& q){std::string s;int n=sqlite3_column_count(q.s);
  for(int i=0;i<n;i++){if(i)s+=",";s+=csv_field(sqlite3_column_name(q.s,i));}s+="\r\n";
  while(q.step()){for(int i=0;i<n;i++){if(i)s+=",";s+=csv_field(value(q.s,i));}s+="\r\n";}
  return {200,s,"text/csv; charset=utf-8"};
}
static Response api(const Request& r){
  if(r.path=="/api/login" && r.method=="POST"){
    std::string email=lower(trim(get(r.params,"email"))),password=get(r.params,"password");
    if(!valid_email(email)||password.empty())return error(400,"Email and password are required");
    Stmt lock("SELECT attempts,locked_until FROM login_failures WHERE email=?");lock.bind(1,email);
    if(lock.step()&&sqlite3_column_int64(lock.s,1)>now())return error(429,"Too many attempts. Try again after 15 minutes");
    Stmt u("SELECT id,pass_hash FROM users WHERE email=?");u.bind(1,email);
    int id=0;bool valid=false;
    if(u.step()){id=sqlite3_column_int(u.s,0);valid=verify_password(password,value(u.s,1));}
    if(!valid){Stmt f("INSERT INTO login_failures(email,attempts,locked_until) VALUES(?,1,0) ON CONFLICT(email) DO UPDATE SET attempts=attempts+1,locked_until=CASE WHEN attempts+1>=5 THEN ? ELSE 0 END");
      f.bind(1,email);f.bind(2,(int)(now()+900));f.step();return error(401,"Invalid email or password");}
    Stmt reset("DELETE FROM login_failures WHERE email=?");reset.bind(1,email);reset.step();
    std::string token=random_hex(32),csrf=random_hex(24);
    Stmt ses("INSERT INTO sessions(token_hash,user_id,csrf,expires_at) VALUES(?,?,?,?)");
    ses.bind(1,hash_token(token));ses.bind(2,id);ses.bind(3,csrf);ses.bind(4,(int)(now()+12*3600));ses.step();
    Response out=good("{\"message\":\"Signed in\"}");
    out.cookie="pf_session="+token+"; HttpOnly; SameSite=Strict; Path=/; Max-Age=43200";return out;
  }
  Auth a=authorize(r);
  if(!a.logged_in())return error(401,"Please sign in");
  if(r.method!="GET" && get(r.headers,"x-csrf-token")!=a.csrf)return error(403,"Invalid CSRF token");
  if(r.path=="/api/me"&&r.method=="GET")return good("{\"user\":"+user_json(a)+",\"csrf\":"+escape(a.csrf)+"}");
  if(r.path=="/api/logout"&&r.method=="POST"){
    Stmt s("DELETE FROM sessions WHERE token_hash=?");s.bind(1,hash_token(a.token));s.step();Response o=good("null");o.cookie="pf_session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0";return o;
  }
  if(r.path=="/api/password"&&r.method=="POST"){
    std::string old=get(r.params,"old_password"),pwd=get(r.params,"new_password");
    if(pwd.size()<8||pwd.size()>128)return error(400,"New password must be 8 to 128 characters");
    Stmt q("SELECT pass_hash FROM users WHERE id=?");q.bind(1,a.id);
    if(!q.step()||!verify_password(old,value(q.s,0)))return error(400,"Current password is incorrect");
    Stmt update("UPDATE users SET pass_hash=? WHERE id=?");update.bind(1,password_hash(pwd));update.bind(2,a.id);update.step();
    Stmt s("DELETE FROM sessions WHERE user_id=? AND token_hash<>?");s.bind(1,a.id);s.bind(2,hash_token(a.token));s.step();
    audit(a.id,"password_changed","user",a.id);return good("null");
  }
  if(r.path=="/api/dashboard"&&r.method=="GET"){
    std::string j="{";
    if(can_manage(a)){
      j+="\"employees\":"+std::to_string(scalar("SELECT COUNT(*) FROM employees"))+",";
      j+="\"active\":"+std::to_string(scalar("SELECT COUNT(*) FROM employees WHERE status='Active'"))+",";
      j+="\"pending\":"+std::to_string(scalar("SELECT COUNT(*) FROM leave_requests WHERE status='Pending'"))+",";
      Stmt pay("SELECT COALESCE(ROUND(SUM(annual_salary)/12,2),0) FROM employees WHERE status!='Inactive'");pay.step();j+="\"payroll\":"+value(pay.s,0)+",";
      Stmt deps("SELECT department,COUNT(*) AS headcount FROM employees GROUP BY department ORDER BY headcount DESC");j+="\"departments\":"+json_rows(deps);
    }else{
      Stmt pending("SELECT COUNT(*) FROM leave_requests WHERE employee_id=? AND status='Pending'");pending.bind(1,a.employee_id);pending.step();
      Stmt att("SELECT COUNT(*) FROM attendance WHERE employee_id=? AND status IN ('Present','Remote')");att.bind(1,a.employee_id);att.step();
      j+="\"employees\":"+std::to_string(a.employee_id?1:0)+",\"active\":"+std::to_string(a.employee_id?1:0)+",\"pending\":"+value(pending.s,0)+",\"attendance_days\":"+value(att.s,0)+",\"payroll\":null,\"departments\":[]";
    }
    return good(j+"}");
  }
  if(r.path=="/api/employees"&&r.method=="GET"){
    std::string term=get(r.params,"q");
    if(!can_manage(a)){
      if(!a.employee_id)return good("[]");
      Stmt q("SELECT id,name,email,department,job_title,annual_salary,joined_on,status FROM employees WHERE id=?");q.bind(1,a.employee_id);return good(json_rows(q));
    }
    Stmt q("SELECT id,name,email,department,job_title,annual_salary,joined_on,status FROM employees WHERE name LIKE ? OR department LIKE ? OR email LIKE ? ORDER BY id DESC");
    for(int i=1;i<=3;i++)q.bind(i,"%"+term+"%");
    return good(json_rows(q));
  }
  if((r.path=="/api/employees"&&r.method=="POST")||(route_id(r.path,"/api/employees/")&&r.method=="PUT")){
    if(!can_manage(a))return error(403,"Only Admin or HR can edit employees");
    int id=r.method=="PUT"?route_id(r.path,"/api/employees/"):0;
    std::string name=trim(get(r.params,"name")),email=lower(trim(get(r.params,"email"))),department=trim(get(r.params,"department"));
    std::string title=trim(get(r.params,"job_title")),date=get(r.params,"joined_on"),status=get(r.params,"status");
    double salary=0;
    if(!nonempty(name)||!valid_email(email)||!nonempty(department)||!nonempty(title)||!valid_date(date)||!salary_value(get(r.params,"annual_salary"),salary)||
      (status!="Active"&&status!="On Leave"&&status!="Inactive"))return error(400,"Check employee name, email, department, title, salary, joining date and status");
    if(id){Stmt exists("SELECT COUNT(*) FROM employees WHERE id=?");exists.bind(1,id);exists.step();if(sqlite3_column_int(exists.s,0)==0)return error(404,"Employee not found");}
    Stmt q(id?"UPDATE employees SET name=?,email=?,department=?,job_title=?,annual_salary=?,joined_on=?,status=? WHERE id=?":
            "INSERT INTO employees(name,email,department,job_title,annual_salary,joined_on,status) VALUES(?,?,?,?,?,?,?)");
    q.bind(1,name);q.bind(2,email);q.bind(3,department);q.bind(4,title);q.bind(5,salary);q.bind(6,date);q.bind(7,status);if(id)q.bind(8,id);q.step();
    int ref=id?id:q.id();audit(a.id,id?"employee_updated":"employee_created","employee",ref);
    return good("{\"id\":"+std::to_string(ref)+"}");
  }
  if(route_id(r.path,"/api/employees/")&&r.method=="DELETE"){
    if(!can_manage(a))return error(403,"Only Admin or HR can delete employees");
    int id=route_id(r.path,"/api/employees/");Stmt q("DELETE FROM employees WHERE id=?");q.bind(1,id);q.step();
    if(sqlite3_changes(db)==0)return error(404,"Employee not found");
    audit(a.id,"employee_deleted","employee",id);return good("null");
  }
  if(r.path=="/api/attendance"&&r.method=="GET"){
    if(!can_manage(a)){
      Stmt q("SELECT a.id,a.employee_id,e.name AS employee,a.day,a.status FROM attendance a JOIN employees e ON e.id=a.employee_id WHERE a.employee_id=? ORDER BY a.day DESC LIMIT 200");
      q.bind(1,a.employee_id);return good(json_rows(q));
    }
    Stmt q("SELECT a.id,a.employee_id,e.name AS employee,a.day,a.status FROM attendance a JOIN employees e ON e.id=a.employee_id ORDER BY a.day DESC,a.id DESC LIMIT 300");return good(json_rows(q));
  }
  if(r.path=="/api/attendance"&&r.method=="POST"){
    if(!can_manage(a))return error(403,"Only Admin or HR can mark attendance");
    int id=to_id(get(r.params,"employee_id"));std::string day=get(r.params,"day"),status=get(r.params,"status");
    if(!id||!valid_date(day)||(status!="Present"&&status!="Absent"&&status!="Remote"))return error(400,"Invalid attendance details");
    Stmt s("INSERT INTO attendance(employee_id,day,status,marked_by) VALUES(?,?,?,?) ON CONFLICT(employee_id,day) DO UPDATE SET status=excluded.status,marked_by=excluded.marked_by");
    s.bind(1,id);s.bind(2,day);s.bind(3,status);s.bind(4,a.id);s.step();audit(a.id,"attendance_recorded","employee",id);return good("null");
  }
  if(r.path=="/api/leaves"&&r.method=="GET"){
    if(!can_manage(a)){
      Stmt q("SELECT l.id,l.employee_id,e.name AS employee,l.start_date,l.end_date,l.reason,l.status,l.created_at FROM leave_requests l JOIN employees e ON e.id=l.employee_id WHERE l.employee_id=? ORDER BY l.id DESC");q.bind(1,a.employee_id);return good(json_rows(q));
    }
    Stmt q("SELECT l.id,l.employee_id,e.name AS employee,l.start_date,l.end_date,l.reason,l.status,l.created_at FROM leave_requests l JOIN employees e ON e.id=l.employee_id ORDER BY l.id DESC");return good(json_rows(q));
  }
  if(r.path=="/api/leaves"&&r.method=="POST"){
    int id=can_manage(a)?to_id(get(r.params,"employee_id")):a.employee_id;
    std::string start=get(r.params,"start_date"),end=get(r.params,"end_date"),reason=trim(get(r.params,"reason"));
    if(!id||!valid_date(start)||!valid_date(end)||end<start||!nonempty(reason,500))return error(400,"Invalid leave dates, employee or reason");
    Stmt overlap("SELECT COUNT(*) FROM leave_requests WHERE employee_id=? AND status IN ('Pending','Approved') AND start_date<=? AND end_date>=?");
    overlap.bind(1,id);overlap.bind(2,end);overlap.bind(3,start);overlap.step();
    if(sqlite3_column_int(overlap.s,0)>0)return error(409,"An overlapping pending/approved leave request exists");
    Stmt q("INSERT INTO leave_requests(employee_id,start_date,end_date,reason,created_by) VALUES(?,?,?,?,?)");
    q.bind(1,id);q.bind(2,start);q.bind(3,end);q.bind(4,reason);q.bind(5,a.id);q.step();int ref=q.id();audit(a.id,"leave_requested","leave",ref);return good("{\"id\":"+std::to_string(ref)+"}");
  }
  if(route_id(r.path,"/api/leaves/")&&r.method=="PUT"){
    if(!can_manage(a))return error(403,"Only Admin or HR can review leave");
    std::string status=get(r.params,"status");if(status!="Approved"&&status!="Rejected")return error(400,"Status must be Approved or Rejected");
    int id=route_id(r.path,"/api/leaves/");Stmt q("UPDATE leave_requests SET status=?,reviewed_by=? WHERE id=? AND status='Pending'");
    q.bind(1,status);q.bind(2,a.id);q.bind(3,id);q.step();if(sqlite3_changes(db)==0)return error(409,"Leave is missing or already reviewed");
    audit(a.id,"leave_"+lower(status),"leave",id);return good("null");
  }
  if(r.path=="/api/payroll"&&r.method=="GET"){
    if(!can_manage(a))return error(403,"Only Admin or HR can see payroll estimates");
    Stmt q("SELECT id,name,department,annual_salary,ROUND(annual_salary/12,2) AS monthly_estimate FROM employees WHERE status!='Inactive' ORDER BY name");return good(json_rows(q));
  }
  if(r.path=="/api/export/employees"&&r.method=="GET"){
    if(!can_manage(a))return error(403,"Not permitted");
    Stmt q("SELECT id,name,email,department,job_title,annual_salary,joined_on,status FROM employees ORDER BY name");return export_csv(q);
  }
  if(r.path=="/api/export/attendance"&&r.method=="GET"){
    if(!can_manage(a))return error(403,"Not permitted");
    Stmt q("SELECT e.name AS employee,a.day,a.status FROM attendance a JOIN employees e ON e.id=a.employee_id ORDER BY a.day DESC");return export_csv(q);
  }
  if(r.path=="/api/users"&&r.method=="GET"){
    if(a.role!="Admin")return error(403,"Admin access required");
    Stmt q("SELECT id,name,email,role,employee_id FROM users ORDER BY id");return good(json_rows(q));
  }
  if(r.path=="/api/users"&&r.method=="POST"){
    if(a.role!="Admin")return error(403,"Admin access required");
    std::string name=trim(get(r.params,"name")),email=lower(trim(get(r.params,"email"))),role=get(r.params,"role"),pwd=get(r.params,"password");
    int eid=to_id(get(r.params,"employee_id"));
    if(!nonempty(name)||!valid_email(email)||(role!="Admin"&&role!="HR"&&role!="Employee")||pwd.size()<8||pwd.size()>128||(role=="Employee"&&!eid))return error(400,"Valid name, email, role, linked employee and password (8+ characters) are required");
    Stmt q("INSERT INTO users(name,email,pass_hash,role,employee_id) VALUES(?,?,?,?,?)");q.bind(1,name);q.bind(2,email);q.bind(3,password_hash(pwd));q.bind(4,role);
    if(eid)q.bind(5,eid);else sqlite3_bind_null(q.s,5);q.step();int id=q.id();audit(a.id,"user_created","user",id);return good("{\"id\":"+std::to_string(id)+"}");
  }
  if(r.path=="/api/audit"&&r.method=="GET"){
    if(a.role!="Admin")return error(403,"Admin access required");
    Stmt q("SELECT a.id,COALESCE(u.name,'Deleted user') AS actor,a.action,a.entity,a.entity_id,a.created_at FROM audit_log a LEFT JOIN users u ON u.id=a.actor_id ORDER BY a.id DESC LIMIT 100");return good(json_rows(q));
  }
  return error(404,"API endpoint not found");
}
static std::string read_file(const fs::path& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Missing web asset: "+p.string());return {std::istreambuf_iterator<char>(f),{}};}
static Response handle(Request& r){
  if(r.path.rfind("/api/",0)==0){
    // Loopback-only, plus same-origin and anti-CSRF checks for mutations.
    if(r.method!="GET"){
      std::string origin=get(r.headers,"origin");
      if(!origin.empty()&&origin!="http://127.0.0.1:"+std::to_string(server_port)&&origin!="http://localhost:"+std::to_string(server_port))return error(403,"Cross-origin requests are blocked");
    }
    try{return api(r);}catch(const std::exception& e){
      std::string reason=e.what();
      if(reason.find("UNIQUE constraint failed")!=std::string::npos)return error(409,"Email or linked employee already exists");
      if(reason.find("FOREIGN KEY constraint failed")!=std::string::npos)return error(400,"Referenced employee does not exist");
      std::cerr<<"API error: "<<e.what()<<"\n";return error(500,"Internal database error");
    }
  }
  if(r.method!="GET")return error(405,"Method not allowed");
  std::string filename=r.path=="/"?"index.html":r.path=="/app.js"?"app.js":r.path=="/styles.css"?"styles.css":"";
  if(filename.empty())return error(404,"Not found");
  try{Response response;response.content=read_file(fs::path(web_root)/filename);
    response.mime=filename=="index.html"?"text/html; charset=utf-8":filename=="app.js"?"application/javascript; charset=utf-8":"text/css; charset=utf-8";return response;
  }catch(...){return error(500,"Frontend files not found");}
}
static std::string status_text(int code){switch(code){case 200:return "OK";case 400:return "Bad Request";case 401:return "Unauthorized";case 403:return "Forbidden";case 404:return "Not Found";case 405:return "Method Not Allowed";case 409:return "Conflict";case 413:return "Payload Too Large";case 429:return "Too Many Requests";default:return "Internal Server Error";}}
static bool send_all(socket_t client,const std::string& data){size_t pos=0;while(pos<data.size()){
  int n=send(client,data.data()+pos,(int)std::min((size_t)16384,data.size()-pos),0);if(n<=0)return false;pos+=(size_t)n;}return true;}
static void serve(socket_t client){
  constexpr size_t max_request=1024*1024;std::string data;char buf[8192];size_t sep=std::string::npos;
  while((sep=data.find("\r\n\r\n"))==std::string::npos){int n=recv(client,buf,sizeof(buf),0);if(n<=0)return;data.append(buf,(size_t)n);if(data.size()>32768)return;}
  Request r;std::istringstream hs(data.substr(0,sep));std::string request_line;
  if(!std::getline(hs,request_line))return;
  request_line=trim(request_line);
  std::istringstream line(request_line);std::string target,version;line>>r.method>>target>>version;
  if(target.size()>2048||target.empty())return;
  size_t qmark=target.find('?');r.path=target.substr(0,qmark);if(qmark!=std::string::npos)r.params=parse_vars(target.substr(qmark+1));
  std::string header;while(std::getline(hs,header)){auto colon=header.find(':');if(colon!=std::string::npos)r.headers[lower(trim(header.substr(0,colon)))]=trim(header.substr(colon+1));}
  size_t length=0;try{auto cl=get(r.headers,"content-length");if(!cl.empty()){size_t used=0;length=std::stoul(cl,&used);if(used!=cl.size())return;}}catch(...){return;}
  Response response;
  if(length>max_request)response=error(413,"Request too large");
  else{
    r.body=data.substr(sep+4);
    while(r.body.size()<length){int n=recv(client,buf,(int)std::min(sizeof(buf),length-r.body.size()),0);if(n<=0)return;r.body.append(buf,(size_t)n);}
    if(r.body.size()>length)r.body.resize(length);
    if(r.method!="GET" && r.path.rfind("/api/",0)==0){
      if(get(r.headers,"content-type").find("application/x-www-form-urlencoded")!=0)response=error(400,"Expected URL-encoded form body");
      else {auto fields=parse_vars(r.body);r.params.insert(fields.begin(),fields.end());response=handle(r);}
    }else response=handle(r);
  }
  std::ostringstream out;out<<"HTTP/1.1 "<<response.status<<' '<<status_text(response.status)<<"\r\n";
  out<<"Content-Type: "<<response.mime<<"\r\nContent-Length: "<<response.content.size()<<"\r\n";
  out<<"Connection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nX-Frame-Options: DENY\r\nReferrer-Policy: no-referrer\r\n";
  out<<"Content-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; base-uri 'none'; frame-ancestors 'none'\r\n";
  if(!response.cookie.empty())out<<"Set-Cookie: "<<response.cookie<<"\r\n";
  if(response.mime.find("text/csv")!=std::string::npos)out<<"Content-Disposition: attachment; filename=peopleflow-export.csv\r\n";
  out<<"\r\n"<<response.content;send_all(client,out.str());
}
int main(int argc,char** argv){
  try{
    std::string dbpath="data/peopleflow_v2.db";
    for(int i=1;i<argc;i++){
      std::string arg=argv[i];if(arg=="--db"&&i+1<argc)dbpath=argv[++i];
      else if(arg=="--port"&&i+1<argc)server_port=std::stoi(argv[++i]);
      else if(arg=="--web-root"&&i+1<argc)web_root=argv[++i];
      else {std::cerr<<"Unknown argument: "<<arg<<"\n";return 2;}
    }
    if(server_port<1024||server_port>65535)throw std::runtime_error("Choose a port between 1024 and 65535");

#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif
    fs::path path(dbpath);if(path.has_parent_path())fs::create_directories(path.parent_path());
    if(sqlite3_open(dbpath.c_str(),&db)!=SQLITE_OK)throw std::runtime_error("Could not open SQLite database");
    initialize();
#ifdef _WIN32
    WSADATA wsa;if(WSAStartup(MAKEWORD(2,2),&wsa)!=0)throw std::runtime_error("Winsock initialization failed");
#endif
    socket_t listener=socket(AF_INET,SOCK_STREAM,0);if(listener==invalid_socket)throw std::runtime_error("Cannot create socket");
    sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons((unsigned short)server_port);addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    int reuse=1;setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,(char*)&reuse,sizeof(reuse));
    if(bind(listener,(sockaddr*)&addr,sizeof(addr))!=0){close_socket(listener);throw std::runtime_error("Cannot bind localhost port; is PeopleFlow already running?");}
    if(listen(listener,16)!=0){close_socket(listener);throw std::runtime_error("Cannot listen on port");}
    std::cout<<"PeopleFlow HRMS running at http://127.0.0.1:"<<server_port<<"\n"<<"Use Ctrl+C to stop (local demo only)."<<std::endl;
    while(true){sockaddr_in peer{};
#ifdef _WIN32
      int len=sizeof(peer);
#else
      socklen_t len=sizeof(peer);
#endif
      socket_t c=accept(listener,(sockaddr*)&peer,&len);if(c==invalid_socket)continue;
      // Small local learning app: one connection at a time, bounded network read timeout.
#ifdef _WIN32
      DWORD ms=4000;setsockopt(c,SOL_SOCKET,SO_RCVTIMEO,(const char*)&ms,sizeof(ms));
#else
      timeval tv{4,0};setsockopt(c,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
#endif
      serve(c);close_socket(c);
    }
  }catch(const std::exception& e){std::cerr<<"Startup failed: "<<e.what()<<std::endl;if(db)sqlite3_close(db);return 1;}
}
