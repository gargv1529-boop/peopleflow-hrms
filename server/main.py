"""PeopleFlow - actual multi-organization HRMS backend.

FastAPI + SQLAlchemy; PostgreSQL on the public deployment, SQLite for local development.
Use fictional data until external security, privacy, and operational reviews are complete.
"""
from __future__ import annotations

import csv
import hashlib
import io
import json
import os
import re
import secrets
import threading
import time
from datetime import date, datetime, timedelta, timezone
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs

from argon2 import PasswordHasher
from argon2.exceptions import VerifyMismatchError, VerificationError
from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import FileResponse, JSONResponse, PlainTextResponse, Response, StreamingResponse
from sqlalchemy import (
    Boolean, Date, DateTime, ForeignKey, Integer, String, Text, UniqueConstraint,
    create_engine, event, func, select
)
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column, sessionmaker
from starlette.middleware.trustedhost import TrustedHostMiddleware

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / "web"
DATA = ROOT / "data"
DATA.mkdir(parents=True, exist_ok=True)
ENV = os.getenv("APP_ENV", "development").lower()
IS_PRODUCTION = ENV == "production"
# Render exposes the public URL and hostname automatically. Explicit values take
# precedence, which is useful when switching to a custom domain.
PUBLIC_BASE_URL = (os.getenv("PUBLIC_BASE_URL") or os.getenv("RENDER_EXTERNAL_URL") or "").rstrip("/")
ALLOWED_HOSTS_CONFIG = os.getenv("ALLOWED_HOSTS") or os.getenv("RENDER_EXTERNAL_HOSTNAME") or ""
DATABASE_URL = os.getenv("DATABASE_URL", f"sqlite:///{(DATA / 'peopleflow_real.db').as_posix()}")
if DATABASE_URL.startswith("postgres://"):
    DATABASE_URL = DATABASE_URL.replace("postgres://", "postgresql+psycopg://", 1)
if DATABASE_URL.startswith("postgresql://"):
    DATABASE_URL = DATABASE_URL.replace("postgresql://", "postgresql+psycopg://", 1)
if IS_PRODUCTION:
    if not DATABASE_URL.startswith("postgresql+"):
        raise RuntimeError("Production requires a durable PostgreSQL DATABASE_URL; do not use ephemeral SQLite hosting.")
    if not PUBLIC_BASE_URL.startswith("https://"):
        raise RuntimeError("Production requires PUBLIC_BASE_URL=https://your-real-host")
    if not ALLOWED_HOSTS_CONFIG:
        raise RuntimeError("Production requires ALLOWED_HOSTS=your-hostname")

engine = create_engine(
    DATABASE_URL,
    pool_pre_ping=True,
    connect_args={"check_same_thread": False, "timeout": 20} if DATABASE_URL.startswith("sqlite:") else {},
)
if DATABASE_URL.startswith("sqlite:"):
    @event.listens_for(engine, "connect")
    def sqlite_foreign_keys(dbapi_connection, _record):
        cur = dbapi_connection.cursor()
        cur.execute("PRAGMA foreign_keys=ON")
        cur.close()

SessionLocal = sessionmaker(bind=engine, expire_on_commit=False)

class Base(DeclarativeBase):
    pass

class Organization(Base):
    __tablename__ = "organizations"
    id: Mapped[int] = mapped_column(primary_key=True)
    name: Mapped[str] = mapped_column(String(120))
    slug: Mapped[str] = mapped_column(String(90), unique=True, index=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))

class Employee(Base):
    __tablename__ = "employees"
    __table_args__ = (UniqueConstraint("org_id", "email", name="uq_employee_org_email"),)
    id: Mapped[int] = mapped_column(primary_key=True)
    org_id: Mapped[int] = mapped_column(ForeignKey("organizations.id"), index=True)
    name: Mapped[str] = mapped_column(String(120))
    email: Mapped[str] = mapped_column(String(254))
    department: Mapped[str] = mapped_column(String(80))
    job_title: Mapped[str] = mapped_column(String(100))
    annual_salary: Mapped[int] = mapped_column(Integer, default=0)
    status: Mapped[str] = mapped_column(String(20), default="Active")
    joined_on: Mapped[date] = mapped_column(Date)

