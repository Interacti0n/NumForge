# Optional account database

Local development foundation only: Django 5.2 LTS, Psycopg 3 and PostgreSQL 14+.
Identity, session and login-throttle tables have migrations. Private login/logout
API and SK/EN form tests pass through an optional HTTPS gateway. Workspace storage
and the authenticated C-session bridge remain pending.
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

## Private pilot authentication

The private Django service is reached through the optional
[HTTPS gateway](CONTAINERS.md#https-account-pilot); its shared token authenticates
forwarded scheme/client address. No public registration.

| Route | Contract |
| --- | --- |
| `GET /api/account/session` | Own `{user: {id, username}}` or `user: null`, plus masked `csrf_token` |
| `POST /api/account/login` | JSON `username`, `password`, optional boolean `remember`; returns own user and rotated token |
| `POST /api/account/logout` | Revokes current session; returns null user and a new CSRF token |
| `GET /api/account/preferences` | Own `{user, values, revision}`; unsaved defaults have revision 0 |
| `POST /api/account/preferences` | Saves complete `values` with `user_id` and `expected_revision`; returns the next revision |

POST requires the CSRF cookie plus `X-CSRFToken` and a same-origin HTTPS request.
Session/CSRF cookies are Secure/HttpOnly/SameSite=Lax; responses prohibit caching.
Passwords use Django Argon2id (100 MiB, 2 passes, parallelism 8); a 7-sample local
check measured about 58 ms hashing/60 ms verifying, not a concurrency capacity claim.
Default login is browser-session-only with 12-hour server expiry; remembered login
lasts 14 days. Password changes/deactivation invalidate authentication on subsequent
requests. Other-device management remains pending.

The WSGI service reuses each thread's PostgreSQL connection; requests replace
connections older than 60 seconds and check their health before DB access. Recovery
does not require restarting accounts; an outage still fails account operations.

All well-formed attempts share a PostgreSQL limit of 10 per source IP/5 minutes,
including successes, across workers and restarts. Only token-authenticated gateway
requests may supply a client address; arbitrary forwarding headers remain untrusted.
Errors: 400 malformed JSON/fields, 401 generic invalid credentials (including inactive
users), 403 CSRF/origin, 405 method, 413 oversized body, 415 content type, 429 with
`Retry-After`. Limit: 4 KiB JSON, username 150 characters, password 1024 characters.

Provision a regular user interactively with `docker compose exec accounts python
manage.py createpilotuser NAME` (or the native `manage.py` with `app.json`). Passwords
are prompted twice, validated and never passed as CLI arguments. Expired sessions
need periodic `manage.py clearsessions`; the [pilot supervisor](CONTAINERS.md#https-account-pilot)
runs it daily.

### Account defaults

The account screen explicitly saves language (`sk/en`), precision mode
(`auto/custom/full`), decimal places (integer 0–10000), notation
(`auto/plain/scientific/math/fraction`), angle (`rad/deg`) and complex form
(`cartesian/trig/exp`). `rounding` is currently fixed to `half_even`, matching
the web evaluator. Every save supplies all seven fields; unknown fields fail.

Preferences require authentication (401), HTTPS/CSRF and JSON within 4 KiB.
Ownership comes from the session; `user_id` only guards against an account switch.
An old revision returns 409 `revision_conflict` with the current own values;
an account mismatch returns 409 `account_changed` without another user's data.
The owner row lock also serializes simultaneous first saves.

Example save body (use the CSRF token/cookie from `/api/account/session`):

```json
{"user_id":7,"expected_revision":0,"values":{
  "language":"sk","precision_mode":"custom","precision":100,
  "notation":"math","angle":"rad","rounding":"half_even","complex_form":"cartesian"
}}
```

Saved defaults apply when opening the calculator; edits in its toolbar remain
local to that task. Restoring defaults does not evaluate an expression. An explicit
URL `lang` overrides the saved language. Guest defaults stay separate in local
storage and are imported only by **Use guest settings**, followed by **Save**.
Account defaults never replace shared guest preferences. Local task snapshots
still retain calculation context; their account-derived controls are skipped on
restore. Sign-out/account changes reset the controls to guest defaults.
Conflicts/offline saves retain the account form's draft; loading
the saved version explicitly discards it. Calculations/history are not synced.

Next: owner-scoped workspaces and device management. See the
[account design](../design/APPLICATION_DESIGN.md#proposed-accounts-and-saved-work).
