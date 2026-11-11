"""End-to-end HTTP API tests against an isolated persistent SQLite database."""
from __future__ import annotations
import os
from pathlib import Path
import sys
import tempfile
from fastapi.testclient import TestClient

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
TMP = tempfile.TemporaryDirectory(prefix="peopleflow_tests_")
os.environ["APP_ENV"] = "development"
os.environ["DATABASE_URL"] = "sqlite:///" + str(Path(TMP.name, "test.db")).replace("\\", "/")
from server.main import app  # noqa: E402

client = TestClient(app)


def post(c, route, data, csrf=None, method="post"):
    headers = {"X-CSRF-Token": csrf} if csrf else {}
    return getattr(c, method)(route, data=data, headers=headers)


def register_org(name, email):
    c = TestClient(app)
    response = post(c, "/api/register", {"organization": name, "name": "Project Owner", "email": email, "password": "GoodStrongerPassword!123"})
    assert response.status_code == 200, response.text
    current = c.get("/api/me")
    assert current.status_code == 200
    return c, current.json()["data"]["csrf"]


def add_employee(c, csrf, name, email, salary=600000):
    return post(c, "/api/employees", {"name": name, "email": email, "department": "Engineering", "job_title": "Engineer",
                                        "annual_salary": salary, "status": "Active", "joined_on": "2026-11-01"}, csrf)


def test_real_service_with_multi_tenant_auth_and_persistence():
    unauth = TestClient(app)
    assert unauth.get("/api/me").status_code == 401
    assert unauth.get("/api/employees").status_code == 401
    assert unauth.get("/api/health").status_code == 200
    assert b"Create an organization" in unauth.get("/").content
    assert b"Demo@123" not in unauth.get("/").content

    alice, csrf_a = register_org("North Star Labs", "owner1@example.com")
    bob, csrf_b = register_org("Bright Start Ltd", "owner2@example.com")
    assert alice.get("/api/dashboard").json()["data"]["employees"] == 0
    assert alice.get("/api/users").json()["data"][0]["role"] == "Admin"
    assert bob.get("/api/employees").json()["data"] == []

    e = add_employee(alice, csrf_a, "Riya Sharma", "riya@example.com")
    assert e.status_code == 200, e.text
    eid = e.json()["data"]["id"]
    assert alice.get("/api/employees").json()["data"][0]["name"] == "Riya Sharma"
    assert bob.get("/api/employees").json()["data"] == []  # isolated company
    bob_employee = add_employee(bob, csrf_b, "Sam Jones", "sam@example.com")
    assert bob_employee.status_code == 200
    bid = bob_employee.json()["data"]["id"]
    assert bid != eid

    # Foreign employee IDs must never resolve in another organization.
    forbidden = post(bob, "/api/users", {"name": "Wrong Org", "email": "cross@example.com", "role": "Employee",
                                            "employee_id": eid, "password": "GreatPasswordString123!"}, csrf_b)
    assert forbidden.status_code == 404
    assert bob.delete(f"/api/employees/{eid}", headers={"X-CSRF-Token": csrf_b}).status_code == 404

    # No auth bypass via missing CSRF header.
    assert post(alice, "/api/employees", {"name": "Hacker"}).status_code == 403
    assert post(bob, "/api/attendance", {"employee_id": bid, "day": "2026-11-11", "status": "Present"}, csrf_b).status_code == 200
    # Bob's attendance must not appear in Alice's workspace.
    assert alice.get("/api/attendance").json()["data"] == []

    assert post(alice, "/api/attendance", {"employee_id": eid, "day": "2026-11-11", "status": "Present"}, csrf_a).status_code == 200
    assert post(alice, "/api/attendance", {"employee_id": eid, "day": "2026-11-11", "status": "Remote"}, csrf_a).status_code == 200
    assert len(alice.get("/api/attendance").json()["data"]) == 1
    assert alice.get("/api/attendance").json()["data"][0]["status"] == "Remote"

    euser = post(alice, "/api/users", {"name": "Riya Sharma", "email": "riya.login@example.com", "role": "Employee",
                                      "employee_id": eid, "password": "SomeStrongPassword!1234"}, csrf_a)
    assert euser.status_code == 200, euser.text
    emp = TestClient(app)
    login = post(emp, "/api/login", {"email": "riya.login@example.com", "password": "SomeStrongPassword!1234"})
    assert login.status_code == 200
    csrf_emp = emp.get("/api/me").json()["data"]["csrf"]
    assert emp.get("/api/payroll").status_code == 403
    assert emp.get("/api/users").status_code == 403
    assert len(emp.get("/api/employees").json()["data"]) == 1
    assert post(emp, "/api/attendance", {"employee_id": eid, "day": "2026-11-12", "status": "Present"}, csrf_emp).status_code == 403
    assert post(emp, "/api/leaves", {"start_date": "2026-11-13", "end_date": "2026-11-14", "reason": "Family event"}, csrf_emp).status_code == 200
    assert post(emp, "/api/leaves", {"start_date": "2026-11-14", "end_date": "2026-11-15", "reason": "Overlap"}, csrf_emp).status_code == 409
    leave_id = alice.get("/api/leaves").json()["data"][0]["id"]
    assert post(alice, f"/api/leaves/{leave_id}", {"status": "Approved"}, csrf_a, "put").status_code == 200
    assert post(alice, f"/api/leaves/{leave_id}", {"status": "Rejected"}, csrf_a, "put").status_code == 400

    # Member can change password; old password then stops working.
    assert post(emp, "/api/password", {"old_password": "SomeStrongPassword!1234", "new_password": "NewStrongPassword!!4321"}, csrf_emp).status_code == 200
    assert post(emp, "/api/logout", {}, csrf_emp).status_code == 200
    assert emp.get("/api/me").status_code == 401
    assert post(emp, "/api/login", {"email": "riya.login@example.com", "password": "SomeStrongPassword!1234"}).status_code == 401
    assert post(emp, "/api/login", {"email": "riya.login@example.com", "password": "NewStrongPassword!!4321"}).status_code == 200

    export = alice.get("/api/export/employees")
    assert export.status_code == 200 and b"Riya Sharma" in export.content
    assert bob.get("/api/export/employees").status_code == 200
    assert b"Riya Sharma" not in bob.get("/api/export/employees").content
    assert len(alice.get("/api/audit").json()["data"]) >= 4

    # Archiving retains history rather than deleting related records.
    assert alice.delete(f"/api/employees/{eid}", headers={"X-CSRF-Token": csrf_a}).status_code == 200
    assert alice.get("/api/employees").json()["data"][0]["status"] == "Inactive"
    assert len(alice.get("/api/leaves").json()["data"]) == 1

    # DB remains durable when a separate connection is opened.
    from server.main import SessionLocal, Employee
    with SessionLocal() as db:
        assert db.query(Employee).count() == 2


def test_validate_and_security_guards():
    c = TestClient(app)
    weak = post(c, "/api/register", {"organization": "Weak LLC", "name": "A User", "email": "weak@example.com", "password": "12345678"})
    assert weak.status_code == 400
    assert c.get("/api/unknown").status_code == 404
    assert c.get("/", headers={"host": "evil.example.com"}).status_code == 400
    assert c.get("/api/audit").status_code == 401
