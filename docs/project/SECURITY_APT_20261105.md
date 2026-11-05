# Bookworm APT maintenance — 5 November 2026

Four qualified maintenance images upgrade **apt and libapt-pkg6.0 from 2.6.1
to 2.6.2**, removing eight HIGH records for CVE-2026-109518. The application
files, account schema, PostgreSQL/gosu binaries, both qualified SQLite repairs,
monitor package receipts and image configuration remain unchanged. Gateway is
unchanged and has a fresh scan with no records carrying listed fixes.

The later [monitor system helper repair](SECURITY_SYSTEM_HELPERS_20261105.md)
adds separately qualified SQLite/PyJWT backports. The exact APT artifacts and
matched counts below remain the record of this earlier maintenance step.

## Bootstrap protection and scope

Debian identifies 2.6.2 as the Bookworm fix. The advisory concerns package
installation through manipulated HTTP redirects/retries and reused hard links;
non-zero `Acquire::Retries` is a prerequisite. Both maintenance recipes use
`-o Acquire::Retries=0` for update and install while bootstrapping the fixed
APT itself. They retain signature verification and install the exact two fixed
package versions. Runtime retry configuration is not changed.

The same per-command retry override is applied to APT invocations in the
ordinary web/accounts/PostgreSQL/monitor Dockerfiles and the SQLite overlay.
The accounts source-download/pinning prefix was rebuilt with the override through
the backport source guard. It produces the same patched source hash as the
fully tested ordinary CI build; its native compilation inputs and command are
unchanged. This validates the changed download path without recompiling an
unchanged native library again.

The [non-root maintenance recipe](../../deploy/apt-maintenance.Dockerfile)
temporarily selects root and restores the parent's exact image USER. The
[PostgreSQL recipe](../../deploy/postgres-apt.Dockerfile) retains its existing
root entrypoint and gosu privilege drop without changing USER metadata.
Runtime Compose user overrides are preserved separately; the monitor's numeric
runtime user must not be confused with its image's `node` USER. Qualification
requires complete before/after image Config equality and original layer prefixes.

Source: [Debian advisory and fixed versions](https://security-tracker.debian.org/tracker/CVE-2026-109518).

## Exact artifacts and verification

| Service | Qualified image ID (`sha256:`) |
| --- | --- |
| Web | `1706436e976c0461df1bfe5cf5c8698a2d55e8768d30965e3142ae6b702e4321` |
| Accounts | `3080228383f4dc95c8f11940f10f9fe81c389b95cca39b706979ba422ace30eb` |
| PostgreSQL | `7cd98b66214c355a7b7724c26c17dabef053c7d77df995bd656fa2ff5e1cad0b` |
| Monitor | `0c0c6652d3dba8908c5c71ff0c1a3f0ae05ebc2f56aeb49da43044752cc0a911` |

All installed package maps differ only in the two declared APT versions.
Actual `apt-get --version` reports 2.6.2 in each candidate. Web/calculator and
account source hashes, account SQLite library/receipt, PostgreSQL/gosu binaries,
and monitor native binary/package receipts match their frozen parents.

The shared disposable stack passes 15 preserved-release account/preference
tests, both browser suites, TLS/CSRF/cookie/session checks, DB restart recovery,
restoration of all eleven tables/rows, DB-independent calculation and forty
HTTPS probes without retained gateway zombies. The accounts image also passes
all four bounded SQLite native regressions. The monitor separately passes
**230 checks**, authenticated provisioning, keyword/push up/down behavior,
restart and persistence. Both owned fixture projects, networks and volumes
are verified absent after cleanup.

This maintenance preserves the deployed application. The separately qualified
[ordinary CI build](SECURITY_SQLITE_BACKPORT_20261104.md#permanent-build-and-ci-integration-on-5-november)
exercises current workspace functionality only in its own fixture; no workspace
migration is required for the APT overlays.

## Matched scan results

Trivy 0.76.1 uses the database updated **5 November at 13:09:41 UTC**. Every
report's DiffIDs match its image layers. Comparisons use type/package/path/
advisory keys and check severity equality for retained records; a package version
change alone is not attributed as a newly introduced advisory.

| Updated service | CRITICAL | HIGH | MEDIUM | LOW | UNKNOWN | Records with listed fixes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Web | 1 | 44 | 130 | 149 | 0 | 0 |
| Accounts | 2 | 37 | 117 | 105 | 6 | 0 |
| PostgreSQL | 3 | 47 | 173 | 168 | 6 | 0 |
| Monitor | 5 | 77 | 210 | 193 | 17 | 0 |

Each matched parent has two additional HIGH records, both CVE-2026-109518.
Exactly those eight records disappear, with no new advisories or retained-record
severity increases. The CVE-2011-3374 records remain against the new APT versions.
No suppression or vulnerability threshold change was added.

The previously disputed minimatch 3.1.5 attribution is independently resolved by
the advisory database's 4 November update: affected versions now begin at 7.0.0.
Both old/new monitor scans still inventory minimatch 3.1.5 and 11.0.1 among 647
Node packages, but report no npm vulnerabilities. This is a range correction,
not an APT fix or a newly replaced legacy matcher.
[Updated minimatch advisory](https://github.com/advisories/GHSA-g3hv-75wg-m249).

Zero listed fixes is not zero vulnerabilities. Unfixed OS records remain visible,
including the system SQLite/libxml2/PyJWT work recorded in earlier reviews.
Version-based scans also retain the private accounts SQLite backport record;
its source/binary and functional verification remain necessary.

## Promotion and recovery verification

The four qualified images were deployed on **5 November at 18:16:06 CET
(17:16:06 UTC)** from source commit `3d55e07`, using the service tags
`numforge-web:apt-20261105`, `numforge-accounts:apt-20261105`,
`numforge-postgres:apt-20261105` and `numforge-uptime:apt-20261105`.
The frozen parent images remain available for rollback.

Only the four intended services were recreated under the supervisor lock.
Effective Compose changes are limited to their images and the accounts setup
image. Users, mounts and service protections are preserved. The gateway container,
CA and public tunnel URL are unchanged. Live checks confirm APT 2.6.2, the
preserved application files, PostgreSQL/gosu binaries, both SQLite repairs,
original account data and monitor history. The live schema still has eleven
tables; the newer workspace application was not deployed.

Standard application and monitor backups were taken before and after deployment
to the authorized encrypted repository. All four restore points were restored
and verified, including all eleven account tables and their rows, monitor
database integrity and repository checks. All four restore points were retained.

The normal Ensure run and final check passed: **five healthy services and six
UP monitors**, working HTTPS, preserved data and repairs, and matching deployed
image identities. The fixture's forty HTTPS probes retained no gateway zombies.
All three disposable qualification stacks, networks and volumes are absent.
No push or reboot was performed.

One audit limitation occurred during console-log redirection: the APT launcher
used the older `build/security-stack-20261103/deployment/deployment.log` path,
overwriting that historical console stream. The complete new stream was copied
to the correct APT deployment directory and the launcher path corrected. The
original console stream cannot be recovered. The older structured deployment,
final-check and backup records were preserved; deployment and data were unaffected.
An explanatory placeholder and `log-location-note.txt` record the mistake.

Evidence is under ignored `build/security-apt-20261105/`: frozen identities,
build/configuration checks, exact inventories, native and fixture logs, both
monitor/application cleanup records, bootstrap receipt, matched reports and
scanner metadata. The unchanged gateway was refreshed against the same database.
Its `deployment/` directory contains before-state and mount checks, pre/post
backup verification, deployment and final-check records, the complete current
console log and the log-location note. Private restored data remains under the
existing local operations storage and is not committed.