class User(Base):
    __tablename__ = "users"
    id: Mapped[int] = mapped_column(primary_key=True)
    org_id: Mapped[int] = mapped_column(ForeignKey("organizations.id"), index=True)
    name: Mapped[str] = mapped_column(String(120))
    email: Mapped[str] = mapped_column(String(254), unique=True, index=True)
    role: Mapped[str] = mapped_column(String(12))
    employee_id: Mapped[int | None] = mapped_column(ForeignKey("employees.id"), nullable=True)
    password_hash: Mapped[str] = mapped_column(Text)
    is_active: Mapped[bool] = mapped_column(Boolean, default=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))

class WebSession(Base):
    __tablename__ = "sessions"
    id: Mapped[int] = mapped_column(primary_key=True)
    user_id: Mapped[int] = mapped_column(ForeignKey("users.id"), index=True)
    token_hash: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    csrf: Mapped[str] = mapped_column(String(64))
    expires_at: Mapped[datetime] = mapped_column(DateTime(timezone=True))
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))

class Attendance(Base):
    __tablename__ = "attendance"
    __table_args__ = (UniqueConstraint("org_id", "employee_id", "day", name="uq_attendance_org_employee_day"),)
    id: Mapped[int] = mapped_column(primary_key=True)
    org_id: Mapped[int] = mapped_column(ForeignKey("organizations.id"), index=True)
    employee_id: Mapped[int] = mapped_column(ForeignKey("employees.id"))
    day: Mapped[date] = mapped_column(Date)
    status: Mapped[str] = mapped_column(String(20))

class LeaveRequest(Base):
    __tablename__ = "leaves"
    id: Mapped[int] = mapped_column(primary_key=True)
    org_id: Mapped[int] = mapped_column(ForeignKey("organizations.id"), index=True)
    employee_id: Mapped[int] = mapped_column(ForeignKey("employees.id"))
    start_date: Mapped[date] = mapped_column(Date)
    end_date: Mapped[date] = mapped_column(Date)
    reason: Mapped[str] = mapped_column(String(500))
    status: Mapped[str] = mapped_column(String(20), default="Pending")
    reviewed_by: Mapped[int | None] = mapped_column(ForeignKey("users.id"), nullable=True)

class AuditLog(Base):
    __tablename__ = "audit_log"
    id: Mapped[int] = mapped_column(primary_key=True)
    org_id: Mapped[int] = mapped_column(ForeignKey("organizations.id"), index=True)
    actor: Mapped[str] = mapped_column(String(120))
    action: Mapped[str] = mapped_column(String(80))
    entity: Mapped[str] = mapped_column(String(60))
    entity_id: Mapped[int] = mapped_column(Integer)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))

Base.metadata.create_all(engine)
app = FastAPI(title="PeopleFlow HRMS", docs_url=None, redoc_url=None, openapi_url=None)
allowed_hosts = [h.strip() for h in (ALLOWED_HOSTS_CONFIG or "127.0.0.1,localhost,testserver").split(",") if h.strip()]
app.add_middleware(TrustedHostMiddleware, allowed_hosts=allowed_hosts)
ph = PasswordHasher(time_cost=3, memory_cost=65536, parallelism=2)
EMAIL = re.compile(r"^[A-Za-z0-9.!#$%&'*+/=?^_`{|}~-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$")
SLUG = re.compile(r"^[a-z0-9][a-z0-9-]{2,59}$")
failed_logins: dict[str, list[float]] = {}
rate_lock = threading.Lock()


def now() -> datetime:
    return datetime.now(timezone.utc)


def api(data: Any = None) -> JSONResponse:
    return JSONResponse({"ok": True, "data": data})


