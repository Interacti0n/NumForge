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
healthcheck/cloudflared/npm executables. The initial 2.5.9 update used no application
dependency overrides or finding suppressions. The [upstream release](https://github.com/louislam/uptime-kuma/releases/tag/2.5.9)
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

### Targeted monitor dependency patches

The first targeted recipe applied three exact patch versions through npm overrides:

| Dependency | Upstream installed | Patched | Scope |
| --- | --- | --- | --- |
| body-parser | 1.20.8 | 1.20.9 | Express parser |
| follow-redirects | 1.16.1 | 1.16.2 | Axios redirect handling |
| minimatch | 9.0.3 | 9.0.9 | Only `redbean-node → glob` |

`deploy/monitor-patches.json` pins the versions and registry archive integrity.
The build guard checks every installed consumer of each replaced package against
its declared range, rejects an unexpected upstream version and verifies the final
installed versions, integrity and dependency closure. npm updates its lockfiles
with lifecycle scripts disabled. Unrelated installed package versions, integrity
and package metadata must remain unchanged. Three optional native build helpers
that npm reinstalls are checked against the unchanged upstream lock, then omitted
again exactly as in the base image. This preserves the supplied native bindings.
Other minimatch major versions retain their original consumers and versions;
a global minimatch override was rejected during preparation.

The same Trivy database used for both images reports **37 → 32 fixable
HIGH/CRITICAL records**, all remaining records in Node dependencies. All HIGH
records fall from 108 to 103; all CRITICAL remain seven, including unfixed records.
The five removed findings are the body-parser compressed-body crash,
follow-redirects finding and three minimatch ReDoS records. No new fixable
HIGH/CRITICAL record appears; no ignore list or VEX suppression is used.
Sources: [body-parser advisory](https://github.com/advisories/GHSA-27r3-jxmh-p828),
[follow-redirects release](https://github.com/follow-redirects/follow-redirects/releases/tag/v1.16.2),
[minimatch advisories](https://github.com/isaacs/minimatch/security/advisories).

Nine regression checks cover rejected build mutations, parser success/error/size
limits, a compressed trailing-byte process crash, redirect behavior/loop limits,
glob matching and the documented repeated-wildcard/nested-extglob cases. The old
image fails the crash and adversarial-glob checks; the patched image passes.
The two-second glob deadline is a test guard, not a runtime CPU bound for arbitrary
patterns. An exploratory alternating-literal/wildcard pattern still times out on
the patched library; the update does not establish a general ReDoS-free contract.
The ordinary monitor fixture runs these checks on patched images in CI as well.
Evidence: ignored `build/security-patches-20261102/`, including raw scan JSON,
red/green logs, build guard output and image identities.
The frozen-image lifecycle fixture also passed upstream → patched → upstream →
patched with stored user/configuration/history hashes unchanged, authenticated
provisioning, keyword up/down, push down/up and runtime protections. Both patched
stages ran the nine dependency regressions successfully.

The qualified image was then deployed as `numforge-uptime:patches-20261102`,
exact ID `sha256:f72e4f2e625725adc3d972e6b0665a065e697365e6221ffab7d9750ea757068b`.
Only monitoring was recreated under the supervisor lock. The web, accounts,
PostgreSQL and gateway retained their container identities; the current public
URL/tunnel stayed unchanged. Complete stored user/monitor settings and retained
heartbeat history matched the pre-change copy. Six monitors were up, with fresh
non-push heartbeats from the new process; local/public HTTPS checks and all
runtime protections passed. The deployed patch receipt matched the tested manifest.
A subsequent standard supervisor Ensure check preserved the patched image,
the other service identities and URL; all five services were healthy and all six
monitors remained up. The owned test project's containers, volume and network
were verified removed.

Both before and after promotion, the existing external encrypted repository
received a standard account/configuration/Git backup and a separately tagged
consistent monitor snapshot. Restored bytes, SQLite integrity/stored state,
repository checks and comparison of all eleven account tables/rows passed.
All four restore points were retained at completion using the same retention
supplement as the preceding update. Source commit: `3b8bd67`; deployment, backup
and final-check evidence is in `build/security-patches-20261102/`.

### Protobuf monitor compatibility migration

The next candidate pins `protobufjs` 7.6.6, from the supported 7.x release line.
This deliberately changes Kuma's direct `~7.2.6` requirement to exact `7.6.6`;
it is a tested minor migration, beyond the previous patch-only changes. Other
installed consumers must still accept the new version under their original ranges.
The build guard requires an exact original direct range, the pinned registry
integrity and the same stable major, and rejects unrelated package changes.
The new protobuf release removes its `@protobufjs/inquire` dependency; removal is
permitted only after checking its old version/integrity and that no installed
package still requires it.

Compatibility testing found that schemas must be fully resolved before use to
retain packed repeated-number encoding. The pinned Kuma gRPC adapter therefore
calls `root.resolveAll()` immediately after parsing the schema. The recipe verifies
the complete upstream source SHA256, exactly one replacement and the resulting
SHA256. Unexpected upstream source changes fail the build. This is the only
monitor application source adaptation; no other service or monitor type is changed.

Nineteen checks cover the existing patch regressions, explicit-migration guards,
source-change rejection, shared protobuf/proto-loader resolution, independent wire
bytes for int64/packed/oneof values, schema-default code injection, malformed-option
termination and real Kuma gRPC success/keyword failure/RPC error. The old image
executes the benign marker from a crafted bytes-default descriptor; the candidate
prevents it. A remaining decoder limitation is recorded: truncated native Buffer
string fields can clamp without error, while bytes fields and Uint8Array input
reject the corresponding truncation. This is not a general strict-frame validator.

Primary references: [protobuf 7.6.6 release](https://github.com/protobufjs/protobuf.js/releases/tag/protobufjs-v7.6.6),
[supported release lines](https://github.com/protobufjs/protobuf.js/security),
[schema-default advisory](https://github.com/protobufjs/protobuf.js/security/advisories/GHSA-66ff-xgx4-vchm),
[schema resolution API](https://protobufjs.github.io/protobuf.js/Root.html#resolveAll).
Evidence is under ignored `build/security-protobuf-20261102/`, including the
unadapted compatibility diagnostics, final red/green tests and matched image scans.

Matched Trivy 0.76.1 scans with the same database report **32 → 26 fixable
HIGH/CRITICAL records**, removing five HIGH and one CRITICAL protobuf finding,
with none added. All HIGH/CRITICAL records including unfixed findings move from
103/7 to 98/6. Remaining fixable records are other Node dependencies; gateway/DB
images were not rescanned for this change. Both final scan DiffID lists match
the qualified images. The frozen-image fixture passed previous → candidate →
previous → candidate with the stored account/configuration/history hashes
unchanged; both candidate stages passed all nineteen checks. Authenticated setup,
keyword up/down, push down/up and non-root/read-only/capability protections passed.

The qualified image was deployed as `numforge-uptime:protobuf-20261102`, exact
ID `sha256:f2c2fdc96236dfe477d12444ce214c1719aa534686732ad9153562f23fa8ffc5`,
from source commit `3dc69e7`. Only the monitor container changed under the
supervisor lock; web/accounts/PostgreSQL/gateway identities and the current
tunnel URL were retained. The live protobuf version, patch receipt and adapted
gRPC source SHA256 matched the candidate. Stored users, full monitor settings
and retained heartbeat history matched the pre-change copy; all six monitors
were up with fresh non-push checks. Local/public HTTPS and runtime protections passed.

Pre/post account/configuration/Git and separately tagged consistent monitor
backups to the existing external encrypted repository passed byte comparison,
SQLite integrity/stored-state checks, repository checks and restoration/comparison
of all eleven account tables/rows. All four restore points were retained at
completion with the existing same-day retention supplement. Deployment/backup
records and logs are in `build/security-protobuf-20261102/`.
A later standard supervisor Ensure check preserved the qualified monitor,
application container identities and URL. All five services were healthy and
all six monitors were up; the isolated test project's containers, volume and
network were verified removed. These checks do not establish Windows reboot behavior.

### Liquid notification-template compatibility migration

The current recipe pins `liquidjs` 10.27.2, explicitly migrating Kuma's direct
`~10.26.0` range within the same stable major. This is the smallest release that
contains all three targeted HIGH fixes: the `strip_html` infinite loop and the
`pop`/`join` memory-accounting bypasses. Later releases exist; this candidate does
not introduce their additional template features. The existing build guards check
the registry SHA512, all consumer ranges and unchanged unrelated packages. The
previous protobuf migration and its pinned gRPC source adaptation remain intact.

Twenty-six checks include seven new Liquid regressions. They exercise the real
Kuma notification provider's context, Unicode, legacy aliases, success/down/test
messages, filters, malformed syntax and blocked file includes. An isolated child
with a 32 MiB heap cap and two-second test deadline reproduces the old process
failure from the two-character message `a<`; the candidate completes normally,
also for unfinished script/style/comment openers. Small, bounded `pop` and `join`
cases demonstrate rejection when a configured memory budget would be exceeded,
while ordinary templates retain their expected output. Tests send no external
notifications. The previous image fails five checks; the candidate passes all 26.

These are bounded regression cases, not a general template isolation guarantee.
Kuma's provider does not configure Liquid memory/render limits; the update fixes
the library accounting when limits are supplied, without claiming a new runtime
budget for notifications. Direct/negative/first/last access now hides inherited
array values, but the published Node build still exposes them through join,
reverse, slice, compact and loop iteration. The same diagnostic reproduces on
the SHA512-verified latest 10.30.1 archive. This inherited-array limitation remains
explicit in the tests; no comprehensive `ownPropertyOnly` protection is claimed.
No additional notification-provider source patch is applied.

Primary references: [Liquid 10.27.1 fixes](https://github.com/harttle/liquidjs/releases/tag/v10.27.1),
[Liquid 10.27.2 fixes](https://github.com/harttle/liquidjs/releases/tag/v10.27.2),
[HTML loop advisory](https://github.com/harttle/liquidjs/security/advisories/GHSA-m7fp-h3p4-hr49),
[pop accounting advisory](https://github.com/harttle/liquidjs/security/advisories/GHSA-g357-x5c3-c72p),
[join accounting advisory](https://github.com/harttle/liquidjs/security/advisories/GHSA-4r6h-5v86-94p3),
[inherited-array advisory](https://github.com/harttle/liquidjs/security/advisories/GHSA-fwxr-j5w2-587m).
Raw evidence is under ignored `build/security-liquid-20261103/`, including the
initial failed isolation expectation and latest-version diagnostic.

Matched Trivy 0.76.1 scans, using the database updated 2 November 2026 at
13:15:56 UTC, report **26 → 23 fixable HIGH/CRITICAL records**. The three removed
HIGH records are CVE-2026-55575, CVE-2026-61556 and CVE-2026-69222; none are added.
All HIGH/CRITICAL records including unfixed findings move from 98/6 to 95/6.
Scanner removal of the inherited-array MEDIUM record does not override the
observed limitation above. Both scan DiffID lists match the frozen local images.
The previous → candidate → previous → candidate fixture preserves its stored
user/configuration/history digest, authenticated login, keyword/push up/down
behavior and runtime protections; both candidate stages pass all 26 checks.
The owned test containers, volume and network are removed by the fixture.
Gateway/DB images were not rescanned for this change.

The qualified image was deployed on 3 November 2026 as
`numforge-uptime:liquid-20261103`, exact ID
`sha256:de10ae1e722d9fc7e10746a851fa81fd99645423fa90459d30fee92d4831791f`,
from source commit `d7b736d`. Only monitoring was recreated under the supervisor
lock. Web/accounts/PostgreSQL/gateway container identities and the owned public
tunnel/URL stayed unchanged. The live Liquid/protobuf versions, complete patch
receipt and adapted gRPC source hash matched the candidate. Full stored
users/monitor settings and retained heartbeat history matched the pre-change
copy. All six monitors were up with fresh non-push heartbeats; local/public HTTPS
and non-root/read-only/loopback/capability protections passed.

Before and after promotion, standard account/configuration/Git backups and
separately tagged consistent monitor snapshots in the existing external encrypted
repository passed byte-for-byte restore, SQLite integrity/stored-state checks,
repository checks and comparison of all eleven account tables/rows. All four
restore points were retained at completion with the same retention supplement
as the preceding updates. Daily standard retention may subsequently rotate those
standard snapshots; the daily job still does not refresh monitor SQLite.
Deployment and backup records are in `build/security-liquid-20261103/`.
A subsequent normal supervisor Ensure check preserved the new monitor image,
the other service identities and the public URL. All five services were healthy,
all six monitors were up and no containers/volumes/networks from the owned test
project remained. This does not establish Windows reboot/login behavior.
