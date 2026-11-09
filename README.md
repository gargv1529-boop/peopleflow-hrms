<div align="center">

# PeopleFlow HRMS

### C++17 · SQLite · Role-Based Access · Web Dashboard

**Human Resource Management System — B.Tech Portfolio Project**

[![Live Demo](https://img.shields.io/badge/Live%20Demo-GitHub%20Pages-0d9488?style=for-the-badge)](https://gargv1529-boop.github.io/peopleflow-hrms/)
[![C++17](https://img.shields.io/badge/Language-C%2B%2B17-00599C?style=for-the-badge)](https://isocpp.org/)
[![SQLite](https://img.shields.io/badge/Database-SQLite-003B57?style=for-the-badge)](https://www.sqlite.org/)

**[Interactive Portfolio Demo](https://gargv1529-boop.github.io/peopleflow-hrms/) · [Source Code](https://github.com/gargv1529-boop/peopleflow-hrms)**

</div>

## Project overview

PeopleFlow HRMS is a learning-focused human resource application demonstrating employee record management, attendance, leave approval, and role-based permissions using a local **C++17 HTTP backend**, a **SQLite database**, and a responsive **HTML/CSS/JavaScript** interface.

This repository includes **two different experiences**:

1. **Public GitHub Pages demo (`docs/`)** — browser-only, interactive simulation with fictional employees and simulated Admin/HR/Employee permissions. It does **not** connect to a live server or SQLite database.
2. **Local full-stack application (`src/`, `web/`)** — C++ server running on `127.0.0.1:8765` with SQLite persistence, server-side authorization, login sessions, and role-specific operations. This is *not* publicly hosted.

> The public demo shows the UI and workflows. The C++ source demonstrates how the local database-backed version is implemented. Neither is intended as production HR software.

## Features

| Area | Local C++ + SQLite application |
| --- | --- |
| Accounts | Admin, HR and Employee login with role checks on the server |
| Employees | Add, view, search, update and remove records |
| Attendance | Record and view attendance; employees see their own records |
| Leave requests | Submit, approve and reject requests with permissions |
| Dashboard | Team headcount, departments, payroll estimates and pending leaves |
| Data persistence | SQLite tables for employees, attendance, leaves and users |
| Passwords | Salted PBKDF2-HMAC-SHA256 hashing using OpenSSL |
| Tests | C++ HRMS core tests run with CTest / GitHub Actions |

## Technology stack

**C++17**, STL, SQLite3, OpenSSL, CMake, GCC / MSYS2, HTML5, CSS3, JavaScript, Git and GitHub Pages.

### Architecture

```text
Local full-stack app
Browser (web/) ──HTTP requests──> C++ server (src/web_server.cpp)
                                       │
                      Authentication, authorization & HRMS rules
                                       │
                                 SQLite database
                              (data/peopleflow.db)

Public portfolio preview
GitHub Pages (docs/) ──> Static HTML/CSS/JS ──> Fictional browser-only data
```

## Try the public demo

**[Open PeopleFlow HRMS](https://gargv1529-boop.github.io/peopleflow-hrms/)**

| Demo role | Email | Password |
| --- | --- | --- |
| Admin | `admin@peopleflow.demo` | `Demo@123` |
| HR | `hr@peopleflow.demo` | `Demo@123` |
| Employee | `employee@peopleflow.demo` | `Demo@123` |

The demo uses fictional records. These are *simulation credentials only*. Changes made in the public demo are local to the browser and do not update any database.

## Run the full C++ application on Windows

1. Install [MSYS2](https://www.msys2.org/) and open **MSYS2 UCRT64**.
2. Install build dependencies:

   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-openssl
   ```

3. Clone this repo and open the **repository root** in VS Code.
4. In a PowerShell terminal, run:

   ```powershell
   .\run_dashboard_windows.bat
   ```

5. Open **http://127.0.0.1:8765** and keep the server terminal running.

The first local launch creates `data/peopleflow.db` with *demo-only* Admin/HR accounts. New installations may have no employees yet; add a fictional employee and create a linked Employee login from the Admin interface. Default demo passwords are `Demo@123` **for local learning only**. Change passwords and do not use real personal records.

The console-only app remains available via `run_windows.bat`. It uses separate legacy text files and is not the same storage as the SQLite dashboard.

### Linux / CMake (developers)

```bash
sudo apt-get install cmake g++ libsqlite3-dev libssl-dev
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/peopleflow_web
```

Launch the server from the repository root because it serves `web/` and stores data in `data/` relative to the working directory.

## Repository structure

```text
peopleflow-hrms/
├── docs/                       # Public simulated GitHub Pages site
│   ├── index.html
│   ├── app.js
│   ├── styles.css
│   └── .nojekyll
├── src/
│   ├── Database.cpp/.hpp       # SQLite, accounts, password hashing, import
│   ├── HRMS.cpp/.hpp           # HRMS domain logic and legacy support
│   ├── web_server.cpp          # Local REST-like API and role enforcement
│   └── main.cpp                # Legacy console app
├── web/                        # Local C++ dashboard frontend
├── tests/test_hrms.cpp         # C++ core tests
├── data/.gitkeep               # Database created locally; never committed
├── .github/workflows/ci.yml    # Build and test automation
├── CMakeLists.txt
├── run_dashboard_windows.bat
├── run_dashboard_unix.sh
├── run_windows.bat
├── run_unix.sh
└── README.md
```

## Role permissions (local application)

| Capability | Admin | HR | Employee |
| --- | :---: | :---: | :---: |
| View all employee records | Yes | Yes | No; own record only |
| Create, edit or delete employee records | Yes | Yes | No |
| Mark attendance | Yes | Yes | No |
| View own attendance | Yes | Yes | Yes |
| Submit leave request | Yes | Yes | Own request only |
| Approve or reject leave | Yes | Yes | No |
| Manage user accounts | Yes | No | No |

## Screenshots

Add screenshots of the **fictional public demo** to `docs/screenshots/` and reference them here:

```markdown
![PeopleFlow login page](docs/screenshots/login.png)
![PeopleFlow dashboard](docs/screenshots/dashboard.png)
![PeopleFlow employee directory](docs/screenshots/employees.png)
```

Do not publish screenshots showing real employee data.

## Security and responsible use

- **Local demo only:** the C++ server binds to loopback (`127.0.0.1`) and is not hardened for internet exposure.
- **Fixed demo credentials** exist for convenient local evaluation; do not deploy this backend unchanged on a public server.
- Never commit `data/peopleflow.db`, `.db-wal`/`.db-shm`, real employee text files, API secrets or private keys. The included `.gitignore` excludes them, but review staged files manually.
- A production deployment needs security review, HTTPS, stronger authentication safeguards, audit logs, backup/recovery controls, input validation hardening and operational monitoring.
- Payroll numbers are illustrative estimates; this is not a statutory payroll system.

## Future enhancements

Email invitations, leave balances, payroll export, audit trails, finer-grained access permissions, more automated integration/browser tests, and production deployment hardening.

## Resume-ready summary

**PeopleFlow HRMS | C++17, SQLite, OpenSSL, HTML, CSS, JavaScript** — Built a local database-backed HR management application featuring employee records, attendance, leave approvals, authentication, and role-based authorization. Published a separate interactive browser demo using GitHub Pages for portfolio review.

---

*Learning project built with AI assistance. Review, understand and personalize the code before presenting it as independently authored.*
