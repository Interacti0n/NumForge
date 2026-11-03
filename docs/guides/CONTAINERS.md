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

### gRPC transport compatibility migration

The current recipe migrates Kuma's exact original `@grpc/grpc-js ~1.8.22` range to
`1.13.7`. This stable maintenance release contains the malformed-message fixes
and subsequent error-detail fixes while retaining the existing proto-loader
0.7.15 and protobuf 7.6.6. The newer 1.14 branch requires proto-loader 0.8.
The only added runtime package is exact `@js-sdsl/ordered-map` 4.4.2, required by
the migrated gRPC package. Both registry archives are pinned by SHA512.

The build guard now handles scoped registry archive URLs correctly. An added
dependency must be absent at its exact root path before installation, be pinned
exactly, belong to an explicitly migrated owner, satisfy that owner's new range
and resolve from it to the permitted path. Its archive URL/integrity/version and
dependency closure are verified. All unlisted installed additions/removals or
package changes remain rejected. Five new guard regressions exercise accepted
scoped additions and rejected pre-existing, unneeded, tampered or unlisted ones.
Existing nested consumers and the previous optional omissions remain unchanged.

Thirty-nine checks include eight new gRPC regressions and the five new guards.
Four bounded child processes send invalid gzip/deflate frames to a real client
and server: each old process crashes on an unhandled decompressor error; the new
runtime reports INTERNAL and stays alive. After a malformed request, a valid
request still succeeds. Further tests cover receive-size limits, deadlines,
handler exceptions without private error-detail disclosure, successful requests
after rejection, and TLS with trusted certificates versus untrusted/missing
client certificates or an untrusted server root. Certificates are generated only
inside the disposable container. The previous real Kuma gRPC/wire-byte tests,
Liquid tests and parser/redirect/glob regressions remain in the same suite.

The TLS test verifies transport certificate enforcement; it does not reproduce
the optional-client `getAuthContext` advisory. That public server API is absent
from the qualified 1.13 runtime. Scanner removal of an advisory is not a claim
that every configuration described by it has been exercised. The pilot uses
Kuma's client monitor, not an exposed gRPC server; server-side regressions cover
the supplied library without adding a service to the pilot. Previous Liquid,
glob and protobuf limitations still apply. This is not a web speed benchmark.

