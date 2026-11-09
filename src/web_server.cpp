// PeopleFlow HRMS local demo web server. C++17; no external dependencies.
// Binds to 127.0.0.1 only. NOT suitable for deployment on public networks.
#include "Database.hpp"
#include <openssl/rand.h>
#include <chrono>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using Socket = SOCKET;
  constexpr Socket invalidSocket = INVALID_SOCKET;
  void closeSocket(Socket s) { closesocket(s); }
#else
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <unistd.h>
  using Socket = int;
  constexpr Socket invalidSocket = -1;
  void closeSocket(Socket s) { close(s); }
#endif

namespace {
struct Request {
    std::string method, path, body;
    std::map<std::string, std::string> headers;
};
struct Reply {
    int status = 200;
    std::string body;
    std::string mime = "application/json; charset=utf-8";
};
std::string trim(std::string s) {
    const auto a = s.find_first_not_of(" \r\n\t");
    if (a == std::string::npos) return "";
    const auto b = s.find_last_not_of(" \r\n\t");
    return s.substr(a, b - a + 1);
}
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
std::string quote(const std::string& s) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 32) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(c) << std::dec;
                } else out << static_cast<char>(c);
        }
    }
    out << '"';
    return out.str();
}
std::string number(double d) {
    std::ostringstream ss;
    ss << std::setprecision(15) << d;
    return ss.str();
}
std::string decode(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') out += ' ';
        else if (s[i] == '%' && i + 2 < s.size()) {
            const auto hex = s.substr(i + 1, 2);
            char* end = nullptr;
            const long val = std::strtol(hex.c_str(), &end, 16);
            if (end == hex.c_str() + 2) { out += static_cast<char>(val); i += 2; }
            else out += s[i];
        } else out += s[i];
    }
    return out;
}
std::map<std::string, std::string> form(const std::string& body) {
    std::map<std::string, std::string> result;
    std::size_t start = 0;
    while (start <= body.size()) {
        auto end = body.find('&', start);
        if (end == std::string::npos) end = body.size();
        const auto part = body.substr(start, end - start);
        const auto pos = part.find('=');
        if (pos != std::string::npos)
            result[decode(part.substr(0, pos))] = decode(part.substr(pos + 1));
        if (end == body.size()) break;
        start = end + 1;
    }
    return result;
}
std::string required(const std::map<std::string, std::string>& p, const std::string& key) {
    auto it = p.find(key);
    if (it == p.end()) throw std::invalid_argument("Missing field: " + key);
    return it->second;
}
int asInt(const std::map<std::string, std::string>& p, const std::string& key) {
    const auto s = required(p, key);
    std::size_t used = 0;
    const int result = std::stoi(s, &used);
    if (used != s.size()) throw std::invalid_argument("Invalid numeric field: " + key);
    return result;
}
double asMoney(const std::map<std::string, std::string>& p) {
    const auto s = required(p, "salary");
    std::size_t used = 0;
    const double d = std::stod(s, &used);
    if (used != s.size() || !std::isfinite(d)) throw std::invalid_argument("Invalid salary.");
    return d;
}
std::string employeeJson(const Employee& e) {
    return "{\"id\":" + std::to_string(e.id) + ",\"name\":" + quote(e.name) +
           ",\"department\":" + quote(e.department) + ",\"email\":" + quote(e.email) +
           ",\"salary\":" + number(e.annualSalary) + "}";
}
std::string leaveJson(const LeaveRequest& l, const Database& hrms) {
    const auto* e = hrms.findEmployee(l.employeeId);
    return "{\"id\":" + std::to_string(l.id) + ",\"employeeId\":" +
           std::to_string(l.employeeId) + ",\"employee\":" + quote(e ? e->name : "Unknown") +
           ",\"date\":" + quote(l.date) + ",\"reason\":" + quote(l.reason) +
           ",\"status\":" + quote(std::string(1, l.status)) + "}";
}
std::string attendanceJson(const Attendance& a, const Database& hrms) {
    const auto* e = hrms.findEmployee(a.employeeId);
    return "{\"employeeId\":" + std::to_string(a.employeeId) + ",\"employee\":" +
           quote(e ? e->name : "Unknown") + ",\"date\":" + quote(a.date) +
           ",\"status\":" + quote(std::string(1, a.status)) + "}";
}
template <typename T, typename F>
std::string jsonArray(const T& collection, F fn) {
    std::string out = "[";
    bool first = true;
    for (const auto& row : collection) {
        if (!first) out += ',';
        out += fn(row);
        first = false;
    }
    return out + "]";
}
std::string success(std::string msg = "Saved successfully") {
    return "{\"ok\":true,\"message\":" + quote(msg) + "}";
}
std::string token() {
    unsigned char bytes[32];
    if (RAND_bytes(bytes, 32) != 1) throw std::runtime_error("Random generator failed");
    std::ostringstream out;
    for (unsigned char b : bytes) out << std::hex << std::setw(2) << std::setfill('0') << int(b);
    return out.str();
}
std::string fileText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Missing web asset: " + path +
        ". Run the project from its main folder.");
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
bool receiveRequest(Socket client, Request& req) {
    std::string raw;
    std::array<char, 8192> chunk{};
    std::size_t split = std::string::npos;
    while ((split = raw.find("\r\n\r\n")) == std::string::npos) {
        const int n = recv(client, chunk.data(), static_cast<int>(chunk.size()), 0);
        if (n <= 0) return false;
        raw.append(chunk.data(), n);
        if (raw.size() > 1048576) return false;
    }
    std::istringstream headers(raw.substr(0, split));
    std::string line, version, requestTarget;
    if (!std::getline(headers, line)) return false;
    {
        std::istringstream first(trim(line));
        if (!(first >> req.method >> requestTarget >> version)) return false;
    }
    if (requestTarget.empty() || requestTarget[0] != '/') return false;
    req.path = requestTarget.substr(0, requestTarget.find('?'));
    while (std::getline(headers, line)) {
        auto sep = line.find(':');
        if (sep != std::string::npos)
            req.headers[lower(trim(line.substr(0, sep)))] = trim(line.substr(sep + 1));
    }
    std::size_t length = 0;
    const auto it = req.headers.find("content-length");
    if (it != req.headers.end()) {
        try { length = static_cast<std::size_t>(std::stoul(it->second)); }
        catch (...) { return false; }
    }
    if (length > 262144) return false;
    req.body = raw.substr(split + 4);
    while (req.body.size() < length) {
        const int n = recv(client, chunk.data(), static_cast<int>(chunk.size()), 0);
        if (n <= 0) return false;
        req.body.append(chunk.data(), n);
    }
    req.body.resize(length);
    return true;
}
void sendReply(Socket client, const Reply& reply) {
    const std::string phrase = reply.status == 200 ? "OK" : reply.status == 201 ? "Created" :
        reply.status == 400 ? "Bad Request" : reply.status == 401 ? "Unauthorized" :
        reply.status == 403 ? "Forbidden" : reply.status == 404 ? "Not Found" : reply.status == 405 ? "Method Not Allowed" :
        "Internal Server Error";
    const std::string header = "HTTP/1.1 " + std::to_string(reply.status) + " " + phrase +
        "\r\nContent-Type: " + reply.mime + "\r\nContent-Length: " +
        std::to_string(reply.body.size()) +
        "\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff"
        "\r\nX-Frame-Options: DENY\r\nConnection: close\r\n\r\n";
    const std::string all = header + reply.body;
    std::size_t sent = 0;
    while (sent < all.size()) {
        const int n = send(client, all.data() + sent,
            static_cast<int>(std::min<std::size_t>(all.size() - sent, 65536)), 0);
        if (n <= 0) break;
        sent += static_cast<std::size_t>(n);
    }
}
std::string userJson(const User& u) {
    return "{\"id\":" + std::to_string(u.id) + ",\"email\":" + quote(u.email) +
        ",\"name\":" + quote(u.name) + ",\"role\":" + quote(u.role) +
        ",\"employeeId\":" + std::to_string(u.employeeId) + "}";
}
class Server {
    Database hrms_{"data/peopleflow.db", "data"};
    struct Session { User user; std::chrono::steady_clock::time_point expiry; };
    std::map<std::string,Session> sessions_;
    static bool manager(const User& u) {return u.role=="ADMIN" || u.role=="HR";}
    static void requireManager(const User& u) {if(!manager(u))throw std::runtime_error("FORBIDDEN");}
    static void requireAdmin(const User& u) {if(u.role!="ADMIN")throw std::runtime_error("FORBIDDEN");}
    void invalidate(int id) {
        for(auto it=sessions_.begin();it!=sessions_.end();) {
            if(it->second.user.id==id)it=sessions_.erase(it);else ++it;
        }
    }
public:
    Reply handle(const Request& r) {
        if (r.method == "GET" && (r.path == "/" || r.path == "/index.html"))
            return {200, fileText("web/index.html"), "text/html; charset=utf-8"};
        if (r.method == "GET" && r.path == "/styles.css")
            return {200, fileText("web/styles.css"), "text/css; charset=utf-8"};
        if (r.method == "GET" && r.path == "/app.js")
            return {200, fileText("web/app.js"), "text/javascript; charset=utf-8"};
        if (r.method == "GET" && r.path == "/api/health") return {200, success("PeopleFlow running")};
        if (r.method == "POST" && r.path == "/api/login") {
            const auto p=form(r.body); User u;
            if (!hrms_.authenticate(required(p,"email"),required(p,"password"),u))
                return {401, "{\"error\":\"Incorrect email or password.\"}"};
            const std::string id=token();
            sessions_[id]={u,std::chrono::steady_clock::now()+std::chrono::hours(8)};
            return {200,"{\"ok\":true,\"token\":"+quote(id)+",\"user\":"+userJson(u)+"}"};
        }
        if (r.path.rfind("/api/",0)!=0) return {404,"{\"error\":\"Not found\"}"};
        auto sit=r.headers.find("x-session-token");
        if(sit==r.headers.end())return {401,"{\"error\":\"Please sign in.\"}"};
        auto session=sessions_.find(sit->second);
        if(session==sessions_.end() || session->second.expiry<std::chrono::steady_clock::now()) {
            if(session!=sessions_.end())sessions_.erase(session);
            return {401,"{\"error\":\"Session expired. Sign in again.\"}"};
        }
        const User user=session->second.user;
        if(user.role=="EMPLOYEE" && !hrms_.findEmployee(user.employeeId)) {
            sessions_.erase(session);
            return {401,"{\"error\":\"Linked employee record no longer exists.\"}"};
        }
        if(r.method=="POST" && r.path=="/api/logout"){
            sessions_.erase(session);return {200,success("Signed out")};
        }
        if(r.method=="GET" && r.path=="/api/me")return {200,userJson(user)};
        if(r.method=="GET" && r.path=="/api/users"){
            requireAdmin(user);
            return {200,jsonArray(hrms_.users(),userJson)};
        }
        if (r.method == "GET" && r.path == "/api/overview") {
            int pending = 0, approved = 0, present = 0, absent = 0;
            for (const auto& l : hrms_.leaves()) {
                if(!manager(user) && l.employeeId!=user.employeeId)continue;
                if (l.status == 'P') ++pending;
                if (l.status == 'A') ++approved;
            }
            for (const auto& a : hrms_.attendance()) {
                if(!manager(user) && a.employeeId!=user.employeeId)continue;
                if (a.status == 'P') ++present;
                if (a.status == 'A') ++absent;
            }
            std::string deps = "[";
            bool first = true;
            for (const auto& [dep, count] : hrms_.departmentHeadcount(manager(user)?0:user.employeeId)) {
                if (!first) deps += ',';
                deps += "{\"name\":" + quote(dep) + ",\"count\":" + std::to_string(count) + "}";
                first = false;
            }
            deps += "]";
            const std::string result = "{\"employees\":" + std::to_string(manager(user)?hrms_.employees().size():1) +
                ",\"departments\":" + std::to_string(hrms_.departmentHeadcount(manager(user)?0:user.employeeId).size()) +
                ",\"payroll\":" + number(manager(user)?hrms_.monthlyPayrollEstimate():0) +
                ",\"pending\":" + std::to_string(pending) +
                ",\"approved\":" + std::to_string(approved) +
                ",\"present\":" + std::to_string(present) +
                ",\"absent\":" + std::to_string(absent) + ",\"departmentCounts\":" + deps + "}";
            return {200, result};
        }
        if (r.method == "GET" && r.path == "/api/employees") {
            std::vector<Employee> filtered;
            for(const auto& e:hrms_.employees())if(manager(user)||e.id==user.employeeId)filtered.push_back(e);
            return {200,jsonArray(filtered,employeeJson)};
        }
        if (r.method == "GET" && r.path == "/api/leaves") {
            std::vector<LeaveRequest> filtered;
            for(const auto& l:hrms_.leaves())if(manager(user)||l.employeeId==user.employeeId)filtered.push_back(l);
            return {200,jsonArray(filtered,[&](const LeaveRequest& l){return leaveJson(l,hrms_);})};
        }
        if (r.method == "GET" && r.path == "/api/attendance") {
            std::vector<Attendance> filtered;
            for(const auto& a:hrms_.attendance())if(manager(user)||a.employeeId==user.employeeId)filtered.push_back(a);
            return {200,jsonArray(filtered,[&](const Attendance& a){return attendanceJson(a,hrms_);})};
        }
        if (r.method != "POST") return {405, "{\"error\":\"Use POST for this action\"}"};
        const auto p = form(r.body);
        if (r.path == "/api/users/add") {
            requireAdmin(user);
            const auto role=required(p,"role");
            if(role!="HR" && role!="EMPLOYEE")throw std::invalid_argument("New accounts must use HR or EMPLOYEE role");
            int id=hrms_.addUser(required(p,"email"),required(p,"name"),role,role=="EMPLOYEE"?asInt(p,"employeeId"):0,required(p,"password"));
            return {201,"{\"ok\":true,\"id\":"+std::to_string(id)+"}"};
        }
        if(r.path=="/api/users/change-password"){
            hrms_.changePassword(user.id,required(p,"oldPassword"),required(p,"newPassword"));
            invalidate(user.id);
            return {200,success("Password changed. Please sign in again.")};
        }
        if(r.path=="/api/users/reset-password"){
            requireAdmin(user);int target=asInt(p,"id");
            if(target==user.id)throw std::invalid_argument("Use Change password for your own account.");
            hrms_.resetPassword(target,required(p,"newPassword"));invalidate(target);
            return {200,success("Account password reset")};
        }
        if (r.path == "/api/employees/add") {
            requireManager(user);
            int id = hrms_.addEmployee(required(p, "name"), required(p, "department"),
                                       required(p, "email"), asMoney(p));
            return {201, "{\"ok\":true,\"id\":" + std::to_string(id) + "}"};
        }
        if (r.path == "/api/employees/update") {
            requireManager(user);
            hrms_.updateEmployee(asInt(p, "id"), required(p, "name"),
                  required(p, "department"), required(p, "email"), asMoney(p));
            return {200, success("Employee updated")};
        }
        if (r.path == "/api/employees/delete") {
            requireManager(user);
            hrms_.removeEmployee(asInt(p, "id"));
            return {200, success("Employee deleted")};
        }
        if (r.path == "/api/attendance/add") {
            requireManager(user);
            auto value = required(p, "status");
            if (value.size() != 1) throw std::invalid_argument("Invalid attendance status.");
            hrms_.markAttendance(asInt(p, "employeeId"), required(p, "date"), value[0]);
            return {200, success("Attendance saved")};
        }
        if (r.path == "/api/leaves/add") {
            const int employeeId=manager(user)?asInt(p,"employeeId"):user.employeeId;
            const int id = hrms_.requestLeave(employeeId, required(p, "date"), required(p, "reason"));
            return {201, "{\"ok\":true,\"id\":" + std::to_string(id) + "}"};
        }
        if (r.path == "/api/leaves/review") {
            requireManager(user);
            const auto action = required(p, "action");
            if (action != "approve" && action != "reject")
                throw std::invalid_argument("Action must be approve or reject.");
            hrms_.reviewLeave(asInt(p, "id"), action == "approve");
            return {200, success("Leave reviewed")};
        }
        return {404, "{\"error\":\"Endpoint not found\"}"};
    }
};
} // namespace

