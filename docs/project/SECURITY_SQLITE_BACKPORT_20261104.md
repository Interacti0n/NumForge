# Accounts system SQLite backport — 4 November 2026

The Bookworm accounts image receives the upstream fix for CVE-2025-7458 while
retaining SQLite 3.40.1, the original OS packages, Python application files and
SQLite compile options. This avoids the util-linux regression found in the
[Trixie experiment](SECURITY_MAINTENANCE_20261103.md#accounts-trixie-qualification--4-november).
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
identity when reproducing the overlay. This is a maintenance build for the
existing application, rather than a replacement for the ordinary current-source
accounts Dockerfile; future application images need the backport reapplied and
qualified until the distribution supplies a suitable fix.

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

Evidence is under ignored `build/security-accounts-sqlite-backport-20261104/`:
upstream pages/mirror metadata, exploratory and final build logs, source guard,
red/green native logs, exact inventories, fixture results and matched reports.

Primary references: [Debian CVE status](https://security-tracker.debian.org/tracker/CVE-2025-7458),
[official upstream mirror change](https://github.com/sqlite/sqlite/commit/b816ca9994e03a8bc829b49452b8158a731e81a9),
[Fossil check-in](https://www.sqlite.org/src/info/12ad822d9b827777).
