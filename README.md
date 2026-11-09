<div align="center">

# PeopleFlow HRMS

### Human Resource Management System | B.Tech Portfolio Project

**An interactive HR workspace for employee records, attendance, leave requests, and role-based workflows.**

[![Live Demo](https://img.shields.io/badge/Live%20Demo-GitHub%20Pages-0d9488?style=for-the-badge)](https://gargv1529-boop.github.io/peopleflow-hrms/)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![SQLite](https://img.shields.io/badge/Database-SQLite-003B57?style=for-the-badge&logo=sqlite&logoColor=white)](https://www.sqlite.org/)

**[Explore Live Demo](https://gargv1529-boop.github.io/peopleflow-hrms/) · [GitHub Repository](https://github.com/gargv1529-boop/peopleflow-hrms)**

</div>

---

## Overview

**PeopleFlow HRMS** is a human resource management project that brings employee records, attendance, leave workflows, and workforce insights into one responsive dashboard. It was developed as a learning and portfolio project using **C++17, SQLite, HTML, CSS, and JavaScript**.

The project has **two distinct versions**:

- **Public web demo (this repository):** An interactive, browser-only preview published through GitHub Pages. It uses fictional employees and simulated Admin, HR, and Employee accounts. Changes are saved only within the visitor's browser session, not in a shared database.
- **Local full-stack application (companion project):** A C++17 web server and SQLite-backed application with backend role checks, authentication, employee management, attendance, and leave workflows. **The backend source is not yet included in this public repository**; it runs separately on the developer's computer.

> **Important:** The GitHub Pages demo is a functional *front-end simulation*, not a deployed C++/SQLite backend or production authentication system.

## Features

| Module | Capabilities |
| --- | --- |
| **Dashboard** | Employee count, department distribution, payroll estimates, and pending leave summaries |
| **Employee directory** | View, search, add, edit, and remove employee records in the interactive demo |
| **Attendance** | Mark attendance and inspect attendance information |
| **Leave management** | Submit requests and explore approval/rejection workflows |
| **Role-based experience** | Separate Admin, HR, and Employee views and permitted actions in the demo |
| **Responsive design** | Browser-based interface for desktop and smaller screens |
| **Local database application** | SQLite persistence and C++ server-side access checks in the separate local version |

## Technology Stack

| Layer | Technology | Purpose |
| --- | --- | --- |
| Core application | **C++17** | Backend and HRMS business logic in the local version |
| Database | **SQLite** | Persistent local employee, attendance, leave, and account records |
| Local authentication | **OpenSSL + C++** | Password hashing and session-based authentication in the local version |
| Frontend | **HTML5, CSS3, JavaScript** | Responsive login page, dashboard, forms, and interactive views |
| Public hosting | **GitHub Pages** | Hosting the static, browser-only portfolio preview |
| Version control | **Git and GitHub** | Code management and portfolio presentation |

## Live Demo

**Website:** https://gargv1529-boop.github.io/peopleflow-hrms/

Use any of these **public demo-only** accounts:

| Role | Email | Password |
| --- | --- | --- |
| Admin | `admin@peopleflow.demo` | `Demo@123` |
| HR | `hr@peopleflow.demo` | `Demo@123` |
| Employee | `employee@peopleflow.demo` | `Demo@123` |

**Suggested walkthrough:** Sign in as Admin → explore the Overview and Employees pages → try the Attendance and Leave requests screens → sign out and compare the Employee view.

The preview starts with **five fictional employee records**. Data modifications are simulated within the browser session and do not affect other visitors or the developer's SQLite database. Demo credentials are public by design; **never reuse them for real accounts**.

## Screenshots

The interface includes a login screen, department and payroll dashboard, employee directory, attendance records, and leave-request views.

**To add screenshots to this README:** Capture the public demo using fictional records, save images to `docs/screenshots/` (for example, `login.png`, `dashboard.png`, and `employees.png`), then add the following Markdown after uploading them:

```markdown
![PeopleFlow login screen](docs/screenshots/login.png)
![PeopleFlow HR dashboard](docs/screenshots/dashboard.png)
![Employee directory](docs/screenshots/employees.png)
```

## Run the Public Demo Locally

The files in `docs/` are static and require **no C++ compiler or SQLite database**.

1. Clone the repository:

   ```bash
   git clone https://github.com/gargv1529-boop/peopleflow-hrms.git
   cd peopleflow-hrms
   ```

2. If Python is installed, start a local static server:

   ```bash
   python -m http.server 8000 --directory docs
   ```

   On Windows, `py -m http.server 8000 --directory docs` may be used instead.

3. Open **http://localhost:8000** in your browser and sign in with a demo account above.

You can also use the hosted **[GitHub Pages demo](https://gargv1529-boop.github.io/peopleflow-hrms/)** without downloading anything.

## Repository Structure

```text
peopleflow-hrms/
├── docs/
│   ├── index.html        # Public demo page and interface markup
│   ├── styles.css        # Layout, colors, and responsive design
│   ├── app.js            # Browser-based demo logic and fictional data
│   └── .nojekyll         # GitHub Pages static-site configuration
├── README.md             # Project documentation
├── .gitignore
└── .gitattributes
```

*The local C++ and SQLite source is a companion project and is not part of the repository structure shown above.*

## Architecture

**Public demo**

```text
GitHub Pages (static hosting)
            |
HTML + CSS + JavaScript
            |
Fictional sample data + browser session storage
            |
Admin / HR / Employee simulated workflows
```

**Separate local application**

```text
Web browser
    |
C++17 HTTP application (localhost)
    |
Authentication + role checks + HRMS logic
    |
SQLite database (local persistent storage)
```

The public preview intentionally **does not contact or expose** the local application or database.

## Security & Limitations

- This is an **educational portfolio project**, not production HR or payroll software.
- Demo role switching is simulated in JavaScript. It should **not** be interpreted as real server-enforced authorization.
- Public sample data is fictional. Do not add real employee names, payroll information, passwords, or `.db` files to the repository.
- The local C++ server is intended for `localhost` development. It has not been hardened for internet deployment and should not be exposed directly to the public internet.
- Payroll totals in the dashboard are **illustrative estimates**, not a complete payroll calculation or statutory compliance engine.

## Future Improvements

- Publish the sanitized C++/SQLite backend source with reproducible build instructions and automated tests.
- Add audit logging and CSV/PDF report exports.
- Implement stronger session management, password reset, rate limiting, and deployment hardening.
- Add automated browser tests and CI/CD checks.
- Expand attendance, payroll, and leave-policy capabilities.

## Portfolio Summary

**PeopleFlow HRMS | C++17, SQLite, HTML, CSS, JavaScript**

Built a local HR management application featuring employee records, attendance, leave workflows, and role-based access. Created and published a responsive, interactive GitHub Pages preview using fictional sample data so recruiters can explore the interface without installing the C++ backend.

---

<div align="center">

**[Try the interactive demo](https://gargv1529-boop.github.io/peopleflow-hrms/)**

*Developed as a learning project with AI assistance. The source and behavior should be reviewed and understood before representing the implementation as independent work.*

</div>
