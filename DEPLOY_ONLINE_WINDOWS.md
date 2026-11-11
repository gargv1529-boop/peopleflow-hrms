# Publish PeopleFlow HRMS on GitHub + Render

These steps are for **Windows**. No MSYS2 or C++ compiler is needed for cloud deployment.

## Before publishing

1. Extract `PeopleFlow_HRMS_Online_Ready.zip`.
2. Open the folder `PeopleFlow_HRMS_Online_Ready` (the one containing `render.yaml`, `Dockerfile`, `server` and `web`).
3. **Never publish** `.venv/`, `data/*.db`, `.env`, passwords, `DATABASE_URL`, real employee data, or a screenshot containing private records. Those are excluded from this ZIP.
4. Keep the existing local application and database as a backup. This is a **separate cloud database**; data you entered at `127.0.0.1:8765` is not automatically uploaded.

## Part A — Put the new real app on GitHub using GitHub Desktop

1. Open GitHub Desktop (already present in your Windows screenshots) and sign in to GitHub.
2. Select **File → Add local repository... → Choose...** and select the extracted `PeopleFlow_HRMS_Online_Ready` folder.
3. If GitHub Desktop says that folder is not a repository, choose **Create a Repository Here**. Set repository name to `peopleflow-hrms-fullstack` and create it.
4. Write the commit summary `Publish real PeopleFlow HRMS full-stack app` and choose **Commit to main**.
5. Click **Publish repository**. Choose a suitable public/private setting (public for a portfolio; only fictional data and code). Do not check in any secret files. Click Publish.
6. Open your new repo, check `server/main.py`, `web/index.html`, `Dockerfile`, `render.yaml`, and `README.md` exist. Confirm that `.env` and database files are NOT shown.

**Alternative using Git Bash:** After creating an empty GitHub repo, open Git Bash inside the extracted project folder and run:

```bash
git init
git add .
git status
git commit -m "Publish real PeopleFlow HRMS full-stack app"
git branch -M main
git remote add origin https://github.com/YOUR_GITHUB_USERNAME/peopleflow-hrms-fullstack.git
git push -u origin main
```

Use your real GitHub username. `git status` must not show database or `.env` files. If you use an existing Git repository, do not reinitialize it or overwrite its history; use GitHub Desktop to commit the changes normally.

## Part B — Make the site public on Render

The `render.yaml` in this project provisions a FastAPI Docker web service and a private managed PostgreSQL instance. **Both default plans are paid**. Inspect the invoice/estimated charges before you approve anything. The plans were chosen to avoid free database expiry and service sleeping. If you're only exploring with fictional data, you can manually choose free tiers, but the database expires after 30 days and a free web service may spin down.

1. Open https://dashboard.render.com/ and sign in.
2. Choose **New → Blueprint** (sometimes labelled **New Blueprint Instance**), authorize GitHub access, and select your `peopleflow-hrms-fullstack` repository.
3. Render should detect `render.yaml`, propose the Docker web service `peopleflow-hrms-app` and Postgres `peopleflow-hrms-postgres`, and link `DATABASE_URL` automatically using the database's internal connection string.
4. Check your chosen **plan, billing, region and database options**. Choose the same region for the app and database. Set a region appropriate to you before approving if desired (edit `render.yaml` to add the supported region string for both).
5. Approve/Apply the Blueprint. Render builds the container, creates the DB and starts your real backend. Wait for the deploy status to show **Live**. Do not share credentials or private database connection strings.
6. Open the **actual HTTPS URL** shown in Render for your web service; it will be a `*.onrender.com` address. Never invent the URL from the repository name.
7. Visit `<your-url>/api/health`. Expected response: `{"ok":true,"data":{"status":"ok"}}`.
8. Go to your site homepage and register a **new, fictional testing organization**. Test login, employee creation, attendance, leave approvals, reload and logout; also test that separate organizations cannot see each other's data.

### Custom domain

Render provides an HTTPS subdomain automatically. To use your own purchased domain, configure **Custom Domains**, update DNS, and set these two environment variables on the web service (no quotes):

```
PUBLIC_BASE_URL=https://hrms.yourdomain.com
ALLOWED_HOSTS=hrms.yourdomain.com,peopleflow-hrms-app.onrender.com
```

Replace the Render hostname with your **actual** one. Render's public URL/hostname are read automatically when you don't set these values. The `PUBLIC_BASE_URL` should match the origin users visit when submitting forms. With custom domains, use a consistent canonical domain; verify registration and sign-in again after DNS changes.

## Part C — Test before sharing with recruiters

- [ ] Hosted site loads over HTTPS and `/api/health` is healthy
- [ ] Registration creates a new organization with zero initial employees
- [ ] User password is never displayed in logs or repository
- [ ] Employee create/edit/archive saves and survives logout and service restart
- [ ] Attendance, leave decisions and audit entries persist
- [ ] Admin/HR/Employee roles have distinct server-side permissions
- [ ] Cross-organization records are not accessible
- [ ] UI works on mobile and desktop, including errors and empty states
- [ ] Test pipeline passes in GitHub Actions
- [ ] Public README includes the real URL, screenshots using fictional data, and clear limitations

## Important limitation

A published beta is **not the same as a production HR platform**. For real customers' personal or salary data, arrange a security review, email verification and recovery, durable backups, privacy/terms, incident response, account deletion, migration support, and appropriate data-protection compliance. See `SECURITY_AND_LIMITATIONS.md`.

Sources: https://render.com/docs/blueprint-spec , https://render.com/docs/environment-variables , https://render.com/docs/free , https://docs.github.com/en/desktop/adding-and-cloning-repositories/adding-an-existing-project-to-github-using-github-desktop

## Part D — Retire the OLD GitHub Pages demo (only after the new site is live)

Your previous demo was served from `https://gargv1529-boop.github.io/peopleflow-hrms/`. Publishing the new site does **not** automatically replace this old static page. After your Render deployment works and the portfolio link points to the new URL:

1. Go to https://github.com/gargv1529-boop/peopleflow-hrms .
2. In the existing repository find its **GitHub Pages** deployment (or use **Settings → Pages**).
3. Use the Pages menu **Unpublish site** to remove the old demo deployment while preserving your historical source code. If needed, disable branch publishing by setting the Pages publishing branch to **None**.
4. Update your GitHub profile README and resume to point to your **actual Render HTTPS URL**, not `127.0.0.1:8765` and not the old `github.io` preview.

Do this only once your real cloud site works. GitHub's instructions: https://docs.github.com/en/pages/getting-started-with-github-pages/unpublishing-a-github-pages-site
