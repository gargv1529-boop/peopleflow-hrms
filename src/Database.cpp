#include "Database.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {
struct Statement {
    sqlite3_stmt* stmt{};
    Statement(sqlite3* db,const char* sql) {
        if(sqlite3_prepare_v2(db,sql,-1,&stmt,nullptr)!=SQLITE_OK)
            throw std::runtime_error(sqlite3_errmsg(db));
    }
    ~Statement(){ sqlite3_finalize(stmt); }
    Statement(const Statement&)=delete;
    Statement& operator=(const Statement&)=delete;
    void bind(int i,int n){sqlite3_bind_int(stmt,i,n);}
    void bind(int i,double d){sqlite3_bind_double(stmt,i,d);}
    void bind(int i,const std::string& s){sqlite3_bind_text(stmt,i,s.c_str(),-1,SQLITE_TRANSIENT);}
    bool next(){return sqlite3_step(stmt)==SQLITE_ROW;}
    void run(){const int rc=sqlite3_step(stmt);if(rc!=SQLITE_DONE) throw std::invalid_argument(sqlite3_errmsg(sqlite3_db_handle(stmt)));}
};
std::string stringColumn(sqlite3_stmt* st,int i){auto p=sqlite3_column_text(st,i);return p?reinterpret_cast<const char*>(p):"";}
std::string cleanEmail(std::string s){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return char(std::tolower(c));});return s;}
void checkText(const std::string& s,const std::string& label,int max=150){if(s.find_first_not_of(" \t\r\n")==std::string::npos || s.size()>size_t(max)) throw std::invalid_argument(label+" is required (max "+std::to_string(max)+" characters).");}
void checkEmployee(const std::string& name,const std::string& dep,const std::string& email,double salary){
    checkText(name,"Name",100);checkText(dep,"Department",80);checkText(email,"Email",120);
    if(email.find('@')==std::string::npos)throw std::invalid_argument("Enter a valid email address.");
    if(!std::isfinite(salary)||salary<0||salary>1e12)throw std::invalid_argument("Invalid annual salary.");
}
std::string randBytesHex(int size){
    std::string bytes(size,'\0');if(RAND_bytes(reinterpret_cast<unsigned char*>(&bytes[0]),size)!=1)throw std::runtime_error("Cryptographic RNG failed");
    std::ostringstream out;for(unsigned char c:bytes)out<<std::hex<<std::setw(2)<<std::setfill('0')<<int(c);
    return out.str();
}
std::string passwordHash(const std::string& pass,const std::string& hexSalt){
    std::string salt;for(size_t i=0;i<hexSalt.size();i+=2) salt.push_back(char(std::stoi(hexSalt.substr(i,2),nullptr,16)));
    unsigned char result[32];
    if(PKCS5_PBKDF2_HMAC(pass.c_str(),static_cast<int>(pass.size()),reinterpret_cast<const unsigned char*>(salt.data()),static_cast<int>(salt.size()),200000,EVP_sha256(),32,result)!=1)
        throw std::runtime_error("Password hashing failed");
    std::ostringstream out;for(unsigned char c:result)out<<std::hex<<std::setw(2)<<std::setfill('0')<<int(c);
    return out.str();
}
void checkPassword(const std::string& password){if(password.size()<8||password.size()>128)throw std::invalid_argument("Password must have 8 to 128 characters.");}
}

