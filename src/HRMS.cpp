#include "HRMS.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace fs = std::filesystem;

namespace {
std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

void requireNonEmpty(const std::string& value, const std::string& label) {
    if (value.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::invalid_argument(label + " cannot be empty.");
}

std::ofstream openForWrite(const fs::path& filename) {
    std::ofstream file(filename, std::ios::trunc);
    if (!file) throw std::runtime_error("Cannot write file: " + filename.string());
    return file;
}
} // namespace

HRMS::HRMS(fs::path dataDirectory) : directory_(std::move(dataDirectory)) {
    fs::create_directories(directory_);
    load();
}

bool HRMS::validDate(const std::string& date) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') return false;
    for (std::size_t i = 0; i < date.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(date[i]))) return false;
    }
    const int year = std::stoi(date.substr(0, 4));
    const int month = std::stoi(date.substr(5, 2));
    const int day = std::stoi(date.substr(8, 2));
    if (year < 1900 || month < 1 || month > 12 || day < 1) return false;
    const bool leap = year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
    const int days[] = {31, leap ? 29 : 28, 31, 30, 31, 30,
                        31, 31, 30, 31, 30, 31};
    return day <= days[month - 1];
}

void HRMS::load() {
    {
        std::ifstream file(directory_ / "employees.txt");
        Employee e;
        while (file >> e.id >> std::quoted(e.name) >> std::quoted(e.department)
                    >> std::quoted(e.email) >> e.annualSalary)
            employees_.push_back(e);
    }
    {
        std::ifstream file(directory_ / "attendance.txt");
        Attendance a;
        while (file >> a.employeeId >> a.date >> a.status) attendance_.push_back(a);
    }
    {
        std::ifstream file(directory_ / "leaves.txt");
        LeaveRequest leave;
        while (file >> leave.id >> leave.employeeId >> leave.date
                    >> std::quoted(leave.reason) >> leave.status)
            leaves_.push_back(leave);
    }
}

void HRMS::saveEmployees() const {
    auto file = openForWrite(directory_ / "employees.txt");
    file << std::setprecision(17);
    for (const auto& e : employees_)
        file << e.id << ' ' << std::quoted(e.name) << ' '
             << std::quoted(e.department) << ' ' << std::quoted(e.email)
             << ' ' << e.annualSalary << '\n';
}

void HRMS::saveAttendance() const {
    auto file = openForWrite(directory_ / "attendance.txt");
    for (const auto& a : attendance_)
        file << a.employeeId << ' ' << a.date << ' ' << a.status << '\n';
}

void HRMS::saveLeaves() const {
    auto file = openForWrite(directory_ / "leaves.txt");
    for (const auto& l : leaves_)
        file << l.id << ' ' << l.employeeId << ' ' << l.date << ' '
             << std::quoted(l.reason) << ' ' << l.status << '\n';
}

const Employee* HRMS::findEmployee(int employeeId) const {
    const auto it = std::find_if(employees_.begin(), employees_.end(),
                                 [employeeId](const Employee& e) { return e.id == employeeId; });
    return it == employees_.end() ? nullptr : &*it;
}

void HRMS::ensureEmployee(int id) const {
    if (!findEmployee(id)) throw std::invalid_argument("Employee ID not found.");
}

int HRMS::addEmployee(std::string name, std::string department,
                      std::string email, double annualSalary) {
    requireNonEmpty(name, "Name");
    requireNonEmpty(department, "Department");
    requireNonEmpty(email, "Email");
    if (email.find('@') == std::string::npos)
        throw std::invalid_argument("Enter a valid email address.");
    if (annualSalary < 0 || annualSalary > 1e12)
        throw std::invalid_argument("Annual salary must be between 0 and 1 trillion.");
    const auto duplicate = std::find_if(employees_.begin(), employees_.end(),
        [&](const Employee& e) { return lowercase(e.email) == lowercase(email); });
    if (duplicate != employees_.end()) throw std::invalid_argument("Email already exists.");

    int id = 1;
    for (const auto& e : employees_) id = std::max(id, e.id + 1);
    employees_.push_back({id, std::move(name), std::move(department),
                          std::move(email), annualSalary});
    saveEmployees();
    return id;
}

