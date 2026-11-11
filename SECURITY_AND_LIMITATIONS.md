# Security and real-world use checklist

PeopleFlow's functionality is **real**: it writes to an actual database and enforces authentication and organization-specific access on the server. It should, however, be deployed initially only as a **portfolio or pilot application with synthetic records** until independently reviewed and operated professionally.

## Controls implemented in source

- Argon2id password hashing, >=12 character passwords, password change.
- Random 12-hour database-backed login sessions; SHA-256 token hashes (the raw token appears only as a browser cookie); HttpOnly and SameSite=Strict cookies; `Secure` cookies on production HTTPS.
- Session-bound CSRF validation for all state-changing operations after login.
- Origin and Host restrictions when configured; HTTPS expected at managed hosting; security headers.
- Server-enforced Admin/HR/Employee role checks and `org_id` restrictions on database queries.
- Input validation, SQLAlchemy parameter binding, HTML escaping in the frontend, CSV injection escaping.
- Basic in-memory per-IP/email login/signup throttling, response no-cache controls.
- Soft archival of employees to retain existing attendance/leave evidence and an administrative audit history.
- Docker runs as an unprivileged Linux user.

## Missing before storing real employee or payroll data

1. **Email verification, password reset, and MFA.** This version requires a registered Admin to create members. It does not send verification or recovery email. Public open signup may invite bot/spam accounts. Add an email service and signed, expiring invite/recovery links before launch for customers.
2. **Stronger abuse prevention.** The request limiter is in memory and per process; it is not shared among replicas or persistent across restarts. Add Redis/database-backed rate limits and monitoring. Add email verification, account lockouts and anti-abuse controls.
3. **Privacy and legal policies.** Establish consent/legal basis, privacy notice, terms, data retention, account export/deletion, staff access policies, applicable Indian DPDP/privacy obligations, breach response procedures and organizational contracts. Do not use real personal data before this review.
4. **Backups and restore exercises.** Use managed Postgres with automated encrypted backups, retention and point-in-time recovery appropriate to the service. Restore-test periodically. A container's local disk is ephemeral.
5. **Schema migrations.** Current first-launch schema is created with SQLAlchemy `create_all`; introduce Alembic migrations before changing tables on existing customer databases.
6. **Third-party security review and pentest.** Test password/CSRF/session failures, access isolation, host spoofing, race conditions, excessive request sizes, login throttling, deployment headers and incident handling in the deployed environment.
7. **Operations.** Add structured logs, application metrics, alerts, capacity planning, uptime SLOs, deployment rollback, automated dependency/security scanning, data-at-rest encryption policy, disaster recovery and observability. One Uvicorn worker is used for the starter deployment.
8. **HR/payroll accuracy.** Pay displayed is a **monthly base-salary estimate only**; it does not compute payslips, PF/ESI, taxes, leave balances, statutory deductions, payroll remittances, or applicable labor-law compliance.
9. **Admin governance.** Add invitations rather than communicating initial plaintext passwords, account disable/revoke and role-change controls, and user lifecycle controls. Global uniqueness of account email means one email cannot join multiple organizations in this starter version.
10. **Auditing policy.** The audit log is an application table editable by a DB administrator, not an immutable or compliance-certified log; define log retention and administrative access.

## Secret handling

- Never put a production `DATABASE_URL`, password, token, export, or database file in GitHub or screenshots.
- Store the database URL only in the cloud service's environment/secrets settings.
- Do not rely on GitHub Pages to run a backend; it hosts static sites only.
- Configure `APP_ENV=production`, `DATABASE_URL`, `PUBLIC_BASE_URL`, `ALLOWED_HOSTS`. Production refuses SQLite/HTTP/unconfigured host values.

## Testing status

API integration tests in `tests/test_api.py` cover registration/login, organization separation, unauthorized and CSRF requests, employee data, attendance upsert, leave approvals/overlaps, role restrictions, password change, logout, export, and database persistence across connections. Python and JavaScript syntax tests passed during packaging. **A full cloud deployment, live PostgreSQL integration test, full interactive browser test, and independent security audit were not performed by the creation environment.**

## Render Blueprint notes

`render.yaml` selects paid web and paid managed PostgreSQL plans; review charges before applying. It does not provide email verification, MFA, retention configuration, or verified offsite restores. On Render, the public URL and host are obtained from `RENDER_EXTERNAL_URL`/`RENDER_EXTERNAL_HOSTNAME` unless explicitly overridden for a custom domain.
