#include "HRMS.hpp"

#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::string ask(const std::string& prompt) {
    std::cout << prompt;
    std::string text;
    if (!std::getline(std::cin, text)) throw std::runtime_error("Input ended.");
    return text;
}

int askInt(const std::string& prompt) {
    while (true) {
        const std::string s = ask(prompt);
        try {
            std::size_t pos = 0;
            const int value = std::stoi(s, &pos);
            if (pos == s.size()) return value;
        } catch (const std::exception&) {}
        std::cout << "Enter a whole number.\n";
    }
}

double askMoney(const std::string& prompt) {
    while (true) {
        const std::string s = ask(prompt);
        try {
            std::size_t pos = 0;
            const double value = std::stod(s, &pos);
            if (pos == s.size() && value >= 0 && value <= 1e12)
                return value;
        } catch (const std::exception&) {}
        std::cout << "Enter a non-negative amount (up to 1 trillion).\n";
    }
}

void showEmployees(const std::vector<Employee>& list) {
    if (list.empty()) { std::cout << "No employees found.\n"; return; }
    std::cout << "\n" << std::left << std::setw(6) << "ID" << std::setw(24) << "Name"
              << std::setw(18) << "Department" << std::setw(29) << "Email"
              << "Annual salary\n";
    std::cout << std::string(95, '-') << '\n';
    for (const auto& e : list)
        std::cout << std::left << std::setw(6) << e.id << std::setw(24) << e.name
                  << std::setw(18) << e.department << std::setw(29) << e.email
                  << std::fixed << std::setprecision(2) << e.annualSalary << '\n';
}

const char* leaveStatus(char s) {
    switch (s) {
        case 'P': return "Pending";
        case 'A': return "Approved";
        case 'R': return "Rejected";
        default: return "Unknown";
    }
}

void showMenu() {
    std::cout << "\n========== PeopleFlow HRMS ==========\n"
              << " 1. Add employee\n 2. List employees\n 3. Search employees\n"
              << " 4. Update salary\n 5. Remove employee\n 6. Mark attendance\n"
              << " 7. Monthly attendance report\n 8. Request one-day leave\n"
              << " 9. View leave requests\n10. Approve/reject leave\n"
              << "11. HR dashboard\n 0. Exit\n";
}
} // namespace

int main(int argc, char* argv[]) {
    // Optional custom data directory allows demos/tests without touching user data.
    const std::string dataDir = argc > 1 ? argv[1] : "data";
    try {
        HRMS app(dataDir);
        std::cout << "PeopleFlow HRMS | Stored data: " << dataDir << "\n";
        while (true) {
            showMenu();
            int choice;
            try { choice = askInt("Choose: "); }
            catch (const std::runtime_error&) { std::cout << "\nGoodbye!\n"; break; }
            if (choice == 0) { std::cout << "Goodbye!\n"; break; }
            try {
                switch (choice) {
                    case 1: {
                        auto name = ask("Full name: ");
                        auto dept = ask("Department: ");
                        auto email = ask("Email: ");
                        auto salary = askMoney("Annual salary (INR): ");
                        const int id = app.addEmployee(name, dept, email, salary);
                        std::cout << "Employee added. ID: " << id << '\n';
                        break;
                    }
                    case 2: showEmployees(app.employees()); break;
                    case 3: showEmployees(app.searchEmployees(ask("Search keyword: "))); break;
                    case 4: {
                        int id = askInt("Employee ID: ");
                        double salary = askMoney("New annual salary (INR): ");
                        app.updateSalary(id, salary);
                        std::cout << "Salary updated.\n";
                        break;
                    }
                    case 5: {
                        int id = askInt("Employee ID to remove: ");
                        if (ask("Type DELETE to confirm: ") == "DELETE") {
                            app.removeEmployee(id);
                            std::cout << "Employee and related records removed.\n";
                        } else std::cout << "Cancelled.\n";
                        break;
                    }
                    case 6: {
                        int id = askInt("Employee ID: ");
                        auto date = ask("Date (YYYY-MM-DD): ");
                        auto s = ask("Status (P=present, A=absent): ");
                        if (s.size() != 1) throw std::invalid_argument("Enter P or A.");
                        app.markAttendance(id, date, s[0]);
                        std::cout << "Attendance saved.\n";
                        break;
                    }
                    case 7: {
                        auto month = ask("Month (YYYY-MM): ");
                        auto records = app.attendanceForMonth(month);
                        int present = 0, absent = 0;
                        for (const auto& a : records) {
                            const auto* e = app.findEmployee(a.employeeId);
                            std::cout << a.date << " | " << (e ? e->name : "Unknown")
                                      << " (#" << a.employeeId << ") | "
                                      << (a.status == 'P' ? "Present" : "Absent") << '\n';
                            if (a.status == 'P') ++present; else ++absent;
                        }
                        std::cout << "Total records: " << records.size()
                                  << " | Present: " << present << " | Absent: " << absent << '\n';
                        break;
                    }
                    case 8: {
                        int id = askInt("Employee ID: ");
                        auto date = ask("Leave date (YYYY-MM-DD): ");
                        auto reason = ask("Reason: ");
                        int requestId = app.requestLeave(id, date, reason);
                        std::cout << "Leave requested. Request ID: " << requestId << '\n';
                        break;
                    }
                    case 9: {
                        if (app.leaves().empty()) std::cout << "No leave requests.\n";
                        for (const auto& l : app.leaves()) {
                            const auto* e = app.findEmployee(l.employeeId);
                            std::cout << "Request #" << l.id << " | "
                                      << (e ? e->name : "Unknown") << " | " << l.date
                                      << " | " << l.reason << " | " << leaveStatus(l.status) << '\n';
                        }
                        break;
                    }
                    case 10: {
                        int id = askInt("Leave request ID: ");
                        auto decision = ask("Approve (A) or Reject (R): ");
                        if (decision != "A" && decision != "a" && decision != "R" && decision != "r")
                            throw std::invalid_argument("Enter A or R.");
                        app.reviewLeave(id, decision == "A" || decision == "a");
                        std::cout << "Leave decision saved.\n";
                        break;
                    }
                    case 11: {
                        std::cout << "\n--- HR DASHBOARD ---\n"
                                  << "Total employees: " << app.employees().size() << '\n'
                                  << "Estimated monthly base salary bill: INR "
                                  << std::fixed << std::setprecision(2)
                                  << app.monthlyPayrollEstimate() << '\n'
                                  << "Employees by department:\n";
                        for (const auto& [dept, count] : app.departmentHeadcount())
                            std::cout << "  " << dept << ": " << count << '\n';
                        int pending = 0;
                        for (const auto& l : app.leaves()) if (l.status == 'P') ++pending;
                        std::cout << "Pending leave requests: " << pending << '\n';
                        std::cout << "Note: salary figure is a basic estimate, not statutory payroll.\n";
                        break;
                    }
                    default: std::cout << "Invalid option.\n";
                }
            } catch (const std::invalid_argument& error) {
                std::cout << "Input error: " << error.what() << '\n';
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