void HRMS::updateEmployee(int employeeId, std::string name, std::string department,
                          std::string email, double annualSalary) {
    ensureEmployee(employeeId);
    requireNonEmpty(name, "Name");
    requireNonEmpty(department, "Department");
    requireNonEmpty(email, "Email");
    if (email.find('@') == std::string::npos)
        throw std::invalid_argument("Enter a valid email address.");
    if (annualSalary < 0 || annualSalary > 1e12)
        throw std::invalid_argument("Invalid annual salary.");
    const auto duplicate = std::find_if(employees_.begin(), employees_.end(),
        [&](const Employee& e) {
            return e.id != employeeId && lowercase(e.email) == lowercase(email);
        });
    if (duplicate != employees_.end()) throw std::invalid_argument("Email already exists.");
    for (auto& e : employees_) {
        if (e.id == employeeId) {
            e.name = std::move(name);
            e.department = std::move(department);
            e.email = std::move(email);
            e.annualSalary = annualSalary;
            break;
        }
    }
    saveEmployees();
}

void HRMS::updateSalary(int employeeId, double annualSalary) {
    ensureEmployee(employeeId);
    if (annualSalary < 0 || annualSalary > 1e12)
        throw std::invalid_argument("Invalid annual salary.");
    for (auto& e : employees_)
        if (e.id == employeeId) e.annualSalary = annualSalary;
    saveEmployees();
}

void HRMS::removeEmployee(int employeeId) {
    ensureEmployee(employeeId);
    employees_.erase(std::remove_if(employees_.begin(), employees_.end(),
        [employeeId](const Employee& e) { return e.id == employeeId; }), employees_.end());
    attendance_.erase(std::remove_if(attendance_.begin(), attendance_.end(),
        [employeeId](const Attendance& a) { return a.employeeId == employeeId; }), attendance_.end());
    leaves_.erase(std::remove_if(leaves_.begin(), leaves_.end(),
        [employeeId](const LeaveRequest& l) { return l.employeeId == employeeId; }), leaves_.end());
    saveEmployees();
    saveAttendance();
    saveLeaves();
}

std::vector<Employee> HRMS::searchEmployees(const std::string& keyword) const {
    std::vector<Employee> result;
    const std::string key = lowercase(keyword);
    for (const auto& e : employees_)
        if (lowercase(e.name).find(key) != std::string::npos ||
            lowercase(e.department).find(key) != std::string::npos ||
            lowercase(e.email).find(key) != std::string::npos)
            result.push_back(e);
    return result;
}

void HRMS::markAttendance(int employeeId, std::string date, char status) {
    ensureEmployee(employeeId);
    if (!validDate(date)) throw std::invalid_argument("Invalid date. Use YYYY-MM-DD.");
    status = static_cast<char>(std::toupper(static_cast<unsigned char>(status)));
    if (status != 'P' && status != 'A')
        throw std::invalid_argument("Status must be P (present) or A (absent).");
    for (auto& a : attendance_) {
        if (a.employeeId == employeeId && a.date == date) {
            a.status = status;
            saveAttendance();
            return;
        }
    }
    attendance_.push_back({employeeId, std::move(date), status});
    saveAttendance();
}

std::vector<Attendance> HRMS::attendanceForMonth(const std::string& month) const {
    if (month.size() != 7 || !validDate(month + "-01"))
        throw std::invalid_argument("Invalid month. Use YYYY-MM.");
    std::vector<Attendance> result;
    for (const auto& a : attendance_)
        if (a.date.substr(0, 7) == month) result.push_back(a);
    return result;
}

int HRMS::requestLeave(int employeeId, std::string date, std::string reason) {
    ensureEmployee(employeeId);
    if (!validDate(date)) throw std::invalid_argument("Invalid leave date.");
    requireNonEmpty(reason, "Leave reason");
    for (const auto& l : leaves_) {
        if (l.employeeId == employeeId && l.date == date && l.status != 'R')
            throw std::invalid_argument("A non-rejected leave request already exists for that day.");
    }
    int id = 1;
    for (const auto& l : leaves_) id = std::max(id, l.id + 1);
    leaves_.push_back({id, employeeId, std::move(date), std::move(reason), 'P'});
    saveLeaves();
    return id;
}

void HRMS::reviewLeave(int requestId, bool approve) {
    for (auto& l : leaves_) {
        if (l.id == requestId) {
            if (l.status != 'P') throw std::invalid_argument("Leave already reviewed.");
            l.status = approve ? 'A' : 'R';
            saveLeaves();
            return;
        }
    }
    throw std::invalid_argument("Leave request ID not found.");
}

std::map<std::string, int> HRMS::departmentHeadcount() const {
    std::map<std::string, int> counts;
    for (const auto& e : employees_) ++counts[e.department];
    return counts;
}

double HRMS::monthlyPayrollEstimate() const {
    double annualTotal = 0;
    for (const auto& e : employees_) annualTotal += e.annualSalary;
    return annualTotal / 12.0;
}
