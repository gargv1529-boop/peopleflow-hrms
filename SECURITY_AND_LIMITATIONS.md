# PeopleFlow HRMS — Security, Privacy and Limitations

**Scope:** Publicly deployed full-stack **portfolio / limited-beta application**, not independently security-audited enterprise HR software. Use only fictional or explicitly consented non-sensitive test records until adequate safeguards are in place.

## Currently implemented in the source

- Argon2 password hashes, database-backed session tokens in HttpOnly cookies, and CSRF checks on write operations.
- Admin / HR / Employee roles with server-side restrictions and organization-scoped queries.
- Input validation and login throttling; API integration tests covering important auth and isolation scenarios.
- HTTPS endpoint supplied by Render and a PostgreSQL connection managed separately in Neon.

These controls have not been independently penetration-tested on the public deployment.

## Work required before real employee data

1. **Security:** Review API authorization by role and organization, login abuse, password recovery, email verification, MFA, CSRF/session handling, dependency vulnerabilities and browser security headers. Perform a professional security test.
2. **Data protection:** Document privacy notice, retention/deletion process, data access rights, employee consent/legal basis and applicable regulations. Avoid logging personal or secret data.
3. **Database:** Verify Neon retention, configure backups/restore plans and test actual restoration. Do not assume Render's temporary disk is persistent.
4. **Operations:** Add monitoring, alerts, error reporting, incident response procedures and an upgrade/rollback plan.
5. **Migrations:** Replace first-run `create_all` schema management with a controlled migration workflow (e.g. Alembic) before changing a persistent production schema.
6. **User lifecycle:** Implement secure invitation and password-reset flows, access revocation and account disable/role-change processes.
7. **Payroll:** The current payroll view is a **base-salary estimate**, not validated payroll processing. It does not prepare payslips or statutory PF/ESI, TDS, tax filings or payroll remittances.
8. **Performance:** Free infrastructure may sleep, have usage restrictions, or become unavailable; it is not a high-availability production configuration.

## Secrets and publishing

Never commit `.env`, PostgreSQL connection URLs, login credentials, HR records, local database files or unredacted screenshots containing private data. Set secrets through Render Environment. Do not publicly share screenshots of Neon connection strings or the Render variable values.
