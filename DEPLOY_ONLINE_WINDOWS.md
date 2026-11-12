# Deploy PeopleFlow HRMS — Render Free + Neon PostgreSQL

This guide documents the **current portfolio deployment**. It is not a guarantee that every hosting feature will remain free indefinitely. Review Render and Neon usage limits before creating resources.

## 1. GitHub

Push the PeopleFlow source to `https://github.com/gargv1529-boop/peopleflow-hrms`. Keep `.env`, `.venv/`, `data/*.db`, and database credentials out of commits.

## 2. Neon database

Create a PostgreSQL database in [Neon](https://console.neon.tech/) on a plan you're comfortable with. In the project dashboard, click **Connect** and copy the PostgreSQL connection URL. **Do not share it**: it contains your password. The URL often starts with `postgresql://` and has SSL parameters.

## 3. Render service

In [Render](https://dashboard.render.com/), connect the GitHub repository through **New → Blueprint**. Keep `main` and `render.yaml` (repository root). The current free-hosting Blueprint should create **one Docker web service** named `peopleflow-hrms-app` and **no separate Render database**.

Before deploying, check that the Blueprint says `plan: free` and no paid resources are being created. If it asks for `DATABASE_URL`, paste the Neon URL in Render's private value field, not in the repository. For a service already created, use **PeopleFlow web service → Environment → Edit**.

Set `APP_ENV=production`; the application reads the Render-provided public hostname automatically. Custom domain users may need `PUBLIC_BASE_URL` and `ALLOWED_HOSTS` overrides.

## 4. Validate

Deploy and inspect **Deploys / Logs**. A healthy startup includes a successful `GET /api/health` request. The portfolio application's public URL is:

https://peopleflow-hrms-app.onrender.com

If deployment fails, look at the **final** exception line in the logs. A message about a durable PostgreSQL URL indicates that `DATABASE_URL` is missing, malformed, or has been populated with placeholder text.

## 5. Check actual workflows

Using **fictional records only**, create an organization and Admin account, add employees, mark attendance, request/approve leave, sign out/in and confirm records remain. Test with a separate Employee account. Hiding an Admin button is not proof of API security.

## Free-tier caveats

Render Free may spin down when idle and start slowly. Database quotas, retention and backup coverage are limited and can change. The portfolio setup is not suitable for real payroll data or high-availability business use without further security, privacy and operational controls.