def fail(message: str, status: int = 400) -> None:
    raise HTTPException(status_code=status, detail=message)


@app.exception_handler(HTTPException)
async def http_exception_handler(_request: Request, exc: HTTPException):
    return JSONResponse({"ok": False, "error": str(exc.detail)}, status_code=exc.status_code)


@app.middleware("http")
async def security_headers(request: Request, call_next):
    if request.method in ("POST", "PUT", "PATCH", "DELETE"):
        origin = request.headers.get("origin")
        if origin:
            expected = PUBLIC_BASE_URL if IS_PRODUCTION else str(request.base_url).rstrip("/")
            if origin.rstrip("/") != expected:
                return JSONResponse({"ok": False, "error": "Request origin not allowed"}, status_code=403)
    response = await call_next(request)
    response.headers["X-Content-Type-Options"] = "nosniff"
    response.headers["X-Frame-Options"] = "DENY"
    response.headers["Referrer-Policy"] = "strict-origin-when-cross-origin"
    response.headers["Content-Security-Policy"] = "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; font-src 'self'; object-src 'none'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'"
    response.headers["Cache-Control"] = "no-store" if request.url.path.startswith("/api/") else "no-cache"
    if IS_PRODUCTION:
        response.headers["Strict-Transport-Security"] = "max-age=31536000; includeSubDomains"
    return response


async def form(request: Request) -> dict[str, str]:
    if not request.headers.get("content-type", "").startswith("application/x-www-form-urlencoded"):
        fail("Invalid form content type", 415)
    body = await request.body()
    if len(body) > 20000:
        fail("Request too large", 413)
    try:
        return {key: values[-1] for key, values in parse_qs(body.decode("utf-8"), max_num_fields=30, keep_blank_values=True).items()}
    except (UnicodeDecodeError, ValueError):
        fail("Malformed form")


def string(data: dict, key: str, minlen: int = 1, maxlen: int = 120) -> str:
    value = (data.get(key) or "").strip()
    if len(value) < minlen or len(value) > maxlen:
        fail(f"{key.replace('_', ' ').capitalize()} must contain {minlen}-{maxlen} characters")
    return value


def email_addr(data: dict, key: str = "email") -> str:
    value = string(data, key, 5, 254).lower()
    if not EMAIL.fullmatch(value):
        fail("Please enter a valid email address")
    return value


def password_valid(p: str) -> None:
    if len(p) < 12 or len(p) > 128:
        fail("Use a password of 12-128 characters")
    if len(set(p)) < 5:
        fail("Choose a less predictable password")


def parse_date(data: dict, field: str) -> date:
    try:
        return date.fromisoformat(string(data, field, 10, 10))
    except ValueError:
        fail(f"{field} must be a valid YYYY-MM-DD date")


def integer(data: dict, field: str, minimum: int = 0, maximum: int = 10**9) -> int:
    try:
        number = int(data.get(field, ""))
    except (TypeError, ValueError):
        fail(f"Invalid {field}")
    if not minimum <= number <= maximum:
        fail(f"{field} is outside the allowed range")
    return number


def staff(user: User) -> None:
    if user.role not in ("Admin", "HR"):
        fail("You do not have permission for this action", 403)


def admin(user: User) -> None:
    if user.role != "Admin":
        fail("Administrator access required", 403)


def record_audit(db, user: User, action: str, entity: str, entity_id: int):
    db.add(AuditLog(org_id=user.org_id, actor=user.name, action=action, entity=entity, entity_id=entity_id))


def session_for(db, request: Request, csrf: bool = False) -> tuple[User, WebSession]:
    token = request.cookies.get("peopleflow_session", "")
    if not token or len(token) > 250:
        fail("Please sign in", 401)
    item = db.scalar(select(WebSession).where(WebSession.token_hash == hashlib.sha256(token.encode()).hexdigest()))
    if not item:
        fail("Please sign in", 401)
    expires = item.expires_at
    if expires.tzinfo is None:
        expires = expires.replace(tzinfo=timezone.utc)
    if expires < now():
        db.delete(item)
        db.commit()
        fail("Your session expired. Please sign in", 401)
    user = db.get(User, item.user_id)
    if not user or not user.is_active:
        fail("Please sign in", 401)
    if csrf and not secrets.compare_digest(request.headers.get("x-csrf-token", ""), item.csrf):
        fail("Invalid CSRF token", 403)
    return user, item


