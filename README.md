# PeopleFlow HRMS

**A full-stack, multi-organization human resource management web application.**

[**Live application**](https://peopleflow-hrms-app.onrender.com) · [**Source code**](https://github.com/gargv1529-boop/peopleflow-hrms) · [**Security & limitations**](SECURITY_AND_LIMITATIONS.md)

> **Portfolio / limited-beta project.** PeopleFlow is publicly deployed and its key workflows have been exercised with test accounts. It is not independently security-audited, and should not be used with real employee or payroll data until further privacy, security, operational and backup requirements are addressed.

## Overview

PeopleFlow HRMS helps a team manage employees, daily attendance, leave applications, user access and basic salary estimates in one browser-based workspace. Unlike a static UI prototype, its frontend communicates with a **FastAPI REST backend** that stores application records in a database. The hosted edition uses **Neon PostgreSQL** and **Render**.

### Features

| Area | Implemented functionality |
|---|---|
| Organization onboarding | Create a workspace and its initial Admin account |
| Authentication | Sign-in/sign-out, password change, database-backed sessions |
| Role-based access | Admin, HR and Employee roles with server-side checks |
| Employees | Create, view, update, archive and reactivate employee records |
| Attendance | Mark daily attendance, review history and export CSV |
| Leave requests | Submit requests and approve/reject them with authorized staff roles |
| Reporting | Dashboard summaries and base-salary estimates (not statutory payroll) |
| Administration | Team user accounts and activity/audit records |
| Data separation | Backend queries scope HR records to their organization |
| Deployment | Dockerized backend with Render Blueprint and external PostgreSQL |

### Roles at a glance

- **Admin:** Manage team accounts and organizational HR operations; access administrative reports and activity logs.
- **HR:** Perform permitted employee, attendance and leave management tasks.
- **Employee:** Use an employee-specific workspace, view permitted records, and submit personal leave requests without approval controls.

The interface hides inappropriate controls, and the API includes server-side authorization checks. **Security is not established by hiding buttons alone**; the deployed backend requires ongoing security tests and review.

## Technology stack

| Layer | Technology |
|---|---|
| Frontend | HTML5, CSS3, JavaScript (responsive UI) |
| Backend/API | Python 3.12, FastAPI, Uvicorn |
| Database / ORM | PostgreSQL (hosted), SQLite (local), SQLAlchemy |
| Authentication | Argon2 password hashing, HttpOnly session cookies, CSRF protection |
| Deployment | Docker, Render Free web service, Neon PostgreSQL |
| Quality | pytest integration tests, GitHub Actions CI, JavaScript syntax check |

## Architecture

```text
Browser (HTML / CSS / JavaScript)
            |
       HTTPS requests
            |
       FastAPI / Uvicorn
            |
   Auth / roles / validation
            |
          SQLAlchemy
            |
  PostgreSQL (Neon, online)
   SQLite (local development)
```

## Run locally (Windows)

**Prerequisites:** Python 3.11+ and an internet connection for the first dependency installation.

```powershell
git clone https://github.com/gargv1529-boop/peopleflow-hrms.git
cd peopleflow-hrms
.\run_windows.bat
```

Or start manually:

```powershell
py -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe -m uvicorn server.main:app --host 127.0.0.1 --port 8765
```

Open **http://127.0.0.1:8765**. Use **Create an organization** to create your own workspace. Local development defaults to SQLite under `data/`; no demo credentials are required.

## Cloud deployment

The live portfolio deployment uses **Render Free** for the Docker web service and **Neon PostgreSQL** for durable database storage. See [`DEPLOY_ONLINE_WINDOWS.md`](DEPLOY_ONLINE_WINDOWS.md) for the expanded setup guide; some instructions in older project versions refer to Render-managed paid PostgreSQL, whereas the live setup uses external Neon.

Render configuration (`render.yaml`) defines the web service and a health check at `/api/health`. Configure the following **privately in the hosting platform**:

| Environment variable | Usage |
|---|---|
| `APP_ENV` | `production` |
| `DATABASE_URL` | Full Neon PostgreSQL connection URL (secret) |
| `PUBLIC_BASE_URL` | Optional override for the public HTTPS URL; otherwise Render supplies its service URL |
| `ALLOWED_HOSTS` | Optional approved-host override; otherwise Render supplies its service hostname |

**Never commit** database URLs, `.env`, local database files, session tokens, or employee records. Free hosting may pause when idle and have usage limits. Production availability and backups are not guaranteed by this portfolio configuration.

## Tests

Run locally after installing test dependencies:

```bash
python -m pip install -r requirements-dev.txt
python -m pytest -q tests/
node --check web/app.js
```

The included integration tests exercise organization registration and isolation, authentication/authorization, CSRF enforcement, employee data, attendance, leave decisions, exports and record persistence. The local API test suite was checked during the portfolio README update (**2 tests passed**). A passing test suite does not replace a live security review.

### Live workflow checks performed

- Admin and Employee accounts signed into the deployed application.
- Employee records and dashboard totals displayed correctly.
- Attendance records saved and reappeared after a subsequent login.
- Leave requests could be submitted, approved or rejected; records remained after signing back in.
- The Employee interface did not display Admin-only navigation and leave-approval buttons.

**Not yet verified to production standards:** adversarial API authorization testing against the live deployment, penetration testing, email verification/password recovery, automated backup restores, monitoring and privacy compliance.

## Project structure

```text
peopleflow-hrms/
├── server/main.py                 # FastAPI backend and SQLAlchemy models
├── web/                           # Browser frontend
├── tests/test_api.py              # API integration tests
├── .github/workflows/ci.yml       # Continuous integration
├── Dockerfile                     # Container build
├── render.yaml                    # Render deployment settings
├── requirements.txt               # Runtime dependencies
├── requirements-dev.txt           # Development/test dependencies
├── run_windows.bat                # Windows local launcher
├── legacy_cpp/                    # Prior C++ implementation
├── src/                           # Earlier C++ project sources
├── docs/                          # Earlier static website (GitHub Pages)
├── SECURITY_AND_LIMITATIONS.md    # Known limitations and safeguards
└── README.md
```

## Illustrations / screenshots

The deployed interface includes an overview dashboard, employee records, attendance, leave approvals and user accounts. Before adding public screenshots, **use fictional employee details and remove names, email addresses, salary figures and organization details from real users**. Add reviewed screenshots to `screenshots/` and link them here.

## Background and project scope

PeopleFlow began as a C++/SQLite project and was expanded into a web-based backend and responsive frontend. The older implementation remains in the repository for reference; **the hosted production-mode web service runs FastAPI**, not the legacy C++ server. Estimated payroll values are for demonstration and do **not** calculate statutory deductions, payslips, or tax compliance.

## Responsible use

Please see [`SECURITY_AND_LIMITATIONS.md`](SECURITY_AND_LIMITATIONS.md) for known gaps and recommended safeguards before use with sensitive HR records. This is a portfolio engineering project and a learning implementation, not a production-certified HR/payroll product.