int main() {
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        std::cerr << "Cannot initialize Windows networking.\n";
        return 1;
    }
#endif
    try {
        Server server;
        Socket socketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socketFd == invalidSocket) throw std::runtime_error("Cannot create socket.");
#ifndef _WIN32
        int enable = 1;
        setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
#endif
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(8765);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(socketFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
            listen(socketFd, 16) != 0) {
            closeSocket(socketFd);
            throw std::runtime_error("Cannot start server. Is port 8765 already in use?");
        }
        std::cout << "\n  PeopleFlow Pro | C++ dashboard running!\n"
                  << "  Open: http://127.0.0.1:8765\n"
                  << "  Admin: admin@peopleflow.demo / Demo@123\n"
                  << "  HR: hr@peopleflow.demo / Demo@123\n"
                  << "  Employee: employee@peopleflow.demo / Demo@123 (if imported employees exist)\n"
                  << "  SQLite database: data/peopleflow.db\n"
                  << "  Local demo only. Press Ctrl+C to stop.\n\n" << std::flush;
        for (;;) {
            Socket client = accept(socketFd, nullptr, nullptr);
            if (client == invalidSocket) continue;
            Request req;
            if (receiveRequest(client, req)) {
                try {
                    const auto origin=req.headers.find("origin");
                    if(origin!=req.headers.end() && origin->second!="http://127.0.0.1:8765" && origin->second!="http://localhost:8765") {
                        sendReply(client,{403,"{\"error\":\"Wrong origin\"}"});
                    } else sendReply(client, server.handle(req));
                } catch (const std::invalid_argument& e) {
                    sendReply(client, {400, "{\"error\":" + quote(e.what()) + "}"});
                } catch (const std::exception& e) {
                    if (std::string(e.what())=="FORBIDDEN") {
                        sendReply(client,{403,"{\"error\":\"This role does not have permission.\"}"});
                        closeSocket(client);continue;
                    }
                    std::cerr << "Request error: " << e.what() << '\n';
                    sendReply(client, {500, "{\"error\":\"Internal server error\"}"});
                }
            }
            closeSocket(client);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }
#ifdef _WIN32
    WSACleanup();
#endif
    return 1;
}