def sign_in(db, user: User) -> tuple[str, str]:
    token = secrets.token_urlsafe(48)
    csrf = secrets.token_urlsafe(32)
    db.add(WebSession(user_id=user.id, token_hash=hashlib.sha256(token.encode()).hexdigest(), csrf=csrf, expires_at=now() + timedelta(hours=12)))
    db.commit()
    return token, csrf


def cookie(response: Response, token: str) -> None:
    response.set_cookie("peopleflow_session", token, max_age=12*3600, httponly=True, secure=IS_PRODUCTION, samesite="strict", path="/")


def rate_limit(key: str) -> None:
    t = time.time()
    with rate_lock:
        hits = [x for x in failed_logins.get(key, []) if t - x < 900]
        if len(hits) >= 10:
            fail("Too many attempts; try again later", 429)
        hits.append(t)
        failed_logins[key] = hits
        if len(failed_logins) > 5000:
            failed_logins.clear()


def clear_attempts(key: str) -> None:
    with rate_lock:
        failed_logins.pop(key, None)


def employee_json(e: Employee) -> dict:
    return {"id": e.id, "name": e.name, "email": e.email, "department": e.department, "job_title": e.job_title,
            "annual_salary": e.annual_salary, "status": e.status, "joined_on": e.joined_on.isoformat()}


def employee_for(db, user: User, employee_id: int) -> Employee:
    e = db.scalar(select(Employee).where(Employee.id == employee_id, Employee.org_id == user.org_id))
    if not e:
        fail("Employee not found", 404)
    return e


def authenticated(request: Request, csrf: bool = False):
    db = SessionLocal()
    try:
        user, session = session_for(db, request, csrf)
        yield db, user, session
    finally:
        db.close()


@app.get("/api/health")
def health():
    try:
        with SessionLocal() as db:
            db.execute(select(func.count()).select_from(Organization)).scalar()
        return api({"status": "ok"})
    except Exception:
        return JSONResponse({"ok": False, "error": "Database unavailable"}, status_code=503)


@app.post("/api/register")
async def register(request: Request):
    data = await form(request)
    org_name = string(data, "organization", 3, 120)
    slug = re.sub(r"[^a-z0-9-]+", "-", org_name.lower()).strip("-")[:55]
    if len(slug) < 3:
        fail("Organization name is too short")
    email = email_addr(data)
    name = string(data, "name", 2, 120)
    password = data.get("password", "")
    password_valid(password)
    rate_limit("signup:" + (request.client.host if request.client else "unknown"))
    with SessionLocal() as db:
        if db.scalar(select(User.id).where(User.email == email)):
            fail("This email is already registered", 409)
        base_slug = slug
        for _ in range(15):
            if not db.scalar(select(Organization.id).where(Organization.slug == slug)):
                break
            slug = f"{base_slug[:45]}-{secrets.token_hex(3)}"
        org = Organization(name=org_name, slug=slug)
        db.add(org)
        db.flush()
        owner = User(org_id=org.id, name=name, email=email, role="Admin", password_hash=ph.hash(password))
        db.add(owner)
        db.flush()
        record_audit(db, owner, "organization_created", "organization", org.id)
        db.commit()
        token, _ = sign_in(db, owner)
        response = api({"organization": org.name, "user": owner.name})
        cookie(response, token)
        return response


