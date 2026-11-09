#include "HRMS.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main() {
    namespace fs = std::filesystem;
    const auto tempDir = fs::temp_directory_path() / "peopleflow_hrms_test";
    fs::remove_all(tempDir);

    {
        HRMS app(tempDir);
        assert(app.employees().empty());
        int first = app.addEmployee("Aarav Sharma", "Engineering", "aarav@example.com", 600000);
        int second = app.addEmployee("Neha Gupta", "HR", "neha@example.com", 720000);
        assert(first == 1 && second == 2);
        assert(app.searchEmployees("ENGINEER").size() == 1);
        assert(app.findEmployee(2)->name == "Neha Gupta");
        assert(std::abs(app.monthlyPayrollEstimate() - 110000) < 0.0001);

        bool duplicateRejected = false;
        try { app.addEmployee("Other", "HR", "AARAV@example.com", 1); }
        catch (const std::invalid_argument&) { duplicateRejected = true; }
        assert(duplicateRejected);

        assert(HRMS::validDate("2024-02-29"));
        assert(!HRMS::validDate("2025-02-29"));
        app.markAttendance(first, "2026-11-04", 'P');
        app.markAttendance(first, "2026-11-04", 'A'); // overwrite, not duplicate
        assert(app.attendanceForMonth("2026-11").size() == 1);
        assert(app.attendanceForMonth("2026-11")[0].status == 'A');

        int leave = app.requestLeave(second, "2026-11-09", "Medical appointment");
        app.reviewLeave(leave, true);
        assert(app.leaves()[0].status == 'A');
        app.updateSalary(second, 840000);
        app.updateEmployee(first, "Aarav S Sharma", "Product", "aarav@example.com", 650000);
        assert(app.findEmployee(first)->name == "Aarav S Sharma");
        assert(app.departmentHeadcount().at("Product") == 1);
        bool editDuplicateRejected = false;
        try { app.updateEmployee(second, "Neha Gupta", "HR", "AARAV@example.com", 840000); }
        catch (const std::invalid_argument&) { editDuplicateRejected = true; }
        assert(editDuplicateRejected);
        assert(app.departmentHeadcount().at("HR") == 1);
    }
    {
        HRMS reloaded(tempDir);
        assert(reloaded.employees().size() == 2);
        assert(reloaded.attendance().size() == 1);
        assert(reloaded.leaves().size() == 1);
        assert(reloaded.findEmployee(2)->annualSalary == 840000);
        reloaded.removeEmployee(1);
        assert(reloaded.employees().size() == 1);
        assert(reloaded.attendance().empty());
    }

    fs::remove_all(tempDir);
    std::cout << "All HRMS tests passed.\n";
}
