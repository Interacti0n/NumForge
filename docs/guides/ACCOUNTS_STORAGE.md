# Optional account database

Local development foundation only: Django 5.2 LTS, Psycopg 3 and PostgreSQL 14+.
The custom identity and framework auth/session tables have migrations. There are
no public account routes, login UI, saved workspaces or C-session bridge yet.
CMake, the mathematical libraries and the existing guest web need none of these.
For the separate container deployment, see [Docker Compose](CONTAINERS.md).

## Local setup

Use Python 3.12 (locally verified) and PostgreSQL binaries from the
[official Windows download page](https://www.postgresql.org/download/windows/).
Verify the distributor's checksum before extracting. Put `pgsql/` under
`.local/accounts/`, or set `NF_POSTGRES_BIN` to an existing PostgreSQL `bin` folder.
On Linux, install PostgreSQL from your distribution and set that variable.

From the repository root (PowerShell):

```powershell
python -m venv .local/accounts/venv
$accountPython = '.local/accounts/venv/Scripts/python.exe'
& $accountPython -m pip install -r apps/accounts/requirements.txt
& $accountPython scripts/accounts_db.py init
& $accountPython scripts/accounts_db.py status
```

On Linux use `.local/accounts/venv/bin/python`. `init` refuses existing state;
it creates a dedicated cluster on **127.0.0.1:55432**, SCRAM passwords, 20 maximum
connections and 64 MiB shared buffers. It does not install a Windows service,
scheduled task, firewall rule or public endpoint. After PC restart, use `start`;
use `stop` for a clean shutdown. Existing preview startup remains separate.

`.local/` is ignored by Git and is **persistent data, not a disposable build**.
The helper restricts its account directory to the current OS user (plus Windows
SYSTEM/Administrators). Generated `app.json`, `migrate.json`, `admin.json` contain
secrets: never commit, print or publish them. Application credentials allow table
CRUD only; migrations own the schema, and cluster administration stays separate.
For deployment, give each process only its own configuration file.

```powershell
$env:NF_ACCOUNTS_CONFIG = Join-Path $PWD '.local/accounts/app.json'
& $accountPython apps/accounts/manage.py check
# Schema changes use the separate migration credentials:
& $accountPython scripts/accounts_db.py migrate
& $accountPython tests/accounts/check_database.py
```

Checks create/drop randomly named disposable databases: schema restrictions,
password hashing, identity uniqueness, session storage, closed HTTP routes,
reverse/forward migrations and full-row backup restoration. They do not create
real users in the account database. Cluster admin configuration is required.

## Backup and restore check

With writes stopped during verification:

```powershell
& $accountPython scripts/accounts_db.py backup
& $accountPython scripts/accounts_db.py verify-backup --file .local/accounts/backups/FILE.dump
```

Backups use `pg_dump` custom format. Verification restores into a new temporary
database, compares every public table/row and drops only that temporary database.
It never overwrites the source. Restore only trusted dumps; use matching/newer
client tools. Keep backup/config copies protected on a separate device: a dump
alone does not include cluster roles, passwords or the Django signing key.
These are manual local backups, without automatic scheduling/retention yet.

Next: private login/logout, CSRF/login throttling and measured Argon2id, then
owner-scoped preferences/workspaces. See the
[account design](../design/APPLICATION_DESIGN.md#proposed-accounts-and-saved-work).
