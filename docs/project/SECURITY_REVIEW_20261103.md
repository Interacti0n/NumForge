# Container security review — 3 November 2026

Subsequent [four-image maintenance](SECURITY_MAINTENANCE_20261103.md) removes
all 19 other-image records with listed fixes in matched scans using the newer
3 November database. The original 18-record inventory below remains a dated
audit snapshot. The separate monitor fixes and its remaining disputed legacy
minimatch record are recorded below and in the container guide.

The extended review found additional MEDIUM/LOW findings and a native SQLite
inventory gap. The qualified monitor candidate removes four MEDIUM records in
`qs` and one LOW record in `@tootallnate/once`. It passes 198 checks and the
persisted upgrade/rollback/reapply fixture. Three fixable MEDIUM scanner records
remain in this candidate. The other four running service images have 18 records
with listed fixes, all MEDIUM, LOW or UNKNOWN. These findings remain open.

The native engine has since been rebuilt and deployed as SQLite **3.54.1** with
211 passing checks and verified rollback/restore. See the
[native rebuild and promotion](../guides/CONTAINERS.md#native-sqlite-source-rebuild).
The scanner still does not identify this embedded engine separately; explicit
native provenance/runtime checks now cover it. Debian SQLite findings and the
archived binding's support status remain open.

A subsequent [glob/minimatch qualification](../guides/CONTAINERS.md#glob-14-and-minimatch-11-qualification)
passes 230 checks and the persisted upgrade/rollback/reapply and clean-volume
fixtures. Matched scans with the newer 3 November database remove two further
MEDIUM records, leaving one disputed legacy minimatch record and zero fixable
HIGH/CRITICAL. The earlier inventory tables below retain their original dates
and counts. The [gateway process fix](../guides/CONTAINERS.md#gateway-https-probe-process-reaping)
also addresses accumulated orphaned HTTPS probe processes found during resumed
qualification.

The qualified glob/minimatch image was then
[deployed with verified backups and recovery checks](../guides/CONTAINERS.md#globminimatch-promotion)
at **15:00:08 UTC**, from commit `32b123e`, without rebuilding. Only the monitor
image changed; the other services and public URL were preserved. Five services
were healthy and six monitors up after the normal supervisor check. Both
pre/post backup pairs passed restore verification and all four restore points
were retained.

At the start of this audit, the pilot used the earlier Axios monitor image, with
**15 fixable HIGH/CRITICAL records**. The qualified review image was subsequently
deployed after explicit approval, with **zero fixable HIGH/CRITICAL records** in
the matched scan. The promotion and retained risks are recorded below.
This report is an audit of the container dependency work, not an assertion that
the application, operating system or deployment is free of vulnerabilities.

## Method and frozen images

Trivy **0.76.1**, vulnerability database updated **2 November 2026,
13:15:56 UTC**. All severities were retained. Scans completed on 3 November
between 10:26 and 10:37 UTC. Images were exported to archives and scanned without
giving Trivy the Docker socket. Every report's DiffIDs were compared with the
corresponding image's RootFS layers. No ignore or VEX suppression was added.

The table counts package/version/path/advisory records; repeated CVEs in
different packages are separate records. UNKNOWN is an unassigned severity,
not a statement of low risk. A listed fix does not establish application-level
exploitability; an absent fix does not establish safety.

| Image snapshot | CRITICAL | HIGH | MEDIUM | LOW | UNKNOWN | Records with listed fixes |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| Running web | 1 | 43 | 131 | 150 | 2 | 2 MEDIUM, 1 UNKNOWN |
| Running accounts | 2 | 36 | 121 | 106 | 9 | 5 MEDIUM, 1 LOW, 1 UNKNOWN |
| Running PostgreSQL | 3 | 46 | 172 | 168 | 13 | 3 MEDIUM, 2 UNKNOWN |
| Running gateway | 0 | 1 | 3 | 0 | 3 | 1 MEDIUM, 2 UNKNOWN |
| Monitor before promotion, Axios image | 6 | 87 | 235 | 194 | 22 | 1 CRITICAL, 14 HIGH, 25 MEDIUM, 2 LOW |
| Previous MSAL monitor candidate | 5 | 73 | 217 | 193 | 22 | 7 MEDIUM, 1 LOW |
| New review monitor candidate | 5 | 73 | 213 | 192 | 22 | 3 MEDIUM |

Exact image IDs at inspection:

| Snapshot | Tag | Image ID (`sha256:`) |
| --- | --- | --- |
| Web | `numforge-web:expiry-0036cbf` | `733aa0787a6588add01c3f44077aa2da1950577fd7236800dcdfa4d935f27ab0` |
| Accounts | `numforge-accounts:preferences-bdf5a39` | `76a1149080c28e0975f8a2de47be8d96156294c11f054b12ed13aa8cef5b1061` |
| PostgreSQL | `numforge-postgres:security-20261030` | `fd90150e2c2c46df35abfed797337bac797127ab0ab54a63dde95f36a54d1170` |
| Gateway | `numforge-gateway:security-20261030` | `5b357e6a0a9b89b6546224d79c063a551ff7583d92173293128f7639a9833a86` |
| Monitor before promotion | `numforge-uptime:axios-20261103` | `9f06bd0d806cd2261571de48c465d19ea8677f122de37a938d15f5b7d6755307` |
| MSAL candidate | `numforge-uptime:candidate-msal-20261103` | `0650f0ed652303207246c4799dae5bf552d14d0526ec43930765e34154aa7990` |
| Review candidate | `numforge-uptime:candidate-review-20261103` | `7db293cb9d838a0a200a6bd3f08a4f66fdfc9b88aad9e3ffe33c39217e2f63c8` |

All five pilot services were healthy at inventory. Web/accounts/gateway/monitor
had read-only root filesystems; PostgreSQL did not. Inspection selected image
and health fields only. No production database, token or credential was used in
the probes or regression fixtures.

## Qualified fixes

`qs` changes **6.14.2 → 6.16.1**, with an explicit direct Kuma range migration
and SHA512 pin. Express and body-parser already had nested 6.16.1 copies; these
are deduplicated into the root while checking their identities and resolution.
The change removes CVE-2026-110816, CVE-2026-82417, CVE-2026-82562 and
CVE-2026-8723 from the scan. Checks cover hostile comma serialization values,
null/undefined values, non-callable `isBuffer`, repeated and bracket-key comma array limits,
normal nesting/Unicode, prototype protection and actual Express query/body
parsing with parameter/body limits. Some unsafe serialization inputs are omitted
by the fixed library; the regression also verifies that a benign sibling survives.
This does not claim that every advisory was reachable through the old Kuma UI.
[Upstream comma serialization advisory](https://github.com/ljharb/qs/security/advisories/GHSA-6pq8-jhm2-2vcw).
[Bracket-key comma limit advisory](https://github.com/ljharb/qs/security/advisories/GHSA-x5fp-wj9c-mxmx).

`@tootallnate/once` changes **1.1.2 → 2.0.1** through the existing consumer-major
migration policy. The sole installed consumer is the nested
`http-proxy-agent` **4.0.1** under make-fetch-happen, whose original range `1`,
version and archive integrity are checked. It awaits connection and ignores
the returned event payload. The old package also exposes cancellation/payload
APIs that are not equivalent to v2; this qualifies that consumer, not every v1
caller. Tests cover ordinary event/error cleanup, already-aborted and later-
aborted signals, and the actual legacy agent's routing and dummy Basic proxy
authentication. CVE-2026-3449 disappears. The old v1 package lacks AbortSignal
support; no AbortSignal-based Kuma exploit through that proxy caller was found.
[Upstream fix](https://github.com/TooTallNate/once/commit/b9f43cc5259bee2952d91ad3cdbd201a82df448a).

The migration guard now treats an absent package `dependencies` declaration as
an empty map for leaf migrations. Explicit `from: {}` and `to: {}` declarations
are still required by the manifest. Two guard checks verify the supported leaf
case and rejection of a missing old map. Optional dependencies, consumers,
archive identities, dependency resolution and unrelated metadata remain guarded.

Standalone tests run in network-disabled, read-only containers with a 256 MiB
memory limit, temporary `/tmp`, dropped capabilities and no host mounts. The old
MSAL image passes 190 and fails eight of the forced 198 checks; the new image
passes all 198. The separate Compose fixture passes 186 applicable checks on
each old-image stage and 198 on each candidate stage. Old → new → old → new
preserves stored user/configuration/history hashes, authenticated login,
keyword and push up/down behavior, and runtime protections. Its containers,
volume and network were verified absent after cleanup.

## Original monitor findings and later disposition

| Package | Advisory | Verification and disposition |
| --- | --- | --- |
| RedBean-owned glob 10.5.0 | CVE-2026-118773, fix 14.0.2 | Confirmed library cancellation flaw: an 85-directory fixture performed 80 additional directory reads after the aborted promise rejected. RedBean's inspected call sites use synchronous model-file discovery without a signal. Subsequently resolved by the qualified glob 14.0.2 deployment. |
| RedBean-owned minimatch 9.0.9 | CVE-2026-118764, fix 11.0.1 | Confirmed optimizer amplification. With `optimizationLevel: 2`, 2/4/6/8/10 distinct `**/../xN/` segments produced 4/16/64/256/1024 globparts. The largest bounded probe took about 110 ms. Subsequently resolved by minimatch 11.0.1 and the default-entry adaptations in the qualified deployment. |
| Legacy glob-owned minimatch 3.1.5 | Same advisory and scanner fix | The installed source and prototype lack `firstPhasePreProcess`, which the publisher's description requires. The same bounded probes produced one globparts entry each. The publisher identifies 10.2.6 while the advisory database applies `<11.0.1`. Treat as a disputed range, retain the record, and seek an upstream clarification. |

Probes used a network-disabled read-only container with 192 MiB memory; individual
minimatch children had a 64 MiB heap and 1.5-second deadline. Temporary directories
were under container `/tmp`. A major replacement of legacy callable minimatch or
glob APIs requires consumer qualification; changing overrides only to silence the
scanner would not establish compatibility.
[Glob publisher advisory](https://github.com/isaacs/node-glob/security/advisories/GHSA-3g3m-2fjr-6pw4),
[minimatch publisher advisory](https://github.com/isaacs/minimatch/security/advisories/GHSA-g3hv-75wg-m249),
[broader database range](https://github.com/advisories/GHSA-g3hv-75wg-m249).

### Native SQLite requires separate work

This is the original audit snapshot. A subsequent
[qualified source rebuild](../guides/CONTAINERS.md#native-sqlite-source-rebuild)
updates the candidate's native engine to 3.54.1 and records its provenance and
compatibility checks separately; deployment status is recorded with that rebuild.

The real Node module `@louislam/sqlite3` **15.1.6** reports SQLite **3.41.1**,
source ID `2023-03-10 12:13:52 20399f3eda5ec249d147ba9e48da6e87f969d7966a9a896764ca437ff7e737ff`.
Its loaded `node_sqlite3.node` does not link to the Debian `libsqlite3` according
to `ldd`; the package includes `sqlite-autoconf-3410100.tar.gz`. The inspected
bundled source contains the old `sqlite3KeyInfoFromExprList` implementation.
Runtime compile options include `MAX_COLUMN=2000` and `MAX_EXPR_DEPTH=1000`.

SQLite 3.41.1 falls in the published affected range for
[CVE-2025-7458](https://security-tracker.debian.org/tracker/CVE-2025-7458).
The Trivy npm inventory does not separately report this embedded engine; the
Debian SQLite records concern different binaries. Native version/source checks
therefore expose a coverage gap behind the zero fixable-HIGH/CRITICAL metric.
No arbitrary-SQL exploit through Kuma was demonstrated, and version inspection
does not by itself establish every backport or exploit precondition. Treat the
native database dependency as an unresolved review and migration priority. A
replacement needs binary provenance, supported Node ABI, database integrity,
migrations and rollback qualification, rather than an OS package update alone.
The [fork repository](https://github.com/louislam/node-sqlite3) was archived on
21 September 2026, adding a maintenance concern.

## Other running images: complete fixable inventory

These are findings in the frozen running images, not newly qualified replacements.
The four images have **11 MEDIUM, 1 LOW and 6 UNKNOWN** fixable records, with no
fixable HIGH/CRITICAL record in this database.

| Images | Package/version | Listed fix | Records and review |
| --- | --- | --- | --- |
| Web, PostgreSQL | libsasl2-2 and libsasl2-modules-db 2.1.28+dfsg-10 | 2.1.28+dfsg-10+deb12u1 | Four MEDIUM records for CVE-2026-107161. Package file lists and standard SASL plugin directories contain only the sasldb plugin, with no DIGEST-MD5 plugin in the inspected locations of either frozen image. This limits applicability of the described plugin flaw. Retain the records and refresh/rebuild both images for package maintenance. |
| Web, accounts, PostgreSQL | tzdata 2026e-0+deb12u1 | 2026f-0+deb12u1 | Three UNKNOWN records for DLA-4867-1, a timezone database update. Keep separate from vulnerability severity assumptions. |
| Accounts | pip 25.0.1 | 26.2.0 covers the listed version thresholds | Five MEDIUM records: CVE-2025-8869, CVE-2026-13346, CVE-2026-3219, CVE-2026-6357, CVE-2026-8643; one LOW record: CVE-2026-1703. Dockerfile uses pip for installation during build; its runtime command starts Gunicorn. Review build inputs and upgrade/remove the runtime installer in a separately tested image. |
| PostgreSQL gosu | github.com/moby/sys/user v0.1.0 | 0.4.1 | One MEDIUM record, CVE-2026-61801. Requires attacker-influenced user/group database files; do not treat this as an unauthenticated PostgreSQL protocol exploit. Rebuild and qualify the gosu dependency. |
| PostgreSQL gosu | golang.org/x/sys v0.1.0 | 0.44.0 | One UNKNOWN record, CVE-2026-39824. The official affected function is Windows-only `NewNTUnicodeString`; this image is Linux. Platform evidence limits applicability without suppressing the inventory record. |
| Gateway | github.com/go-chi/chi/v5 v5.3.2 | 5.4.0 | One MEDIUM record, CVE-2026-114437. Requires use of `ClientIPFromXFF` with an untrusted direct peer. The configured Caddy routes do not establish that middleware's reachability; inspect linked callers and qualify an update. |
| Gateway | golang.org/x/net v0.60.0 | 0.62.0 | Two UNKNOWN records, CVE-2026-78661 and CVE-2026-97045. Official reports concern HTML parsing/rendering, not every x/net or HTTP use. Module inventory alone does not prove a reachable gateway path. |

Primary references:
[Debian SASL status](https://security-tracker.debian.org/tracker/CVE-2026-107161),
[pip installation-path fix](https://github.com/pypa/pip/pull/14110),
[moby user/group parsing advisory](https://github.com/moby/sys/security/advisories/GHSA-mjcv-p78q-w5fw),
[Go Windows-only report](https://pkg.go.dev/vuln/GO-2026-5024),
[chi advisory](https://github.com/go-chi/chi/security/advisories/GHSA-j6qf-3m9c-vwj3),
[Go HTML parse report](https://pkg.go.dev/vuln/GO-2026-6860),
[Go HTML render report](https://pkg.go.dev/vuln/GO-2026-6866).

## Unfixed findings and limits

The candidate retains **502 Debian records** without a listed fix: 322
`affected`, 144 `fix_deferred`, 36 `will_not_fix`. They include 73 HIGH and
five CRITICAL records. The five CRITICAL records are SQLite CVE-2025-7458 in
two packages, PyJWT CVE-2026-102267 and CVE-2026-102268, and zlib
CVE-2023-45853. The accounts and PostgreSQL images also retain system SQLite
records; PostgreSQL additionally has libxml2 CVE-2026-6653.

Debian's SQLite status is vulnerable with no Bookworm security update listed.
The PyJWT issues are deferred and depend on specific JWKS redirect or mixed
algorithm/key handling. The libxml2 issue is also deferred. These need
feature/reachability review and a maintained base-image plan; package removal or
mixing in Debian unstable libraries is not qualified by this audit.
[SQLite status](https://security-tracker.debian.org/tracker/CVE-2025-7458),
[PyJWT redirect status](https://security-tracker.debian.org/tracker/CVE-2026-102267),
[PyJWT key status](https://security-tracker.debian.org/tracker/CVE-2026-102268),
[libxml2 status](https://security-tracker.debian.org/tracker/CVE-2026-6653).

For zlib CVE-2023-45853, Debian explicitly says the affected contrib/minizip
code is not built in the Bookworm zlib binaries. This is material vendor
context for the scanner's CRITICAL label, not a reason to ignore other zlib,
SQLite or Python findings.
[Debian zlib/minizip explanation](https://security-tracker.debian.org/tracker/CVE-2023-45853).

The current CI scan gate fails on fixable HIGH/CRITICAL records only; it does
not fail on MEDIUM/LOW/UNKNOWN or unfixed records. Keep the all-severity report
in reviews. An unchanged scan result also cannot certify native libraries,
every optional notification/database feature, supply-chain provenance, Windows
reboot behavior or a future deployment. No production promotion or push took
place during the initial audit; the subsequent authorized promotion follows.

## Qualified monitor promotion

After explicit approval of the monitor replacement and the sensitive backup
payload/destination, the existing review image was promoted on **3 November 2026
at 11:07:52 UTC** as `numforge-uptime:review-20261103`, from source commit
`6752d74`. Its exact image ID remains
`sha256:7db293cb9d838a0a200a6bd3f08a4f66fdfc9b88aad9e3ffe33c39217e2f63c8`.
There was no rebuild. The previous Axios image and runtime profile remain
available for rollback.

A further isolated rehearsal exercised the direct Axios → review → Axios →
review sequence, preserving users/configuration/history hashes and authenticated
keyword/push behavior. Both review stages passed all 198 checks; the old image
passed its 139 applicable checks. Its containers, volume and network were
verified absent after cleanup.

Under the supervisor operation lock, only the uptime image in the runtime
profile changed; the effective Compose services differed only by that image.
Only uptime was recreated. Web/accounts/PostgreSQL/gateway container IDs and
the owned tunnel identity/URL remained unchanged. The live package receipt,
all patched/added package versions and lock integrity values, previous dependency
sharing and adapted source hashes matched the qualified source. Stored users,
complete monitor settings and retained heartbeat history matched the consistent
pre-change SQLite copy. Non-root/read-only/capability protections and the
loopback-only admin binding passed.

Before and after replacement, standard account/configuration/committed-source
backups and separately tagged consistent monitor SQLite snapshots went to the
existing external encrypted repository. Both pairs passed byte-for-byte restore,
SQLite integrity/stored-state checks, repository checks and comparison of all
eleven account tables and their rows with the unchanged source. All four restore
points were retained at completion. Daily standard retention can later rotate
standard points; the daily job does not refresh monitor SQLite.

Local pinned-TLS and public HTTPS calculation/session checks passed. Six monitors
were up with fresh non-push checks. A subsequent normal supervisor Ensure
preserved the new image, other service identities and public URL; all five
services were healthy and all six monitors remained up.

For these exact old/new images, the matched reports show fixable HIGH/CRITICAL
**15 → 0**, fixable MEDIUM **25 → 3** and fixable LOW **2 → 0**. The unchanged
database still reports 73 HIGH and five CRITICAL OS records without a listed fix.
At this promotion, the native SQLite coverage gap and remaining glob/minimatch/
other-image findings above remained open. A subsequent native source rebuild
and promotion is linked near the top of this report. No other service update,
push or reboot was performed.
Promotion evidence is under ignored `build/security-review-deploy-20261103/`:
`DEPLOYMENT.md`, `direct-upgrade.log`, `deployment.json`, `pre-backup.json`,
`post-backup.json`, `live-package-pins.json`, `deployment-scan-comparison.json`
and `final-check.json`. Private SQLite copies remain in the protected operations
directory, outside these public records.

## Evidence

Strict documentation generation also caught a broken roadmap link to the Docker
pilot measurements: GitHub and MkDocs generated different anchors around the
heading's dash. The heading and internal link now use a shared anchor spelling.

Raw reports, builds, red/green checks, lifecycle logs, version/source probes,
source hashes and summaries are under ignored `build/security-review-20261103/`.
Key files: `scan-summary.json`, `stack-summary.json`, `live-images.json`,
`scanner-version.json`, `qualified-source.json`, `red-tests.log`,
`green-tests.log`, `upgrade.log`, `probes.cjs`, `probes.log`, `sqlite.log` and
`sasl-files.log`.
The qualified changes are in [the patch manifest](../../deploy/monitor-patches.json),
[migration guard](../../deploy/patch_monitor.cjs) and
[regression checks](../../tests/containers/check_monitor_lower.cjs).
The [container guide](../guides/CONTAINERS.md) records the preceding qualified
migrations and their deployment status.
