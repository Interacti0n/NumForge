# PostgreSQL XML qualification on 5 November 2026

The qualified maintenance image replaces PostgreSQL's system libxml2 with an
authenticated 2.13.9 source build and the reader/schema callback repair from
Alpine's maintained 2.13.9-r2 package, adapted to the release archive. It retains
the existing Bookworm/glibc baseline, PostgreSQL 18.6 binaries and data format,
XSLT library, image configuration and all original XML feature bits.

This addresses the CVE-2026-6653 code carried by Debian's older library without
changing database text semantics. Debian still lists Bookworm and Trixie as
vulnerable and identifies the upstream fix as present from 2.11.0. This private
build does not change Debian's installed package version or suppress its records.

Sources: [Debian applicability](https://security-tracker.debian.org/tracker/CVE-2026-6653),
[official release checksums](https://download.gnome.org/sources/libxml2/2.13/libxml2-2.13.9.sha256sum),
[vendor recipe](https://github.com/alpinelinux/aports/blob/35f5eb2864e50dd4448758d31f3e2a32f733ab84/main/libxml2/APKBUILD).

## Rejected whole-base alternative

The official `postgres:18.6-alpine3.24` image was tested first. Its original
scan has two CRITICAL and 26 HIGH records with listed fixes. The
[candidate recipe](../../deploy/postgres-alpine-candidate.Dockerfile) upgrades
Alpine packages and reuses the already qualified static gosu binary. Its fresh
scan reports no known vulnerabilities, with no suppression.

Despite passing XML/XSLT, logical restore/rollback, 22 current API tests, three
browser suites and twelve-table restore, it fails the text compatibility gate.
Both databases say `en_US.utf8`, but the live glibc locale has version 2.36,
whereas the musl candidate has different behavior. A 22-value Unicode corpus
changes ordering; uppercasing `straße` changes from `STRAßE` to `STRAẞE`.
This does not demonstrate a current login defect, but does demonstrate changed
SQL text behavior that ordinary API tests miss. Alpine was **not promoted**.

The [migration fixture](../../tests/containers/check_postgres_xml.py) records
locale metadata and ordering/classification separately from row preservation. With
`--require-compatible` it exits 2 for this candidate. A same-image control
passes with exit 0. Both use only newly created, ownership-labelled fixture
volumes; neither mounts the live data volume. The rollback exercise restores
candidate writes into a separate original-image database, preserving the
original fixture database throughout.

Sources: [official PostgreSQL variants](https://hub.docker.com/_/postgres),
[PostgreSQL collation semantics](https://www.postgresql.org/docs/18/collation.html).

## Same-system source and ABI controls

The [manifest](../../deploy/postgres-libxml.json) pins the official source
archive SHA-256 `a2c9ae7b770da34860050c309f903221c67830c86e4a7e760692b803df95143a`
and both vendor patch SHA-512 values at immutable commit
`35f5eb2864e50dd4448758d31f3e2a32f733ab84`. Downloads are verified before use;
compilation, negative controls, patching and upstream tests run without a network.

The vendor code patch's header/context does not match the release archive: it
uses a newer NOCDATA branch condition. The recipe does not loosen the preimage
check or apply a fuzzy patch. The [build helper](../../deploy/build_postgres_libxml.py)
instead adapts exactly four callback arguments from `ctxt` to `ctxt->userData`,
preserving both older branch conditions. Exact release/parser blob hashes,
two occurrences per callback, and the resulting blob hash are checked. The
vendor regression-test patch applies unchanged with zero fuzz. The receipt
explicitly states that the code patch was adapted rather than applied verbatim.

The unpatched source plus that regression test reproduces the reader/schema
crash (exit -11). The repaired test passes. The upstream suites report 3,830,
49 and 1,471 tests with no unexpected errors; expected malformed-input cases
remain in the logs. The runtime receipt records the manifest, source hashes,
compiler/flags, negative result and installed binary hash.

Sources: [vendor code fix](https://github.com/alpinelinux/aports/blob/35f5eb2864e50dd4448758d31f3e2a32f733ab84/main/libxml2/CVE-2026-6732.patch),
[vendor regression](https://github.com/alpinelinux/aports/blob/35f5eb2864e50dd4448758d31f3e2a32f733ab84/main/libxml2/CVE-2026-6732-test.patch).

The source retains genuine SONAME `libxml2.so.2`; this is not a compatibility
symlink to the different ABI of libxml2 2.14+. ICU, compression and legacy FTP
are explicitly configured to preserve the original feature vector. An initial
configuration omitted FTP, and the native feature comparison rejected it.
The final build matches every original reported feature bit. PostgreSQL and
libxslt remain linked to the original SONAME and pass actual XML/XSLT checks.

The original library is accepted only at SHA-256
`c05750a6f1c9a90c254df313a9dda9b4c958c0a768b0faf4c15e04b3515c7d93`.
The new file has its correct versioned name, `libxml2.so.2.13.9`; the SONAME
link and loader cache are updated and the old 2.9.14 file removed. Native probes
report runtime version 21309. The installed binary SHA-256 is
`e87227602e0ced9dcee0292f5f3e77a8bf900c2169d4f3548964a8ad6f4492a3`.

Source: [upstream ABI change notes](https://raw.githubusercontent.com/GNOME/libxml2/v2.15.4/NEWS).

## Exact images and functional qualification

| Artifact | Image ID |
| --- | --- |
| Frozen deployed parent | `sha256:7cd98b66214c355a7b7724c26c17dabef053c7d77df995bd656fa2ff5e1cad0b` |
| Qualified maintenance overlay | `sha256:318e19500d021b506de8ca2473c4e7d09473d917a17e1d89ec2d174bfce026ed` |
| Ordinary source build | `sha256:7f26c32644d0c9a5aba12087d21539fc5f6e404f2d75c3a84c1f70fbfb4dbab5` |

Both builds preserve package versions, the complete image Config, all **1,150**
PostgreSQL file hashes, entrypoint/gosu and libxslt binaries. Their repaired
library bytes are identical. The maintenance image preserves the parent's
original layer prefix. The [regular Dockerfile](../../deploy/postgres.Dockerfile)
retains the source repair, and CI checks the loaded version, receipt/manifest
and binary hash before exercising XML; forgetting the runtime copy fails CI.

The [SQL checks](../../tests/containers/check_postgres_xml.sql) cover Unicode,
escaping, XPath and namespaces, XMLTABLE, fragments, NULLs, bounded entities,
malformed-input recovery and an actual xml2/XSLT transformation. Their extension
setup is rolled back. Same-system migration tests pass logical restore,
rollback including new writes, and unchanged ordering/classification for the
same corpus and glibc locale metadata. No physical data migration is required.

The ordinary build passes four existing accounts SQLite checks, 22 current
account/preference/workspace API tests, three browser suites, TLS/CSRF/session,
restart and twelve-table restore. The maintenance overlay separately passes
the preserved live-release fixture: 15 account/preference tests, both browser
suites, TLS/restarts, eleven-table/row restore and forty HTTPS probes without
retained gateway zombies. These fixtures do not deploy workspace functionality.

## Promotion and recovery verification

The qualified maintenance image was deployed as
`numforge-postgres:libxml-20261105` on **5 November at 21:05:29 CET
(20:05:29 UTC)** from source commit `9d5d4b0`. Only PostgreSQL was recreated
under the supervisor operation lock. The effective Compose change is limited
to its image; mounts, data volume, user/entrypoint, healthcheck and runtime
protections are preserved. Web, accounts, gateway and monitor retain their
exact container identities. The original image and private runtime profile
remain available for rollback.

Live native probes confirm version 21309, the qualified feature vector and
binary hash. A real PostgreSQL XPath query passes. Account schema and row
digests remain identical across all eleven tables. The monitor's existing
users, configuration and bounded heartbeat history are preserved, along with
its qualified SQLite engine. Local/public HTTPS and the existing tunnel work.
This replacement uses the existing PostgreSQL volume without initdb, schema,
locale or physical data migration. The newer workspace application remains
separate from this deployment.

The authorized encrypted backup workflow ran before and after replacement:
application/configuration/committed-history backups and separate consistent
monitor SQLite snapshots. All four restore points passed restore verification,
including eleven-table/row comparison, monitor byte digests and database
integrity, and repository checks. All four points were retained. The existing
routine retention and separate monitor-snapshot limitations still apply.

Normal supervisor Ensure and the final verification passed: **five healthy
services and six UP monitors**, repaired XML native hashes, unchanged account
data and other containers, runtime/scanner image identity, and absence of all
owned Compose and standalone XML fixtures. No push or reboot was performed.

## Scan interpretation and remaining work

Matched Trivy 0.76.1 reports retain three CRITICAL, 47 HIGH, 173 MEDIUM, 168 LOW
and six UNKNOWN records for parent, maintenance and ordinary builds. No record,
severity or installed package version changes; there are no listed fixes at
any severity. Report layer identities are checked. Debian attribution still
sees libxml2 2.9.14, so native/runtime receipts and upstream source verification
are essential evidence of the private replacement. No ignore/VEX or threshold
change was introduced. This is not a claim that all OS advisories are repaired.

This remains a private source build of the vendor-maintained legacy ABI branch,
not a migration to the newest upstream ABI. Future vendor/source updates must
be qualified again. Moving to 2.14+ requires rebuilding and testing its linked
consumers; bypassing the ABI boundary with a symlink is unsuitable. PostgreSQL's
separate system SQLite record and the monitor's archived Node binding remain
separate follow-up work.

Evidence is under ignored `build/security-postgres-xml-20261105/`: frozen live
identities/locale, rejected Alpine and accepted same-system fixtures, native
negative/positive build logs and receipts, file/feature inventories, ordinary
and preserved application checks, scan reports and metadata. Earlier failed
preimage/test assumptions are retained in their separate attempt logs.
The `deployment/` directory contains before-state, pre/post restore results,
live native verification, deployment/final-check records and the complete
console log. Restored private data remains in local operations storage and
is not committed.
