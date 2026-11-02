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
| `GET/POST /api/account/workspaces` | Lists own metadata / creates a named input-only workspace |
| `GET/POST /api/account/workspaces/{uuid}` | Reads own workspace / updates, renames, duplicates or deletes it |

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

### Named workspaces

**Account tasks** in the calculator, graphs, equations and matrices opens manual
save/load/rename/duplicate/delete. Guest work is uploaded only by **Save as new**;
opening restores inputs/settings without calculating. Results, C variables/ans
and history remain outside saved workspaces. Autosave is not implemented.

All responses are private/no-store; writes require the session CSRF token.
Create: `{user_id, name, data}`. Detail POST: `{user_id, action,
expected_revision}` plus `data` for `update`, or `name` for `rename`/`duplicate`;
`delete` has no extra fields. Unknown fields fail. A read/write returns
`{user_id, id, name, tool, revision, updated_at, data}`; delete returns
`{user_id, deleted}`. Lists return `{user_id, items, limits}` without input data.

`data` is `{version:1, tool, inputs}`:

| Tool | Inputs |
| --- | --- |
| `calculator` | `expression` (4096 UTF-8 bytes), `settings` (the seven account-default fields) |
| `graph` | `view:{x:[min,max],y:[min,max]}` as finite coordinate strings, 1–4 `curves:[{expression,color,visible}]`; six-digit hex colors |
| `solve` | `mode`, three coefficient `inputs`, `expression`, two `bounds`, `precision` (text 16–100); existing equation modes |
| `matrices` | `operation`, `a`, `b`; rectangular 1–8 × 1–8 text cells, up to 64 UTF-8 bytes/cell |

Mathematical inputs stay strings, including unfinished fields; storage does not
validate their mathematical meaning. Graph coordinate strings describe the
existing approximate view. Names: 1–80 characters after trimming. Server limits:
100 workspaces, 100,000 bytes of compact UTF-8 JSON/workspace, 1,000,000 total data
bytes/account; request body at most 128,000 bytes. Limits never truncate data.
Owner locking serializes quotas and revisions, including concurrent creation.

Errors: 400 invalid schema/name, 401 unauthenticated, 403 CSRF, 404 missing or
foreign UUID, 409 `account_changed`/`revision_conflict`/`quota_exceeded`, 413 size,
415 content type. Create/duplicate return 201; updates begin at revision 1 and
require the current revision. Failed saves/conflicts retain the working input;
refresh/open or save a separate copy explicitly. A lost response may follow a
committed save: refresh the list before retrying. Account changes discard cached
names and ignore delayed responses. Newer input edits also invalidate delayed loads.

Next: optional autosave and device management. See the
[account design](../design/APPLICATION_DESIGN.md#proposed-accounts-and-saved-work).