@app.post("/api/login")
async def login(request: Request):
    data = await form(request)
    email = email_addr(data)
    password = data.get("password", "")
    limiter = "login:" + (request.client.host if request.client else "unknown") + ":" + email
    rate_limit(limiter)
    with SessionLocal() as db:
        user = db.scalar(select(User).where(User.email == email, User.is_active.is_(True)))
        try:
            valid = bool(user and ph.verify(user.password_hash, password))
        except (VerifyMismatchError, VerificationError):
            valid = False
        if not valid:
            fail("Incorrect email or password", 401)
        clear_attempts(limiter)
        token, _ = sign_in(db, user)
        response = api({"name": user.name, "role": user.role})
        cookie(response, token)
        return response


@app.get("/api/me")
def me(request: Request):
    with SessionLocal() as db:
        user, sess = session_for(db, request)
        org = db.get(Organization, user.org_id)
        return api({"user": {"id": user.id, "name": user.name, "role": user.role, "email": user.email,
                              "employee_id": user.employee_id, "organization": org.name}, "csrf": sess.csrf})


@app.post("/api/logout")
def logout(request: Request):
    with SessionLocal() as db:
        _, sess = session_for(db, request, csrf=True)
        db.delete(sess)
        db.commit()
        response = api({"message": "Signed out"})
        response.delete_cookie("peopleflow_session", path="/")
        return response


@app.post("/api/password")
async def change_password(request: Request):
    data = await form(request)
    with SessionLocal() as db:
        user, sess = session_for(db, request, csrf=True)
        old = data.get("old_password", "")
        try:
            valid = ph.verify(user.password_hash, old)
        except (VerifyMismatchError, VerificationError):
            valid = False
        if not valid:
            fail("Current password is incorrect", 400)
        new_pass = data.get("new_password", "")
        password_valid(new_pass)
        if new_pass == old:
            fail("Choose a different password")
        user.password_hash = ph.hash(new_pass)
        # Sign out other devices while keeping the current browser session.
        for s in db.scalars(select(WebSession).where(WebSession.user_id == user.id, WebSession.id != sess.id)).all():
            db.delete(s)
        record_audit(db, user, "password_changed", "user", user.id)
        db.commit()
        return api({"message": "Password updated"})


@app.get("/api/dashboard")
def dashboard(request: Request):
    with SessionLocal() as db:
        user, _ = session_for(db, request)
        if user.role == "Employee":
            if not user.employee_id:
                return api({"attendance_days": 0, "pending": 0, "departments": []})
            attendance_count = db.scalar(select(func.count()).select_from(Attendance).where(Attendance.org_id == user.org_id, Attendance.employee_id == user.employee_id, Attendance.status.in_(["Present", "Remote"])))
            pending = db.scalar(select(func.count()).select_from(LeaveRequest).where(LeaveRequest.org_id == user.org_id, LeaveRequest.employee_id == user.employee_id, LeaveRequest.status == "Pending"))
            return api({"attendance_days": attendance_count, "pending": pending, "departments": []})
        emps = db.scalars(select(Employee).where(Employee.org_id == user.org_id)).all()
        pending = db.scalar(select(func.count()).select_from(LeaveRequest).where(LeaveRequest.org_id == user.org_id, LeaveRequest.status == "Pending"))
        depts: dict[str, int] = {}
        for e in emps:
            if e.status != "Inactive":
                depts[e.department] = depts.get(e.department, 0) + 1
        return api({"employees": len(emps), "active": sum(e.status != "Inactive" for e in emps),
                    "pending": pending, "payroll": round(sum(e.annual_salary for e in emps if e.status != "Inactive") / 12),
                    "departments": [{"department": k, "headcount": v} for k, v in sorted(depts.items())]})


@app.get("/api/employees")
def employees(request: Request):
    with SessionLocal() as db:
        user, _ = session_for(db, request)
        q = select(Employee).where(Employee.org_id == user.org_id)
        if user.role == "Employee":
            q = q.where(Employee.id == (user.employee_id or -1))
        emps = db.scalars(q.order_by(Employee.name).limit(2000)).all()
        return api([employee_json(e) for e in emps])


