# Accounts system SQLite backport — 4 November 2026

The Bookworm accounts image receives the upstream fix for CVE-2025-7458 while
retaining SQLite 3.40.1, the original OS packages, Python application files and
SQLite compile options. This avoids the util-linux regression found in the
[Trixie experiment](SECURITY_MAINTENANCE_20261103.md#accounts-trixie-qualification-on-4-november).
The accounts database remains PostgreSQL; this repair concerns Python's separate
standard-library SQLite dependency.

## Source and scope

Debian source package `sqlite3=3.40.1-2+deb12u2` is fetched through authenticated
APT source indexes. The four-line upstream change clamps the satisfied ORDER BY
term count to the actual ORDER BY length in `src/where.c`. The official Git
mirror commit is `b816ca9994e03a8bc829b49452b8158a731e81a9`; its `manifest.uuid`
identifies Fossil check-in
`12ad822d9b827777526ca5ed5bf3e678d600294fc9b5c25482dfff2a021328a4`.
The upstream description, timestamp and regression cases agree across mirrors.
The Fossil patch endpoint returned browser-verification HTML; that response was
retained as evidence and was not treated as a patch.

The [build helper](../../deploy/backport_accounts_sqlite.py) checks the full
source-file SHA256 and Debian changelog version, requires exactly one original
context match, and records before/after source hashes and upstream identities.
Changed-source testing confirms rejection before writing either a source file
or receipt. The [maintenance Dockerfile](../../deploy/accounts-sqlite.Dockerfile)
also requires the original installed library hash. Unexpected source or binary
preimages stop the build.

Debian's package build rules retain their compiler flags and feature choices.
Compilation runs without network access, with two jobs, in a separate build
stage. `DEB_BUILD_OPTIONS=nocheck` skips the full upstream package test suite;
the bounded native and application qualification described below is run
separately. Only `libsqlite3.so.0.8.6` and the provenance receipt are copied into
the original accounts image. Build tools and generated CLI/debug/development
packages stay outside the runtime. This qualification covers Linux amd64.

| Identity | SHA256 |
| --- | --- |
| Original `where.c` | `7ef061e8cb2d2bcfda2cb7153536f7b47bae055bb6c42e2f7de7e5292e12d991` |
| Patched `where.c` | `4dd1a1468693239801037c1d0decd9eaac0db83f9225997404afb590b5b79722` |
| Original shared library | `2e6eef9a727f081f0d453b4e5e6cbd8b9ef8b6f86cbf7681cbad444d3b0b55c8` |
| Rebuilt shared library | `f4b32d83fadc4787cabff8a115e4aca3ac808efe9d9521b9630f1d8815f129b5` |

The frozen base is the deployed accounts maintenance image
`sha256:a9fdf6b585f8731a4951ec144b275561073b899429bb4a8278bc1785b123fe55`.
Its local `numforge-accounts:maintenance-20261103` tag must still resolve to that
identity when reproducing the overlay. This overlay preserves the existing
application. The ordinary current-source accounts Dockerfile also builds the
backport, as recorded below, so subsequent application builds retain the repair.

## Verification

A bounded standalone upstream SELECT DISTINCT/ORDER BY case crashes the original
library (container exit 139). The bounded four-test baseline suite instead
returns incorrect query rows and fails that assertion; undefined behavior need
not produce a stable crash. Neither probe mounts production data or has network
access. The patched library passes the same regression at multiple column/order
counts, EXPLAIN and CREATE TABLE AS variants.

The [four native checks](../../tests/containers/check_accounts_sqlite.py) also
cover bound Unicode/blob values, foreign keys, transaction rollback, JSON/math,
FTS5/RTree, two-connection WAL, consistent backup and read-only restore/integrity.
They require an explicitly marked disposable Docker container. Every original
compile option, including extension support and SQL limits, matches exactly;
every `/service` application file hash is preserved. The final guarded build
produces the same library bytes as the exploratory candidate that first passed
the complete application fixture.

The preserved-release fixture exercises 15 account/preference tests, both
account/preferences browser suites, authentication/CSRF/secure cookies/session
revocation, DB restart, eleven-table/row backup restoration and TLS. It also
checks forty real HTTPS probes without retained gateway zombies. Fixture
resources belong to a dedicated test project and are removed afterward.

The exact final artifact is
`numforge-accounts:qualified-sqlite-backport-20261104`, image ID
`sha256:2a49c956fe24a6c3721453d9dc8aed3c8e439346b64d5ae6979ad808d5071d2b`.

## Scanner interpretation and evidence

The distribution version and dpkg inventory deliberately remain
`3.40.1-2+deb12u2`; they do not claim to be a new Debian release. The private
backport is identified by the receipt and actual source/binary hashes.
Version-based scans consequently retain the SQLite CVE record. No ignore or
VEX suppression is added, and no reduction in scanner counts is attributed to
this fix. Native source and functional verification establish the backport.
Matched Trivy 0.76.1 reports for the exact old/final images have identical
package/version/path/advisory inventories: 2 CRITICAL, 36 HIGH, 117 MEDIUM,
105 LOW and 8 UNKNOWN, with no listed fixes at any severity. Report DiffIDs
match each image's actual layers. No record is added or removed.
The other SQLite advisories and remaining OS records are not claimed fixed by
this single upstream change.

## Promotion and recovery verification

On **4 November 2026 at 19:40:01 CET (18:40:01 UTC)**, the exact qualified
artifact above was deployed without rebuilding as
`numforge-accounts:sqlite-backport-20261104`, from source commit `c188dcd`.
Under the supervisor operation lock, only the accounts service was recreated;
effective service definitions changed only accounts/setup image selections.
The prior runtime profile and maintenance image remain available for rollback.

Live verification confirms the rebuilt library hash and successful execution
of the previously crashing DISTINCT/ORDER BY query in an in-memory SQLite
database. Eleven account tables and complete row aggregates match their
pre-change read-only hashes. No account migration was run. Non-root/read-only/
capability bounds and all mounts remain intact. Web, PostgreSQL, gateway and
monitor container identities stayed unchanged, as did the tunnel identity/URL.
Pinned local TLS and public calculation/session checks passed.

Standard account/configuration/committed-Git backups and separate consistent
monitor SQLite snapshots were taken before and after replacement in the
existing external encrypted repository. Both pairs passed byte-for-byte
restore, all eleven account table/row comparisons, SQLite integrity/stored-state
verification and repository checks. All four restore points were retained at
completion; standard daily retention can later rotate standard points, while
monitor SQLite remains a separate snapshot operation.

A normal supervisor Ensure preserved the selected image, fixed library bytes,
original account data/schema and the other service identities. All five
services were healthy and six monitors up. The disposable backport test project
was verified absent. No push or reboot was performed. Deployment records are
under ignored `build/security-accounts-sqlite-backport-20261104/deployment/`,
including before/pre/post records, deployment result and final check. Sensitive
database copies remain in the protected operations directory.

Strict documentation generation also caught a GitHub/MkDocs anchor difference
around the Trixie heading's dash; its heading and reference now use plain words.

## Permanent build and CI integration on 5 November

The ordinary [accounts Dockerfile](../../deploy/accounts.Dockerfile) now builds
the same backport from the official Python Bookworm base in a separate stage.
It does not require the frozen local maintenance image. Source and installed
library preimage guards are retained, so an unexpected distribution update
requires review before the backport is applied. The rebuilt runtime library
hash and all compile options match the deployed qualified library exactly.

The [stack integration check](../../tests/containers/check_stack.py) executes
all four bounded native regressions immediately after startup. An unpatched
image fails the DISTINCT/ORDER BY case rather than passing merely because a
version-based vulnerability gate has no listed fix. The original image's
negative control fails that assertion while its other three native checks pass.
The container CI time budget increases from 25 to 40 minutes to accommodate the
added distribution source build; test assertions and vulnerability thresholds
remain intact.

The fresh ordinary build, image
`sha256:624b14979f7a831c4c35b856d20d8aec0c84f10fd3d1e3ade044f1c0a2cc2aa9`,
passes the four native checks, all 22 current account/preference/workspace tests,
all three browser suites, TLS/session and restart checks, and restoration of all
twelve fixture tables/rows. This exercises current source and its workspace
schema only in the disposable fixture. Production retains its selected image
and eleven-table schema. All owned fixture resources were removed.

Matched Trivy 0.76.1 scans use the database updated 5 November at 13:09:41 UTC.
The ordinary build has no listed fixes at any severity; its totals are two
CRITICAL, 37 HIGH, 117 MEDIUM, 105 LOW and six UNKNOWN. The fresh production
baseline has 39 HIGH, including two newly listed fixable APT records, with the
other totals identical. The ordinary build upgrades APT/libapt-pkg to 2.6.2,
removing CVE-2026-109518. The retained CVE-2011-3374 records change installed
version but are not new advisories. Production APT remediation is separate work;
this CI qualification does not deploy the newer workspace application.

At the start of local validation the Docker engine was stopped. Its standard
start and Ensure restored the selected five healthy service images and six up
monitors; the cause of the stop is unconfirmed. Evidence is under ignored
`build/security-accounts-ci-20261105/`: native inventory, negative control,
build/fixture logs, matched reports, scanner metadata and live-image check.
The GitHub-hosted workflow was not run; the same stack checks were executed
locally against frozen artifacts, without a push.

APT source: [Debian CVE-2026-109518 status](https://security-tracker.debian.org/tracker/CVE-2026-109518).

Evidence is under ignored `build/security-accounts-sqlite-backport-20261104/`:
upstream pages/mirror metadata, exploratory and final build logs, source guard,
red/green native logs, exact inventories, fixture results and matched reports.

Primary references: [Debian CVE status](https://security-tracker.debian.org/tracker/CVE-2025-7458),
[official upstream mirror change](https://github.com/sqlite/sqlite/commit/b816ca9994e03a8bc829b49452b8158a731e81a9),
[Fossil check-in](https://www.sqlite.org/src/info/12ad822d9b827777).
