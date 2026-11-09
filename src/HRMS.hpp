#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct Employee {
    int id{};
    std::string name;
    std::string department;
    std::string email;
    double annualSalary{};
};

struct Attendance {
    int employeeId{};
    std::string date; // ISO 8601, YYYY-MM-DD
    char status{};    // P = present, A = absent
};

struct LeaveRequest {
    int id{};
    int employeeId{};
    std::string date; // one-day leave
    std::string reason;
    char status{'P'}; // P = pending, A = approved, R = rejected
};

class HRMS {
public:
    explicit HRMS(std::filesystem::path dataDirectory);

    int addEmployee(std::string name, std::string department,
                    std::string email, double annualSalary);
    void updateSalary(int employeeId, double annualSalary);
    void updateEmployee(int employeeId, std::string name, std::string department,
                        std::string email, double annualSalary);
    void removeEmployee(int employeeId);
    const Employee* findEmployee(int employeeId) const;
    std::vector<Employee> searchEmployees(const std::string& keyword) const;
    const std::vector<Employee>& employees() const { return employees_; }

    void markAttendance(int employeeId, std::string date, char status);
    std::vector<Attendance> attendanceForMonth(const std::string& month) const;
    const std::vector<Attendance>& attendance() const { return attendance_; }

    int requestLeave(int employeeId, std::string date, std::string reason);
    void reviewLeave(int requestId, bool approve);
    const std::vector<LeaveRequest>& leaves() const { return leaves_; }

    std::map<std::string, int> departmentHeadcount() const;
    double monthlyPayrollEstimate() const;

    static bool validDate(const std::string& date);

private:
    std::filesystem::path directory_;
    std::vector<Employee> employees_;
    std::vector<Attendance> attendance_;
    std::vector<LeaveRequest> leaves_;

    void load();
    void saveEmployees() const;
    void saveAttendance() const;
    void saveLeaves() const;
    void ensureEmployee(int id) const;
};
