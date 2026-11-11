# PeopleFlow HRMS — Real Full-Stack Website

**A working multi-organization HR management web application, not a static GitHub Pages demo.**

> Stack: **FastAPI (Python), SQLAlchemy, PostgreSQL in the cloud / SQLite locally, Argon2 passwords, HTML, CSS, JavaScript, Docker**. This replaces the earlier C++ localhost-only backend for the internet-facing service. The old C++ source is retained under `legacy_cpp/` for your portfolio history.

## What is real in this edition?

- **No demo users or hardcoded passwords**. A visitor creates a new organization and Admin account.
- **Real sign-in, server sessions and password changes**. Password hashes use Argon2id; login session tokens are random, stored only as hashes in the database, and sent through HttpOnly cookies.
- **Persistent database**. PostgreSQL is required for cloud production; SQLite works for individual local development.
- **Separate organization data**. Employee, attendance, leave, audit, user account, dashboard and export requests are filtered by the signed-in user's organization at the backend.
- **Admin, HR, Employee roles**, with real HTTP authorization checks, CSRF tokens, and validation.
- **Employee management**, including archive/reactivate to preserve leave/attendance history.
- **Daily attendance**, **leave requests and approvals**, **payroll estimates (not statutory payroll)**, **account creation**, **audit trail**, and **CSV exports**.
- **Responsive frontend** based on the existing PeopleFlow look, but connected to the real server rather than a simulated browser session.
- **Docker + Render deployment**, with a prepared Render Blueprint (`render.yaml`) that provisions the web service and PostgreSQL, and HTTPS at the platform's edge.

**Important:** This is an operational full-stack application suitable for deploying as a **portfolio/limited beta**. It is **not independently security-audited** or ready to process real staff identity, payroll, or other sensitive HR data at scale. Production use still requires security testing, email verification and recovery, organization policies, documented privacy practices, backups, migrations, monitoring, and incident response. See `SECURITY_AND_LIMITATIONS.md`.

## Windows — run it on your laptop

1. Install **Python 3.11+** from <https://www.python.org/downloads/> and enable **Add Python to PATH**. You already installed MSYS2 for the older C++ version, but **this new cloud-ready backend uses Python** to handle web requests.
2. Download and extract the `PeopleFlow_HRMS_REAL_Website.zip` file to a normal folder (for example Desktop).
3. Double-click **`run_windows.bat`**. The script creates a private Python environment, installs its dependencies, and starts the server. The first run needs an internet connection.
4. Visit **<http://127.0.0.1:8765>** in Chrome.
5. Click **Create an organization**. Enter your organization name, your name, work email and a strong password of at least 12 characters.
6. Your dashboard starts **empty**, because this is not a seeded demo. Click **Employees → Add employee** to store your first record. Admin can also create new team accounts under **User accounts**.
7. Keep the terminal open while using the local version. Data survives restarts in `data/peopleflow_real.db`.

To run from Windows PowerShell instead of the batch file:

```powershell
cd "C:\path\to\PeopleFlow_HRMS_REAL_Website"
py -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe -m uvicorn server.main:app --host 127.0.0.1 --port 8765
```

Open <http://127.0.0.1:8765>. If port 8765 is in use, change `--port 8765` to a free port and visit that port.

## Publish online — easiest route

**Read [`DEPLOY_ONLINE_WINDOWS.md`](DEPLOY_ONLINE_WINDOWS.md)** for the exact GitHub Desktop → Render Blueprint steps, without needing Git commands. The project also includes `render.yaml` to link the FastAPI web service to PostgreSQL automatically. **Review hosting charges:** the default Blueprint uses paid web and database plans.

Render supplies `RENDER_EXTERNAL_URL` and `RENDER_EXTERNAL_HOSTNAME`, and this project now reads those automatically for production security checks. You can override them with `PUBLIC_BASE_URL` / `ALLOWED_HOSTS` when attaching a custom domain.

## How to put your **real website** online (Render + PostgreSQL)

GitHub Pages **cannot** host a Python/C++ server or a private shared database. **Your existing `github.io` page is a separate static demonstration, and it will not become this real website by copying HTML alone.**

Use the full project as a **new GitHub repository** (recommended name: `peopleflow-real-hrms`) so you don't accidentally overwrite the existing GitHub Pages demo or the earlier C++ project. The code can still be linked from your original repository and profile.

1. Sign in to <https://github.com/> and create a **new empty repository** (no generated README). Upload **all files from this extracted folder** while keeping directories intact, except local `.venv/`, `data/*.db`, `.env` and secrets. Alternatively use the Git commands below.
2. Sign in to <https://render.com/>. Create a **Postgres** database in a region close to your web service. Ensure the plan and backup settings meet your needs. Copy its **internal connection URL** from the dashboard; treat it like a password.
3. Click **New → Web Service → connect GitHub repository**. Choose your new repository. Set **Language/Runtime: Docker**, **Dockerfile Path: `./Dockerfile`** and your desired plan/region. You can leave the Docker command at its default.
4. In the web service's **Environment** settings, add:

   | Variable | Example / guidance |
   | --- | --- |
   | `APP_ENV` | `production` |
   | `DATABASE_URL` | Your Render Postgres **internal** database URL (secret; never commit) |
   | `PUBLIC_BASE_URL` | `https://YOUR-SERVICE.onrender.com` |
   | `ALLOWED_HOSTS` | `YOUR-SERVICE.onrender.com` |

   Replace **YOUR-SERVICE** with the actual service hostname allocated by Render. `PUBLIC_BASE_URL` must have HTTPS and **no trailing slash**. `ALLOWED_HOSTS` is just the hostname, without `https://`. On a custom domain, list both approved hostnames separated by commas and set `PUBLIC_BASE_URL` to your primary actual site origin.