def modify_employee_fields(e: Employee, data: dict):
    e.name = string(data, "name", 2, 120)
    e.email = email_addr(data)
    e.department = string(data, "department", 2, 80)
    e.job_title = string(data, "job_title", 2, 100)
    e.annual_salary = integer(data, "annual_salary", 0, 100000000)
    e.joined_on = parse_date(data, "joined_on")
    e.status = data.get("status", "Active")
    if e.status not in ("Active", "On Leave", "Inactive"):
        fail("Invalid employment status")


@app.post("/api/employees")
async def add_employee(request: Request):
    data = await form(request)
    with SessionLocal() as db:
        user, _ = session_for(db, request, csrf=True)
        staff(user)
        e = Employee(org_id=user.org_id, name="", email="", department="", job_title="", annual_salary=0, joined_on=date.today())
        modify_employee_fields(e, data)
        if db.scalar(select(Employee.id).where(Employee.org_id == user.org_id, Employee.email == e.email)):
            fail("An employee with this email already exists", 409)
        db.add(e)
        db.flush()
        record_audit(db, user, "employee_created", "employee", e.id)
        db.commit()
        return api(employee_json(e))


@app.put("/api/employees/{employee_id}")
async def update_employee(employee_id: int, request: Request):
    data = await form(request)
    with SessionLocal() as db:
        user, _ = session_for(db, request, csrf=True)
        staff(user)
        e = employee_for(db, user, employee_id)
        modify_employee_fields(e, data)
        if db.scalar(select(Employee.id).where(Employee.org_id == user.org_id, Employee.email == e.email, Employee.id != e.id)):
            fail("An employee with this email already exists", 409)
        record_audit(db, user, "employee_updated", "employee", e.id)
        db.commit()
        return api(employee_json(e))


@app.delete("/api/employees/{employee_id}")
def archive_employee(employee_id: int, request: Request):
    with SessionLocal() as db:
        user, _ = session_for(db, request, csrf=True)
        staff(user)
        e = employee_for(db, user, employee_id)
        # Data retention: archive instead of deleting historical attendance and leave records.
        e.status = "Inactive"
        record_audit(db, user, "employee_archived", "employee", e.id)
        db.commit()
        return api({"id": e.id, "status": e.status})


@app.get("/api/attendance")
def get_attendance(request: Request):
    with SessionLocal() as db:
        user, _ = session_for(db, request)
        q = select(Attendance, Employee.name).join(Employee, Employee.id == Attendance.employee_id).where(Attendance.org_id == user.org_id, Employee.org_id == user.org_id)
        if user.role == "Employee":
            q = q.where(Attendance.employee_id == (user.employee_id or -1))
        rows = db.execute(q.order_by(Attendance.day.desc(), Attendance.id.desc()).limit(1000)).all()
        return api([{"id": a.id, "employee_id": a.employee_id, "employee": name, "day": a.day.isoformat(), "status": a.status} for a, name in rows])


@app.post("/api/attendance")
async def post_attendance(request: Request):
    data = await form(request)
    with SessionLocal() as db:
        user, _ = session_for(db, request, csrf=True)
        staff(user)
        e = employee_for(db, user, integer(data, "employee_id", 1))
        day = parse_date(data, "day")
        status = data.get("status")
        if status not in ("Present", "Absent", "Remote"):
            fail("Attendance must be Present, Absent, or Remote")
        a = db.scalar(select(Attendance).where(Attendance.org_id == user.org_id, Attendance.employee_id == e.id, Attendance.day == day))
        if not a:
            a = Attendance(org_id=user.org_id, employee_id=e.id, day=day, status=status)
            db.add(a)
        a.status = status
        db.flush()
        record_audit(db, user, "attendance_recorded", "attendance", a.id)
        db.commit()
        return api({"id": a.id, "status": status})


