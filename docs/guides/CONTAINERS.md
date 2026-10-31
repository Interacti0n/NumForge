# Docker Compose

Optional Linux-container deployment, independent of the C SDK/CLI build.
Requires Docker Engine with Compose, or
[Docker Desktop/WSL 2 on Windows](https://docs.docker.com/desktop/setup/install/windows-install/).
Python 3.12 is needed on the host only for configuration and backup helpers.

```sh
python scripts/compose.py init
docker compose config --quiet
docker compose up --build --wait
```

Open **http://127.0.0.1:18876**. `NUMFORGE_PORT` changes both host/container ports,
preserving browser Origin checks. The existing preview on 8766 is unaffected.

| Service | Purpose |
| --- | --- |
| `web` | C calculator/web; builds without PostgreSQL or Python dependencies |
| `db` | PostgreSQL 18; persistent `accounts-data` volume, no published port |
| `setup` | One-shot role/schema provisioning and migrations before accounts start |
| `accounts` | Private Django/Gunicorn service with runtime credentials only |

`deploy/gateway.Dockerfile` keeps the Caddy release while updating its Go/TLS
dependencies. `deploy/postgres.Dockerfile` rebuilds upstream gosu with the patched
Go toolchain; the PostgreSQL version/data format stays unchanged. The optional
monitor image updates OS packages, runs as `node` with a read-only root and omits
its unused Go healthcheck, built-in tunnel and global npm. Node performs its HTTP
healthcheck; application dependencies remain upstream-owned.

The [login/logout API](ACCOUNTS_STORAGE.md#private-pilot-authentication) and SK/EN
form have an optional HTTPS gateway; public registration remains unavailable.
Keep the host binding on loopback until HTTPS/origin configuration is explicit.
`web` has no dependency on the DB or accounts. Its default native binding remains
127.0.0.1; explicit `--bind 0.0.0.0` enables container networking.

Configuration/secrets live in ignored `.local/compose/`, separate from native
`.local/accounts/`. Do not delete or regenerate them while keeping a DB volume.
Each container receives only its needed secret files. Host directories are
private; individual files must be readable by the non-root service after Docker
bind-mounts them. Compose secrets are not an encrypted secret vault.

```sh
docker compose ps
docker compose logs --tail 50
docker compose stop
docker compose up --wait
docker compose down
```

`down` preserves the DB volume; **`down --volumes` destroys its data**.
Restart policies take effect when Docker itself is running; Docker Desktop's
startup/login behavior still needs validation on the target PC.

## Backups and moving to a server

Pause account writes during verification. The helper preserves binary dumps even
in Windows PowerShell; avoid shell redirection of binary `pg_dump` output there.

```sh
python scripts/compose.py backup
python scripts/compose.py verify-backup --file .local/compose/backups/FILE.dump
```

Verification creates a disposable database, compares all public table rows and
removes only that database. Protect copies of dumps/configuration on another
device. Native and container databases are separate: moving existing data needs
an explicit backup/restore, never copying a Windows PostgreSQL data directory.
On a Linux server, use the same Compose setup, restored data/configuration and a
separately configured HTTPS gateway. Review capacity, updates and backup retention
before public hosting; image builds do not imply tested ARM deployment.

## HTTPS account pilot

```sh
python scripts/compose.py gateway-init
docker compose -f compose.yaml -f compose.accounts.yaml up --build --wait
```

Open **https://localhost:18878/login?lang=sk** (`NUMFORGE_GATEWAY_PORT` overrides
the port). Caddy's local CA stays in `gateway-data`; no host trust store is changed.
Local browsers need that CA explicitly trusted, or use a verified HTTPS tunnel.
Export its public root with `docker compose -f compose.yaml -f compose.accounts.yaml
cp gateway:/data/caddy/pki/authorities/local/root.crt .local/compose/gateway-root.crt`.
Never export its private key. Tunnel origin TLS must verify this root and use
`localhost` as its origin server name/Host; do not disable certificate verification.

On the configured Windows PC, `scripts/pilot.ps1 -Action EnableStartup` installs
hidden logon/minute recovery checks; `-Action Status` shows the last verified URL.
`-Action DisableStartup` removes checks and leaves the running tunnel alone.
The helper needs `.local/compose/pilot.env`, the Python path in
`.local/ops/backup-runner.json` and `.local/tools/bin/cloudflared.exe`.
It starts Docker Desktop when needed, restores containers without rebuilding,
preserves a live tunnel and creates a replacement after process exit. If public
checks fail and Cloudflare explicitly reports `Unauthorized: Tunnel not found`,
it retires only the process with the verified executable/creation time and retries.
Ordinary network failures leave the process alone. A replacement
Quick Tunnel changes the hostname: Origin/CSRF and Kuma follow the exact new URL;
users sign in again on that hostname. The CA remains verified. Current state is in
`.local/compose/pilot-status.json` and `pilot-url.txt`; failures retry next minute.
Daily `manage.py clearsessions` removes expired database sessions and preserves
active sessions. Real Windows reboot/login verification remains a separate test.

For a Cloudflare HTTPS tunnel, set `NF_ACCOUNT_PUBLIC_ORIGIN` to its exact HTTPS
origin before recreating `gateway`/`accounts`. The gateway checks browser Origin,
redirects edge HTTP to HTTPS and supplies a private gateway token plus authoritative
client address. Cloudflare overwrites `Cf-Connecting-IP` at its edge; the origin
port must remain loopback/private. Other proxies need their own explicit address
policy. Do not expose this loopback preview config directly to a public network.

The login form uses secure cookies, clears password fields, checks sessions after
reload/tab changes and keeps failed network operations recoverable. It stores no
credentials in browser storage. Native guest `/login` retains an explanatory fallback.

CI builds the stack, checks real TLS/cookies/CSRF, SK/EN desktop/mobile login,
cross-tab logout, persisted identities, backup/restore and calculations with DB stopped.
It also checks monitor failure detection/persistence, gates listed-fix HIGH/CRITICAL
gateway/DB findings and uploads reports. Remaining upstream monitor findings are
reported separately; keeping its admin endpoint on loopback is essential.
For a disposable local check,
set `COMPOSE_PROJECT_NAME=numforge-test-local`, then run
`python tests/containers/check_stack.py`. It removes that project's volumes.

## Local operations tools

Optional tools remain outside the mathematical engine:

| Tool | Local use |
| --- | --- |
| Restic | Encrypted logical DB dump, committed Git history and private pilot configuration |
| Uptime Kuma | `http://127.0.0.1:18880`; public login/session/calculation, direct C engine, DB and backup freshness |
| DBeaver Community | `scripts/dbeaver.ps1`; isolated workspace, PostgreSQL on loopback `55433`, read-only `nf_inspect` role |
| sccache | Optional CMake compiler launcher; Ninja/Make builds only, no application runtime dependency |
| Trivy | Local image vulnerability reports, with no Docker socket mounted into the scanner |

The configured PC keeps binaries under `.local/tools/`, private credentials under
`.local/ops/` and extends `COMPOSE_FILE` with `compose.tools.yaml`. Provisioning
passes private `monitor-admin.json` through stdin to `scripts/setup_monitor.cjs`
inside Kuma; the credential file is not mounted there. Do not publish its admin port.
This monitor cannot report a whole-PC outage while that same PC is off.

```sh
python scripts/ops_backup.py run
python scripts/ops_backup.py check
python scripts/ops_db.py
python scripts/scan_images.py numforge-web numforge-accounts --fail-on-fixable
```

Backup initialization is one-time: install Restic in `.local/tools/bin/`, then
`python scripts/ops_backup.py init --repository PATH`. The pilot environment is
read from `.local/compose/pilot.env`. Every run restores the exact snapshot,
compares file hashes and restores the dump into a disposable database before
retaining 7 daily, 4 weekly and 6 monthly snapshots. An absent disk fails closed.
The Windows daily task runs at 19:00 when the user is logged in; it requires Docker
and the external disk. `backup.log` and `backup-status.json` record local results.
The GUI launcher prevents console flashes; task result `0` confirms success.

Preserve `.local/ops/restic.password` separately from both PC and backup disk.
Without it, encrypted snapshots cannot be recovered. Git bundles contain committed
history, including unpushed commits; uncommitted edits and runtime build snapshots
are outside this backup. On a new machine, restore configuration/source, provision
database roles (including `nf_inspect` if present), then restore the logical dump.
Never restore over live data as a verification step.

For compilation caching, set `CMAKE_C_COMPILER_LAUNCHER` to `sccache` during a
Ninja/Make configure; keep `SCCACHE_DIR` outside disposable build trees. This PC's
ignored `cached-apps` user preset does that. Trivy reports go to `build/security/`;
`--fail-on-fixable` fails on HIGH/CRITICAL findings with a listed fix. This is a
triage aid, not proof of exploitability or an all-clear for unpatched findings.