Database::Database(const std::string& filename,const std::filesystem::path& legacyDirectory){
    std::filesystem::create_directories(std::filesystem::path(filename).parent_path());
    if(sqlite3_open(filename.c_str(),&db_)!=SQLITE_OK){std::string err=sqlite3_errmsg(db_);sqlite3_close(db_);db_=nullptr;throw std::runtime_error("Cannot open database: "+err);}
    try{
        exec("PRAGMA foreign_keys=ON;");exec("PRAGMA busy_timeout=3000;");
        exec("CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY,value TEXT NOT NULL);");
        exec("CREATE TABLE IF NOT EXISTS employees(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,department TEXT NOT NULL,email TEXT NOT NULL COLLATE NOCASE UNIQUE,salary REAL NOT NULL CHECK(salary>=0));");
        exec("CREATE TABLE IF NOT EXISTS attendance(employee_id INTEGER NOT NULL REFERENCES employees(id) ON DELETE CASCADE,date TEXT NOT NULL,status TEXT NOT NULL CHECK(status IN ('P','A')),PRIMARY KEY(employee_id,date));");
        exec("CREATE TABLE IF NOT EXISTS leaves(id INTEGER PRIMARY KEY AUTOINCREMENT,employee_id INTEGER NOT NULL REFERENCES employees(id) ON DELETE CASCADE,date TEXT NOT NULL,reason TEXT NOT NULL,status TEXT NOT NULL CHECK(status IN ('P','A','R')) DEFAULT 'P');");
        exec("CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY AUTOINCREMENT,email TEXT NOT NULL COLLATE NOCASE UNIQUE,name TEXT NOT NULL,role TEXT NOT NULL CHECK(role IN ('ADMIN','HR','EMPLOYEE')),employee_id INTEGER REFERENCES employees(id) ON DELETE SET NULL,salt TEXT NOT NULL,hash TEXT NOT NULL);");
        migrate(legacyDirectory);refresh();seedUsers();
    }catch(...){sqlite3_close(db_);db_=nullptr;throw;}
}
Database::~Database(){if(db_)sqlite3_close(db_);}
void Database::exec(const std::string& sql){char* errmsg=nullptr;int rc=sqlite3_exec(db_,sql.c_str(),nullptr,nullptr,&errmsg);if(rc!=SQLITE_OK){std::string msg=errmsg?errmsg:"SQLite error";sqlite3_free(errmsg);throw std::runtime_error(msg);}}
void Database::migrate(const std::filesystem::path& directory){
    Statement flag(db_,"SELECT value FROM settings WHERE key='legacy_imported'");if(flag.next())return;
    // First launch only, in a transaction: preserve employee and leave IDs.
    HRMS legacy(directory);
    exec("BEGIN IMMEDIATE");
    try{
        Statement e(db_,"INSERT OR IGNORE INTO employees(id,name,department,email,salary) VALUES(?,?,?,?,?)");
        for(auto& row:legacy.employees()){
            sqlite3_reset(e.stmt);sqlite3_clear_bindings(e.stmt);
            e.bind(1,row.id);e.bind(2,row.name);e.bind(3,row.department);e.bind(4,row.email);e.bind(5,row.annualSalary);e.run();
        }
        Statement a(db_,"INSERT OR IGNORE INTO attendance(employee_id,date,status) VALUES(?,?,?)");
        for(auto& row:legacy.attendance()){
            sqlite3_reset(a.stmt);sqlite3_clear_bindings(a.stmt);
            a.bind(1,row.employeeId);a.bind(2,row.date);a.bind(3,std::string(1,row.status));a.run();
        }
        Statement l(db_,"INSERT OR IGNORE INTO leaves(id,employee_id,date,reason,status) VALUES(?,?,?,?,?)");
        for(auto& row:legacy.leaves()){
            sqlite3_reset(l.stmt);sqlite3_clear_bindings(l.stmt);
            l.bind(1,row.id);l.bind(2,row.employeeId);l.bind(3,row.date);l.bind(4,row.reason);l.bind(5,std::string(1,row.status));l.run();
        }
        exec("INSERT INTO settings(key,value) VALUES('legacy_imported','1');");exec("COMMIT");
    }catch(...){exec("ROLLBACK");throw;}
}
void Database::refresh(){
    employees_.clear();attendance_.clear();leaves_.clear();
    Statement e(db_,"SELECT id,name,department,email,salary FROM employees ORDER BY id");
    while(e.next()) employees_.push_back({sqlite3_column_int(e.stmt,0),stringColumn(e.stmt,1),stringColumn(e.stmt,2),stringColumn(e.stmt,3),sqlite3_column_double(e.stmt,4)});
    Statement a(db_,"SELECT employee_id,date,status FROM attendance ORDER BY date DESC");
    while(a.next())attendance_.push_back({sqlite3_column_int(a.stmt,0),stringColumn(a.stmt,1),stringColumn(a.stmt,2)[0]});
    Statement l(db_,"SELECT id,employee_id,date,reason,status FROM leaves ORDER BY id DESC");
    while(l.next())leaves_.push_back({sqlite3_column_int(l.stmt,0),sqlite3_column_int(l.stmt,1),stringColumn(l.stmt,2),stringColumn(l.stmt,3),stringColumn(l.stmt,4)[0]});
}
const Employee* Database::findEmployee(int id)const{for(auto& e:employees_)if(e.id==id)return &e;return nullptr;}
std::map<std::string,int> Database::departmentHeadcount(int onlyId)const{std::map<std::string,int> m;for(auto& e:employees_)if(!onlyId||e.id==onlyId)++m[e.department];return m;}
double Database::monthlyPayrollEstimate(int onlyId)const{double n=0;for(auto& e:employees_)if(!onlyId||e.id==onlyId)n+=e.annualSalary;return n/12;}
int Database::addEmployee(const std::string& name,const std::string& dep,const std::string& email,double salary){
    checkEmployee(name,dep,email,salary);Statement q(db_,"INSERT INTO employees(name,department,email,salary) VALUES(?,?,?,?)");q.bind(1,name);q.bind(2,dep);q.bind(3,email);q.bind(4,salary);q.run();int id=int(sqlite3_last_insert_rowid(db_));refresh();return id;
}
void Database::updateEmployee(int id,const std::string& name,const std::string& dep,const std::string& email,double salary){
    if(!findEmployee(id))throw std::invalid_argument("Employee not found");
    checkEmployee(name,dep,email,salary);
    Statement q(db_,"UPDATE employees SET name=?,department=?,email=?,salary=? WHERE id=?");q.bind(1,name);q.bind(2,dep);q.bind(3,email);q.bind(4,salary);q.bind(5,id);q.run();refresh();
}
void Database::removeEmployee(int id){if(!findEmployee(id))throw std::invalid_argument("Employee not found");Statement q(db_,"DELETE FROM employees WHERE id=?");q.bind(1,id);q.run();refresh();}
void Database::markAttendance(int id,const std::string& date,char status){
    if(!findEmployee(id))throw std::invalid_argument("Employee not found");
    if(!HRMS::validDate(date)|| (status!='P'&&status!='A'))throw std::invalid_argument("Invalid date or attendance status");
    Statement q(db_,"INSERT INTO attendance(employee_id,date,status) VALUES(?,?,?) ON CONFLICT(employee_id,date) DO UPDATE SET status=excluded.status");q.bind(1,id);q.bind(2,date);q.bind(3,std::string(1,status));q.run();refresh();
}
int Database::requestLeave(int id,const std::string& date,const std::string& reason){
    if(!findEmployee(id))throw std::invalid_argument("Employee not found");
    if(!HRMS::validDate(date))throw std::invalid_argument("Enter valid leave date");
    checkText(reason,"Reason",200);
    Statement q(db_,"INSERT INTO leaves(employee_id,date,reason,status) VALUES(?,?,?,'P')");q.bind(1,id);q.bind(2,date);q.bind(3,reason);q.run();int result=int(sqlite3_last_insert_rowid(db_));refresh();return result;
}
void Database::reviewLeave(int id,bool approve){
    Statement q(db_,"UPDATE leaves SET status=? WHERE id=? AND status='P'");q.bind(1,std::string(1,approve?'A':'R'));q.bind(2,id);q.run();if(sqlite3_changes(db_)!=1)throw std::invalid_argument("Pending leave not found");refresh();
}
std::vector<User> Database::users()const{
    Statement q(db_,"SELECT id,email,role,name,COALESCE(employee_id,0) FROM users ORDER BY id");std::vector<User> out;
    while(q.next())out.push_back({sqlite3_column_int(q.stmt,0),stringColumn(q.stmt,1),stringColumn(q.stmt,2),stringColumn(q.stmt,3),sqlite3_column_int(q.stmt,4)});
    return out;
}
User Database::userById(int id)const{for(auto& u:users())if(u.id==id)return u;throw std::invalid_argument("Account not found");}
int Database::addUser(const std::string& email,const std::string& name,const std::string& role,int employeeId,const std::string& password){
    checkText(email,"Account email",120);checkText(name,"Account name",100);checkPassword(password);
    if(email.find('@')==std::string::npos)throw std::invalid_argument("Enter valid account email");
    if(role!="ADMIN"&&role!="HR"&&role!="EMPLOYEE")throw std::invalid_argument("Invalid role");
    if(role=="EMPLOYEE"&&!findEmployee(employeeId))throw std::invalid_argument("Employee account must be linked to an existing employee");
    if(employeeId!=0&&!findEmployee(employeeId))throw std::invalid_argument("Linked employee does not exist");
    const auto salt=randBytesHex(16),hash=passwordHash(password,salt);
    Statement q(db_,"INSERT INTO users(email,name,role,employee_id,salt,hash) VALUES(?,?,?,?,?,?)");
    q.bind(1,cleanEmail(email));q.bind(2,name);q.bind(3,role);
    if(employeeId==0)sqlite3_bind_null(q.stmt,4);else q.bind(4,employeeId);
    q.bind(5,salt);q.bind(6,hash);q.run();return int(sqlite3_last_insert_rowid(db_));
}
bool Database::authenticate(const std::string& email,const std::string& pass,User& result){
    Statement q(db_,"SELECT id,email,role,name,COALESCE(employee_id,0),salt,hash FROM users WHERE email=?");q.bind(1,cleanEmail(email));
    if(!q.next())return false;
    const std::string stored=stringColumn(q.stmt,6), actual=passwordHash(pass,stringColumn(q.stmt,5));
    if(stored.size()!=actual.size()||CRYPTO_memcmp(stored.data(),actual.data(),stored.size())!=0)return false;
    result={sqlite3_column_int(q.stmt,0),stringColumn(q.stmt,1),stringColumn(q.stmt,2),stringColumn(q.stmt,3),sqlite3_column_int(q.stmt,4)};
    if(result.role=="EMPLOYEE"&&!findEmployee(result.employeeId))return false;
    return true;
}
void Database::resetPassword(int id,const std::string& pass){
    checkPassword(pass);userById(id);const std::string salt=randBytesHex(16),hash=passwordHash(pass,salt);
    Statement q(db_,"UPDATE users SET salt=?,hash=? WHERE id=?");q.bind(1,salt);q.bind(2,hash);q.bind(3,id);q.run();
}
void Database::changePassword(int id,const std::string& old,const std::string& replacement){
    const auto user=userById(id);User matched;
    if(!authenticate(user.email,old,matched))throw std::invalid_argument("Current password is incorrect");
    resetPassword(id,replacement);
}
void Database::seedUsers(){
    Statement count(db_,"SELECT COUNT(*) FROM users");if(!count.next()||sqlite3_column_int(count.stmt,0)>0)return;
    addUser("admin@peopleflow.demo","Admin User","ADMIN",0,"Demo@123");
    addUser("hr@peopleflow.demo","HR Manager","HR",0,"Demo@123");
    if(!employees_.empty())addUser("employee@peopleflow.demo","Employee Demo","EMPLOYEE",employees_[0].id,"Demo@123");
}