@app.get("/api/leaves")
def get_leaves(request: Request):
    with SessionLocal() as db:
        user, _ = session_for(db, request)
        q = select(LeaveRequest, Employee.name).join(Employee, Employee.id == LeaveRequest.employee_id).where(LeaveRequest.org_id == user.org_id, Employee.org_id == user.org_id)
        if user.role == "Employee":
            q = q.where(LeaveRequest.employee_id == (user.employee_id or -1))
        rows = db.execute(q.order_by(LeaveRequest.id.desc()).limit(1000)).all()
        return api([{"id": l.id, "employee": name, "employee_id": l.employee_id, "start_date": l.start_date.isoformat(),
                     "end_date": l.end_date.isoformat(), "status": l.status, "reason": l.reason} for l, name in rows])


@app.post("/api/leaves")
async def post_leave(request: Request):
    data = await form(request)
    with SessionLocal() as db:
        user, _ = session_for(db, request, csrf=True)
        eid = integer(data, "employee_id", 1) if user.role in ("Admin", "HR") else user.employee_id
        if not eid:
            fail("Ask your administrator to link your account to an employee", 403)
        e = employee_for(db, user, eid)
        start, end = parse_date(data, "start_date"), parse_date(data, "end_date")
        if start > end or (end - start).days > 365:
            fail("Enter a valid leave range of up to 366 days")
        reason = string(data, "reason", 4, 500)
        conflict = db.scalar(select(LeaveRequest.id).where(LeaveRequest.org_id == user.org_id, LeaveRequest.employee_id == e.id,
                    LeaveRequest.status.in_(["Pending", "Approved"]), LeaveRequest.start_date <= end, LeaveRequest.end_date >= start))
        if conflict:
            fail("This employee already has an overlapping pending/approved leave request", 409)
        l = LeaveRequest(org_id=user.org_id, employee_id=e.id, start_date=start, end_date=end, reason=reason, status="Pending")
        db.add(l)
        db.flush()
        record_audit(db, user, "leave_requested", "leave", l.id)
        db.commit()
        return api({"id": l.id, "status": l.status})


@app.put("/api/leaves/{leave_id}")
async def decide_leave(leave_id: int, request: Request):
    data = await form(request)
    with SessionLocal() as db:
        user, _ = session_for(db, request, csrf=True)
        staff(user)
        new_status = data.get("status")
        if new_status not in ("Approved", "Rejected"):
            fail("Decision must be Approved or Rejected")
        l = db.scalar(select(LeaveRequest).where(LeaveRequest.id == leave_id, LeaveRequest.org_id == user.org_id))
        if not l:
            fail("Leave request not found", 404)
        if l.status != "Pending":
            fail("Only pending leave requests may be reviewed")
        if user.employee_id and l.employee_id == user.employee_id:
            fail("You cannot approve your own leave", 403)
        l.status = new_status
        l.reviewed_by = user.id
        record_audit(db, user, "leave_" + new_status.lower(), "leave", l.id)
        db.commit()
        return api({"id": l.id, "status": l.status})


@app.get("/api/payroll")
def payroll(request: Request):
    with SessionLocal() as db:
        user, _ = session_for(db, request)
        staff(user)
        rows = db.scalars(select(Employee).where(Employee.org_id == user.org_id, Employee.status != "Inactive").order_by(Employee.name).limit(2000)).all()
        return api([{"id": e.id, "name": e.name, "department": e.department, "annual_salary": e.annual_salary,
                     "monthly_estimate": round(e.annual_salary / 12)} for e in rows])


@app.get("/api/users")
def users(request: Request):
    with SessionLocal() as db:
        user, _ = session_for(db, request)
        admin(user)
        members = db.scalars(select(User).where(User.org_id == user.org_id).order_by(User.id)).all()
        return api([{"id": u.id, "name": u.name, "email": u.email, "role": u.role,
                     "employee_id": u.employee_id, "is_active": u.is_active} for u in members])


