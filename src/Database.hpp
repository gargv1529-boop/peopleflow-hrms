#pragma once
#include "HRMS.hpp"
#include <sqlite3.h>
#include <filesystem>
#include <string>
#include <vector>
#include <map>

struct User {
    int id{};
    std::string email;
    std::string role; // ADMIN, HR, EMPLOYEE
    std::string name;
    int employeeId{}; // 0 for admin / unlinked HR
};

class Database {
public:
    explicit Database(const std::string& filename, const std::filesystem::path& legacyDirectory);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    const std::vector<Employee>& employees() const { return employees_; }
    const std::vector<Attendance>& attendance() const { return attendance_; }
    const std::vector<LeaveRequest>& leaves() const { return leaves_; }
    const Employee* findEmployee(int id) const;
    std::map<std::string,int> departmentHeadcount(int onlyEmployeeId=0) const;
    double monthlyPayrollEstimate(int onlyEmployeeId=0) const;

    int addEmployee(const std::string&,const std::string&,const std::string&,double);
    void updateEmployee(int,const std::string&,const std::string&,const std::string&,double);
    void removeEmployee(int);
    void markAttendance(int,const std::string&,char);
    int requestLeave(int,const std::string&,const std::string&);
    void reviewLeave(int,bool);

    bool authenticate(const std::string& email,const std::string& password,User& result);
    std::vector<User> users() const;
    User userById(int id) const;
    int addUser(const std::string& email,const std::string& name,const std::string& role,int employeeId,const std::string& password);
    void changePassword(int id,const std::string& oldPassword,const std::string& newPassword);
    void resetPassword(int id,const std::string& newPassword);

private:
    sqlite3* db_{};
    std::vector<Employee> employees_;
    std::vector<Attendance> attendance_;
    std::vector<LeaveRequest> leaves_;
    void exec(const std::string& sql);
    void refresh();
    void migrate(const std::filesystem::path& legacyDirectory);
    void seedUsers();
};