Primary references: [gRPC 1.13.5 fixes](https://github.com/grpc/grpc-node/releases/tag/%40grpc%2Fgrpc-js%401.13.5),
[gRPC 1.13.7 maintenance fix](https://github.com/grpc/grpc-node/releases/tag/%40grpc%2Fgrpc-js%401.13.7),
[compressed-message advisory](https://github.com/grpc/grpc-node/security/advisories/GHSA-99f4-grh7-6pcq),
[HTTP/2 stream advisory](https://github.com/grpc/grpc-node/security/advisories/GHSA-5375-pq7m-f5r2),
[optional-certificate advisory](https://github.com/grpc/grpc-node/security/advisories/GHSA-m9gg-hp2v-232j),
[error-detail advisory](https://github.com/grpc/grpc-node/security/advisories/GHSA-f596-whhp-79r4).
Raw evidence is under ignored `build/security-grpc-20261103/`, including registry
metadata, the upstream security diff, exact image scans and regression logs.

Matched Trivy 0.76.1 scans, using the database updated 2 November 2026 at
13:15:56 UTC, report **23 → 20 fixable HIGH/CRITICAL records**. The three removed
HIGH records are CVE-2026-101916, CVE-2026-48068 and CVE-2026-48069; none are added.
All HIGH/CRITICAL records including unfixed findings move from 95/6 to 92/6.
The error-detail LOW record is also removed. Both scan DiffID lists match the
frozen images. The final previous → candidate → previous → candidate fixture
preserves its stored user/configuration/history digest, authenticated login,
keyword/push up/down behavior and runtime protections. Both candidate stages
pass all 39 checks. The earlier 38-check run is retained separately; the final
run includes the added error-detail regression. Gateway/DB were not rescanned.

The qualified image was deployed on 3 November 2026 as
`numforge-uptime:grpc-20261103`, exact ID
`sha256:adf06bb1c3b44c8979c8dfc94783132ce4cd7d06b2d575848a4f83c7460bbf82`,
from source commit `3f09cbc`. Only monitoring was recreated under the supervisor
lock. Web/accounts/PostgreSQL/gateway retained their container identities; the
owned public tunnel and URL stayed unchanged. The live gRPC/proto-loader/
ordered-map/Liquid/protobuf versions, full patch receipt and adapted gRPC source
hash matched the qualified candidate. Full stored users/monitor settings and
retained heartbeat history matched the pre-change copy. Six monitors were up
with fresh non-push heartbeats; local/public HTTPS and runtime protections passed.

Before and after this promotion, standard account/configuration/Git backups and
separately tagged consistent monitor snapshots in the existing external encrypted
repository passed byte-for-byte restoration, SQLite integrity/stored-state checks,
repository checks and comparison of all eleven account tables/rows. All four
restore points were retained at completion with the existing same-day retention
supplement. Daily standard retention may later rotate standard snapshots; the
daily job still does not refresh monitor SQLite. Deployment and backup records
are in `build/security-grpc-20261103/`.
A subsequent normal supervisor Ensure check preserved the new monitor image,
all other service identities and the public URL. All five services were healthy,
all six monitors were up and the owned fixture's containers, volume and network
were verified absent. Windows reboot/login remains unverified.

### WebSocket fragment-limit compatibility migration

The current recipe migrates Kuma's original direct `ws ~8.19.0` range to exact
`8.21.3`. It includes fragment/chunk limits, empty-fragment accounting, typed-array
close-frame protection and the compression-window negotiation correction. Later
8.22 releases exist; the selected version also satisfies the existing Socket.IO
consumers' `~8.21.0` ranges. MQTT retains its separate 7.5.13 runtime.

npm shares three existing 8.21.3 copies from engine.io, engine.io-client and
socket.io-adapter with the migrated root package. The first build correctly
rejected those previously unlisted removals. The final manifest explicitly lists
only these three copies. Before installation, the guard checks each original
version, archive integrity, compatible consumer ranges and pinned destination.
Afterward, each original consumer must resolve the root destination and its
version/integrity/package-metadata hash must match the old copy exactly. Other
installed additions/removals or package changes remain rejected. Four new guard
regressions cover the accepted case, incompatible range, changed shared identity
and a retained duplicate. No additional application source adaptation is made.

Fifty-three checks include ten WebSocket regressions and the four new guards.
Small receiver fixtures reject excess one-byte fragments, empty fragments and
buffered chunks; payload-byte limits remain independent. Real client and server
connections, without custom fragment settings, close an unfinished stream after
16,385 empty fragments. This sends only about 32 KiB unmasked or 96 KiB masked
headers, without deliberately exhausting memory. The receiver reports
`WS_ERR_TOO_MANY_BUFFERED_PARTS`; the peer receives close code 1008. The rejecting
side can report local abnormal closure after stopping its parser.

Other tests preserve compressed fragmented Unicode text, binary data, ping/pong,
normal close status and exact Uint8Array close-frame wire bytes. Non-empty
Uint16Array close reasons are rejected before transmission. Compression offers
below the configured client window are rejected. The actual Kuma WebSocket
monitor passes authenticated upgrade, custom-header and subprotocol checks and
rejects incorrect credentials. The previous image fails seven checks; the final
candidate passes all 53. Previous parser/gRPC/Liquid/glob tests remain included.

These are per-connection structural limits, not a global memory quota or proof
against every resource-exhaustion pattern. No web speed improvement is claimed.
The pilot's six configured monitors and loopback exposure are unchanged.
Primary references: [fragment/chunk advisory](https://github.com/websockets/ws/security/advisories/GHSA-96hv-2xvq-fx4p),
[8.21.1 empty-fragment fix](https://github.com/websockets/ws/releases/tag/8.21.1),
[8.21.3 negotiation fix](https://github.com/websockets/ws/releases/tag/8.21.3),
[typed-array close fix](https://github.com/websockets/ws/releases/tag/8.20.1).
Raw evidence is in ignored `build/security-ws-20261103/`, including the initially
rejected build, metadata, exact image scans and regression logs.

Matched Trivy 0.76.1 scans, using the database updated 2 November 2026 at
13:15:56 UTC, report **20 → 19 fixable HIGH/CRITICAL records**: HIGH
CVE-2026-48779 is removed, with none added. The close-frame MEDIUM record is
also removed. All HIGH/CRITICAL records including unfixed findings move from
92/6 to 91/6. Both scan DiffID lists match the frozen images. The previous →
candidate → previous → candidate fixture preserves its stored user/configuration/
history digest, authenticated login, keyword/push up/down behavior and runtime
protections. Both candidate stages pass all 53 checks. The fixture removes its
own containers, volume and network. Gateway/DB images were not rescanned.

The qualified image was deployed on 3 November 2026 as
`numforge-uptime:ws-20261103`, exact ID
`sha256:dd6f0a9286726178ad4b47acf2ff8e7217b06612d622ac25969c7a02b39533b4`,
from source commit `0e5dc6e`. Only monitoring was recreated under the supervisor
lock. Web/accounts/PostgreSQL/gateway container identities and the owned public
tunnel/URL stayed unchanged. Live ws sharing, the separate MQTT version, all prior
patched versions, receipt and gRPC source hash matched the candidate. Full stored
users/monitor settings and retained heartbeat history matched the pre-change
copy. All six monitors were up with fresh non-push heartbeats; local/public HTTPS
and non-root/read-only/loopback/capability protections passed.

Before and after promotion, standard account/configuration/Git backups and
separately tagged consistent monitor snapshots in the existing external encrypted
repository passed byte-for-byte restore, SQLite integrity/stored-state checks,
repository checks and comparison of all eleven account tables/rows. All four
restore points were retained at completion with the existing retention supplement.
Daily standard retention can later rotate standard snapshots; the daily job still
does not refresh monitor SQLite. Records are in `build/security-ws-20261103/`.
A later normal supervisor Ensure check preserved the new image, other service
identities and public URL. All five services were healthy and all six monitors
were up; the owned test containers, volume and network were verified absent.
Windows reboot/login remains unverified.

### MySQL authentication and packet compatibility migration

The current recipe changes Kuma's direct `mysql2 ~3.11.5` range to exact 3.23.1.
This includes the clear-password authentication gate and the compressed-packet
output bound. The new SQL escaper is pinned to 1.5.1; iconv-lite 0.7.2 is installed
only under mysql2, while Kuma and other consumers retain root iconv-lite 0.6.3.
Only the verified unused seq-queue 0.0.5 is removed; root sqlstring stays installed
for its existing consumers. All other installed packages and earlier patches
remain unchanged. Registry archives are pinned by SHA512.

The addition guard now accepts an exact root path or an exact path under its
explicitly migrated owner. An owner-local dependency requires an exact pin under
that owner's override; unrelated parent paths and missing local pins are rejected.
Three new guard regressions cover those cases. Existing archive, consumer-range,
dependency-closure, sharing and unrelated-change checks still apply. No additional
Kuma application source adaptation or clear-password opt-in is added.

Sixty-two regressions include six MySQL checks and the three new guards. An
independent protocol-v10 TCP fixture requests a switch to mysql_clear_password.
Both the original client and the original actual Kuma MySQL monitor transmit
the disposable fixture password; the candidate rejects the switch before that
transmission. No real credentials or external database are used in this test.
A small zlib fixture expands 1 KiB while advertising eight bytes: the old runtime
accepts it, while the candidate rejects it with ERR_BUFFER_TOO_LARGE. Valid
compressed/plain and corrupt data retain their expected behavior. Formatting
checks preserve quoted literals, identifiers and binary values. The previous
image fails four checks; the candidate passes all 62.

Five additional tests pass on both frozen monitor images against real MySQL
8.4.13, official image digest
`sha256:a81d043930e51745522bd5c430961cf8286f0a23f0a9a2a6e0108f7ea71b0902`.
They cover prepared binary-protocol results with exact BIGINT/DECIMAL strings,
Unicode/binary data, larger compressed queries, named placeholders, pooled
transactions/rollback, actual Kuma row counts/single-value checks and SQL/auth
failures. The server runs on an internal disposable network with generated
fixture credentials, tmpfs data, no published port, a non-root user and no
capabilities. Its container and network are removed afterward; the pilot's
PostgreSQL is not used or changed. Raw setup and exact identities are recorded.

The decompression bound applies to a single advertised packet, not total process
memory. The recipe does not enable clear-password authentication. These tests
cover the recorded MySQL server, not every MariaDB/MySQL version or TLS/auth-plugin
configuration. Previous Liquid/protobuf/glob/WebSocket limits remain documented.
Primary references: [authentication advisory](https://github.com/sidorares/node-mysql2/security/advisories/GHSA-3f6p-5ww8-9rcr),
[default gate implementation](https://github.com/sidorares/node-mysql2/commit/884bec56288d827939d0dd3f1f4ae476fbc8dbeb),
[compression advisory](https://github.com/sidorares/node-mysql2/security/advisories/GHSA-rgwj-5xj2-c3m3),
[compression fix](https://github.com/sidorares/node-mysql2/commit/7c48343).
Raw evidence is in ignored `build/security-mysql-20261103/`, including standalone
regressions, real database checks, scanner reports and the lifecycle fixture.

Matched Trivy 0.76.1 scans using the database updated 2 November 2026 at
13:15:56 UTC report **19 → 18 fixable HIGH/CRITICAL records**. HIGH
GHSA-3f6p-5ww8-9rcr and the compressed-packet MEDIUM record are removed; no new
fixable HIGH/CRITICAL record is added. All HIGH/CRITICAL records including unfixed
findings move from 91/6 to 90/6. Both scan DiffID lists match the frozen images.
The previous → candidate → previous → candidate fixture preserves its stored
user/configuration/history digest, authenticated login, keyword/push up/down
behavior and runtime protections. Both candidate stages pass all 62 checks.
The monitor fixture and the separate real MySQL container/network are removed.
The pilot gateway/PostgreSQL images were not rescanned.

The qualified image was deployed on 3 November 2026 as
`numforge-uptime:mysql-20261103`, exact ID
`sha256:a6d1dc00b403c1943ed71bc62258266138b818730998dc8b16c71ec7d250ce8b`,
from source commit `e01676d`. Only monitoring was recreated under the supervisor
lock. Web/accounts/PostgreSQL/gateway container identities and the owned public
tunnel/URL stayed unchanged. Live MySQL, SQL escaper, isolated/root charset and
all previous patched versions/sharing, receipt and gRPC source hash matched the
candidate. Full stored users/monitor settings and retained heartbeat history
matched the pre-change copy. Six monitors were up with fresh non-push checks;
local/public HTTPS and runtime protections passed.

Before and after promotion, standard account/configuration/Git backups and
separately tagged consistent monitor snapshots in the existing external encrypted
repository passed byte-for-byte restore, SQLite integrity/stored-state checks,
repository checks and comparison of all eleven account tables/rows. All four
restore points were retained at completion with the existing retention supplement.
Daily standard retention may later rotate standard snapshots; the daily job still
does not refresh monitor SQLite. Records are in `build/security-mysql-20261103/`.
A later normal supervisor Ensure check preserved the new monitor image, all
other service identities and public URL. Five services were healthy, six monitors
were up and both owned fixtures' containers/networks (plus the monitor test
volume) were verified absent. Windows reboot/login remains unverified.

### Nested Lodash consumer compatibility migration

The current recipe updates only redbean-node's nested Lodash 4.17.23 to exact 4.18.1.
The root Lodash was already 4.18.1 and remains unchanged. RedBean 0.3.3 declares
`~4.17.21`; its exact scoped override is therefore an explicit, tested consumer
minor migration. Root Knex 3.1.0 and nested Knex 2.4.2 retain their versions.
No additional Kuma source adaptation is applied.

The manifest records the consumer path, original version, registry integrity and
declared range, plus the pinned target archive SHA512. The build guard permits
this exception only within the same stable major and for that exact consumer.
Other consumers still need compatible declared ranges. The owner package's full
installed metadata must remain unchanged after installation and resolve the
qualified target. Seven new guard regressions cover the accepted migration,
changed upstream metadata/range, wrong pin, major jump, non-leaf package, another
incompatible consumer and owner metadata changes after installation.

An added scoped override alone left the old nested archive installed. Rejected
build logs are retained. After all preflight checks, the final recipe refreshes
only that explicitly qualified leaf's lock entry and removes its verified old
directory so npm reifies the pinned registry archive. It leaves other lock entries
locked, uses no lifecycle scripts, and validates the resulting actual package,
archive integrity and unrelated installed packages. The owner and root Lodash
metadata stay unchanged. Unrestricted dependency updates are not used.

Seventy-six checks include seven new Lodash tests and the seven guard regressions.
The old nested library executes a benign global marker from a template import
key; the candidate rejects it before execution. Inherited imports are excluded,
array-wrapped paths cannot delete constructor prototype properties, and modular
template/fromPairs helpers preserve safe output/prototype behavior. Both actual
RedBean/SQLite paths, using root and nested Knex, preserve schema/bindings,
Unicode/JSON CRUD, commit/rollback, unique-constraint error recovery and deletion.
The original image fails five checks; the candidate passes all 76. These bounded
checks do not establish a general template sandbox. Prior limitations remain.

Primary references: [template imports advisory](https://github.com/lodash/lodash/security/advisories/GHSA-r5fr-rjxr-66jc),
[4.18.1 published modular-build fix](https://github.com/lodash/lodash/releases/tag/4.18.1),
[npm scoped overrides](https://docs.npmjs.com/cli/v11/configuring-npm/package-json/#overrides).
Raw evidence is under ignored `build/security-lodash-20261103/`, including rejected
builds, exact installed-version checks, red/green tests, scans and lifecycle logs.

Matched Trivy 0.76.1 scans using the database updated 2 November 2026 at
13:15:56 UTC report **18 → 17 fixable HIGH/CRITICAL records**. HIGH
CVE-2026-4800 and the array-path MEDIUM record are removed; no new fixable
HIGH/CRITICAL record is added. All HIGH/CRITICAL records including unfixed findings
move from 90/6 to 89/6. Both scan DiffID lists match the frozen images. The previous
→ candidate → previous → candidate fixture preserves stored user/configuration/
history hashes, authenticated login, keyword/push up/down and runtime protections.
Both candidate stages pass all 76 checks. Its containers, volume and network are
removed afterward. The gateway/PostgreSQL images were not rescanned.

The qualified image was deployed on 3 November 2026 as
`numforge-uptime:lodash-20261103`, exact ID
`sha256:f0e3c7b565a5f875ab5d6ad8d26d8106705cb13b80c52a07a223e105f84762ac`,
from source commit `52bf9ba`. Only monitoring was recreated under the supervisor
lock. Web/accounts/PostgreSQL/gateway container identities and the owned public
tunnel/URL stayed unchanged. Both live Lodash versions, owner/Knex versions, all
previous pins/sharing, receipt and gRPC source hash matched the candidate. Full
stored users/monitor settings and retained heartbeat history matched the pre-change
copy. Six monitors were up with fresh non-push checks; local/public HTTPS and
runtime protections passed.

Before and after promotion, standard account/configuration/Git backups and
separately tagged consistent monitor snapshots in the existing external encrypted
repository passed byte-for-byte restore, SQLite integrity/stored-state checks,
repository checks and comparison of all eleven account tables/rows. Four restore
points were retained at completion with the existing retention supplement. Daily
standard retention may later rotate standard snapshots; the daily job still does
not refresh monitor SQLite. Records are in `build/security-lodash-20261103/`.
A later normal supervisor Ensure check preserved the new image, other service
identities and public URL. All five services were healthy and all six monitors
were up; the owned test containers, volume and network were verified absent.
Windows reboot/login remains unverified.

### Axios legacy compatibility migration

The next qualified monitor keeps Kuma 2.5.9 and migrates its direct Axios
dependency from 0.32.0 (`~0.32.0`) to exact **0.34.0**. This is an explicit
compatibility migration within the maintained legacy 0.x line, where a minor
change can break behavior. It does not select Axios 1.x. The manifest pins the
registry archive SHA512. The build guard requires an explicit `legacy-0.x`
policy, a forward 0.x version and the original direct range; the normal stable
major policy remains the default. Unrelated consumer ranges and installed
package metadata still must match. No new dependency is introduced; the existing
follow-redirects 1.16.2 pin remains. Development-only wait-on retains its own
Axios declaration in the lock and remains absent from the production image.

Seven additional guard checks cover the exact accepted pin, implicit 0.x
migration, a jump to 1.x, downgrade, unchanged version, unknown policy and a
separate incompatible consumer. Fifteen Axios checks exercise inherited form
visitor/depth/options, async and sync interceptor proxy isolation, default
methods, NO_PROXY IPv4 CIDR routing, explicit proxies, JSON/Unicode/Basic auth,
headers, redirects, timeout, cancellation and decompressed response limits.
The actual Kuma Webhook provider sends JSON, GET parameters, multipart and
custom Liquid bodies only to disposable loopback fixtures, and preserves HTTP
error/malformed-header handling. No external notification is sent.

The candidate passes **98 checks**. The baseline passes 92 and fails six,
including the version assertion, inherited visitor/depth, both interceptor
proxy cases and CIDR bypass. Pollution probes use isolated child processes,
dummy credentials and bounded local servers. The proxy cases demonstrate the
read-side gadget conditional on existing same-process prototype pollution;
they do not prove an attacker can introduce pollution into Kuma. The runtime
compatibility checks cover Kuma's own scalar header use; arbitrary third-party
header objects, all notification providers and a general resource bound are
not qualified. Axios 0.34.0 also tightens accepted header values and introduces
CIDR NO_PROXY semantics; callers should review these when changing proxy/header
configuration. Previous template/parser/resource limitations remain.

Primary references: [Axios 0.34.0 changes](https://github.com/axios/axios/releases/tag/v0.34.0),
[inherited form options advisory](https://github.com/axios/axios/security/advisories/GHSA-x97p-jq2g-jp4f),
[interceptor proxy advisory](https://github.com/axios/axios/security/advisories/GHSA-gcfj-64vw-6mp9).
Raw evidence is under ignored `build/security-axios-20261103/`.

Matched Trivy 0.76.1 scans on 3 November, using the database updated 2 November
at 13:15:56 UTC, report **17 → 15 fixable HIGH/CRITICAL records**. HIGH
CVE-2026-101909 and CVE-2026-67320 and seven MEDIUM records are removed; no
fixable HIGH/CRITICAL record is added. All HIGH/CRITICAL including unfixed
findings move from 89/6 to 87/6. Scan DiffIDs match both frozen images.
Gateway/PostgreSQL images were not rescanned; these are security and behavior
checks, not web performance benchmarks.

The previous → candidate → previous → candidate fixture preserves stored
users/configuration/history digests, authenticated login, keyword/push up/down
and runtime protections. Both candidate stages pass all 98 checks. The owned
fixture containers, volume and network are removed afterward.