@app.post("/api/users")
async def add_user(request: Request):
    data = await form(request)
    with SessionLocal() as db:
        owner, _ = session_for(db, request, csrf=True)
        admin(owner)
        name = string(data, "name", 2, 120)
        email = email_addr(data)
        role = data.get("role")
        if role not in ("Admin", "HR", "Employee"):
            fail("Invalid role")
        password = data.get("password", "")
        password_valid(password)
        eid = integer(data, "employee_id", 1) if data.get("employee_id") else None
        if role == "Employee" and not eid:
            fail("Employee accounts must be linked to an employee record")
        if eid:
            employee_for(db, owner, eid) # Prevent cross-organization linking.
            if db.scalar(select(User.id).where(User.org_id == owner.org_id, User.employee_id == eid)):
                fail("This employee already has an account", 409)
        if db.scalar(select(User.id).where(User.email == email)):
            fail("Email is already registered", 409)
        new_user = User(org_id=owner.org_id, name=name, email=email, role=role, employee_id=eid, password_hash=ph.hash(password))
        db.add(new_user)
        db.flush()
        record_audit(db, owner, "user_created", "user", new_user.id)
        db.commit()
        return api({"id": new_user.id, "role": role})


@app.get("/api/audit")
def audit(request: Request):
    with SessionLocal() as db:
        user, _ = session_for(db, request)
        admin(user)
        rows = db.scalars(select(AuditLog).where(AuditLog.org_id == user.org_id).order_by(AuditLog.id.desc()).limit(200)).all()
        return api([{"id": a.id, "created_at": a.created_at.isoformat(), "actor": a.actor,
                     "action": a.action, "entity": a.entity, "entity_id": a.entity_id} for a in rows])


def csv_protect(value) -> str:
    s = str(value if value is not None else "")
    if s.lstrip().startswith(("=", "+", "-", "@", "\t", "\r")):
        s = "'" + s
    return s


def csv_response(rows: list[list], filename: str):
    f = io.StringIO()
    writer = csv.writer(f)
    for row in rows:
        writer.writerow([csv_protect(c) for c in row])
    result = Response(content="\ufeff" + f.getvalue(), media_type="text/csv; charset=utf-8")
    result.headers["Content-Disposition"] = f'attachment; filename="{filename}"'
    return result


@app.get("/api/export/employees")
def export_employees(request: Request):
    with SessionLocal() as db:
        user, _ = session_for(db, request)
        staff(user)
        rows = [["Name", "Email", "Department", "Job title", "Annual salary INR", "Status", "Joined on"]]
        rows.extend([[e.name, e.email, e.department, e.job_title, e.annual_salary, e.status, e.joined_on.isoformat()]
                     for e in db.scalars(select(Employee).where(Employee.org_id == user.org_id).limit(20000)).all()])
        record_audit(db, user, "employees_exported", "report", 0)
        db.commit()
        return csv_response(rows, "peopleflow_employees.csv")


@app.get("/api/export/attendance")
def export_attendance(request: Request):
    with SessionLocal() as db:
        user, _ = session_for(db, request)
        staff(user)
        rows = [["Employee", "Date", "Status"]]
        records = db.execute(select(Attendance, Employee.name).join(Employee, Employee.id == Attendance.employee_id)
                    .where(Attendance.org_id == user.org_id, Employee.org_id == user.org_id)
                    .order_by(Attendance.day.desc()).limit(20000)).all()
        rows.extend([[name, a.day.isoformat(), a.status] for a, name in records])
        record_audit(db, user, "attendance_exported", "report", 0)
        db.commit()
        return csv_response(rows, "peopleflow_attendance.csv")


@app.get("/styles.css")
def styles():
    return FileResponse(WEB / "styles.css", media_type="text/css")


@app.get("/app.js")
def script():
    return FileResponse(WEB / "app.js", media_type="application/javascript")


@app.get("/")
def index():
    return FileResponse(WEB / "index.html", media_type="text/html")


@app.get("/{other:path}")
def route_fallback(other: str):
    # Static SPA navigation only, not dynamic files / REST paths.
    if other.startswith("api/") or "." in other:
        fail("Not found", 404)
    return FileResponse(WEB / "index.html", media_type="text/html")