5. Deploy the service. Render builds your Dockerfile, runs the server listening on the required port, and gives it a public HTTPS URL. Open the site's `/api/health` endpoint to confirm `{"ok":true,"data":{"status":"ok"}}`.
6. Open the public homepage. Register a **new organization**. Records will be stored in Render PostgreSQL and accessible from other devices when you log into the same account.
7. For a domain like `hrms.yourdomain.com`, configure Render **Custom Domains**, verify DNS, update `PUBLIC_BASE_URL` and `ALLOWED_HOSTS`, and redeploy.

**Hosting is not free by default.** Confirm current pricing, data retention, database backups and uptime of your selected Render plans. Never use the temporary filesystem on a web container to hold the live database. Render's PostgreSQL service is a separate persistent database.

### Git commands (Windows Git Bash / VS Code terminal)

```bash
git init
git add .
git commit -m "PeopleFlow real full-stack HRMS"
git branch -M main
git remote add origin https://github.com/YOUR-USERNAME/peopleflow-real-hrms.git
git push -u origin main
```

Replace `YOUR-USERNAME` and make the repository on GitHub first. If `origin` is already configured, do not add it again. Never push database files or `.env` secrets.

### Testing the cloud website

Use two separate browsers/private windows to create two **different organizations** and confirm their employee, attendance, leave and account lists stay separate. Change an employee record in one browser, reload, and verify it remains. Close the browser, sign back in, and ensure the records persist. Test Admin, HR and Employee restrictions before inviting others.

## Development, testing and source tree

```bash
python -m pip install -r requirements-dev.txt
pytest -q tests/
node --check web/app.js
```

```text
PeopleFlow_HRMS_REAL_Website/
├── server/main.py             # Real backend, SQLAlchemy schema, sessions, role checks, REST endpoints
├── web/
│   ├── index.html             # Actual login / registration / workspace page
│   ├── styles.css             # Responsive design
│   └── app.js                 # REST client, actions and dashboard
├── tests/test_api.py          # HTTP integration tests and cross-organization isolation
├── legacy_cpp/peopleflow_server.cpp  # Prior local C++ server (not deployed)
├── data/.gitkeep              # Local database goes here, excluded from git
├── Dockerfile                 # Cloud build
├── requirements.txt           # Backend dependencies
├── requirements-dev.txt       # Test dependencies
├── run_windows.bat            # Windows launcher
├── .github/workflows/ci.yml   # Build checks on GitHub
├── .env.example               # Environment variable reference (not secrets)
├── SECURITY_AND_LIMITATIONS.md
└── README.md
```

## API

- Account: `POST /api/register`, `POST /api/login`, `GET /api/me`, `POST /api/logout`, `POST /api/password`
- Dashboard: `GET /api/dashboard`
- Employees: `GET/POST /api/employees`, `PUT/DELETE /api/employees/{id}` (`DELETE` archives)
- Attendance: `GET/POST /api/attendance`
- Leave: `GET/POST /api/leaves`, `PUT /api/leaves/{id}`
- Users and audit: `GET/POST /api/users` (Admin only), `GET /api/audit` (Admin only)
- Reports: `GET /api/payroll`, `GET /api/export/employees`, `GET /api/export/attendance`
- Uptime/database check: `GET /api/health`

All POST/PUT/DELETE requests (except initial register/login) require an `X-CSRF-Token` header supplied by `/api/me`. All logged-in resources use an HttpOnly session cookie. HTML/frontend is never trusted for access control; backend checks roles and `org_id`.

## From the prior PeopleFlow project

Your old Windows C++ program can continue running locally on port 8765, but **don't run the old and new servers on the same port simultaneously**. Quit the old black terminal before launching `run_windows.bat`.

This new edition **does not automatically read or overwrite the old `peopleflow_v2.db`**, and does not migrate its sample login passwords. Start with a fresh account and your own fictional test organization. If you want to move your existing records later, build a reviewed importer with explicit mapping; don't paste DB files into the public GitHub repo.

## Old GitHub Pages demo

This real application uses its own FastAPI/PostgreSQL service, not GitHub Pages. If you no longer want the old browser-only demo published, wait until the public Render site works and then follow Part D of `DEPLOY_ONLINE_WINDOWS.md` to unpublish the GitHub Pages site without deleting your original source repository.

## Portfolio description

**PeopleFlow HRMS — Multi-Organization Full-Stack Web App** | FastAPI, SQLAlchemy, PostgreSQL, Argon2, JavaScript, Docker

Built an organization-isolated HRMS with company registration, authenticated role-based workspaces, persistent employee data, attendance and leave workflows, salary estimates, audit logs, CSV exports, security controls, automated API tests, and Docker-based cloud deployment.

**Note:** The project can be deployed online using your own GitHub and hosting accounts. **Creating this source package does not automatically publish a live public URL**. You must complete the hosting deployment steps.
