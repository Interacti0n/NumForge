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

`python tests/containers/check_monitor.py` separately verifies authenticated
provisioning, a working/failing keyword monitor, push down/up transitions and
retention of stored account/configuration/history across restart. It checks the
non-root, read-only, dropped-capability runtime protections. The default isolated
port is 18886; `NUMFORGE_TEST_MONITOR_PORT` overrides it. To test frozen images,
set `NUMFORGE_TEST_MONITOR_IMAGE`; also set `NUMFORGE_TEST_MONITOR_BASE_IMAGE`
to exercise old → candidate → old → candidate on the same disposable volume.
Selected images must already exist locally. Use a unique `numforge-test-` project;
the helper removes that project's containers and volumes when finished.

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

### Monitor security verification — 2 November 2026

The monitor recipe uses the official Kuma 2.5.9-slim image pinned by digest,
retaining OS upgrades, UID 1000, the Node healthcheck and removal of unused
healthcheck/cloudflared/npm executables. No application dependency overrides or
finding suppressions are used. The [upstream release](https://github.com/louislam/uptime-kuma/releases/tag/2.5.9)
also fixes RSS cache poisoning and refreshes dependencies.

Trivy 0.76.1 with the same database updated at 13:15 UTC on 2 November compares:

| Hardened monitor image | Fixable HIGH/CRITICAL records | All HIGH / CRITICAL records |
| --- | ---: | ---: |
| Previous Kuma 2.5.8 | 40 | 111 / 7 |
| Candidate Kuma 2.5.9 | 37 | 108 / 7 |

Three `undici` records disappear; no new fixable HIGH/CRITICAL records appear.
The historical count of 39 used an older vulnerability database. All 37 remaining
fixable records are Node dependencies; this update does not resolve them or the
unfixed OS records. Counts are package findings, not unique CVEs or proven exploits.
Keep the admin endpoint on loopback; absence of a configured feature is not a
blanket exemption. Gateway/PostgreSQL were not rescanned for this monitor change.

The frozen old/candidate images passed the isolated restart, upgrade, rollback
and reapply sequence, including authentication, monitor and push behavior,
preserved account/configuration/history hashes and runtime protections. Raw image
identities, layer-matched scan reports, scanner database times and test logs are
under ignored `build/security-20261102/`. Qualification alone does not update a
running pilot or establish Windows reboot behavior. The default CI invocation
(building the recipe without image overrides) also passed its restart,
authentication, push and stored-state checks; both fixture projects were removed.

The qualified candidate was subsequently deployed to the existing pilot as
`numforge-uptime:security-20261102`, exact image
`sha256:9359229938718f815d88001e70e48cca2d306f3a76042c57d5e838c15d88197d`.
Only the monitor container was replaced under the supervisor's operation lock.
Web/accounts/DB/gateway container identities and the then-current tunnel URL
remained unchanged. Existing users, complete monitor settings and the retained
heartbeat history matched the consistent pre-upgrade SQLite copy; six monitors
reported up, with fresh checks from the new process for non-push monitors.
The loopback-only admin binding and runtime protections were checked again.
A subsequent standard supervisor Ensure run preserved the new monitor image,
the remaining service identities and the public URL; all six monitors stayed up.

Before and after the change, the existing external encrypted repository received
the standard account/configuration/Git backup plus a separately tagged consistent
monitor database snapshot. Both standard backups passed byte-for-byte restore,
repository checks and comparison of all eleven account tables/rows with the
unchanged source. Both monitor snapshots passed byte comparison and SQLite
integrity/stored-state verification after restore. All four restore points were
retained at completion; these standard runs also kept the last ten snapshots to
preserve their same-day pre-upgrade point. Monitor snapshots are point-in-time
upgrade backups; the daily job does not refresh the monitor SQLite database.
Failed preparation attempts are retained in the evidence folder; they stopped
before service replacement. Deployment/backup/final-check records and execution
logs are in `build/security-20261102/`; private database copies stay in `.local/ops`.
