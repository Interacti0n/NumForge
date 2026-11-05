# Monitor system helper repairs on 5 November 2026

The monitor's Node SQLite engine and its Debian Python helpers are separate
consumers. The previously deployed Node engine is 3.54.1, but system Python and
the sqlite3 CLI still load Debian's 3.40.1 library. The dependency-only candidate
repairs CVE-2025-7458 in that system library and CVE-2026-102267 in PyJWT, while
preserving the complete `/app` tree, package versions and image configuration.

## Applicability and chosen scope

The exact deployed monitor contains Python PyJWT 2.6.0-1+deb12u1, oauthlib,
requests-oauthlib and Apprise 2.0.0. Kuma's Apprise provider starts the Python
notification executable; removing Python/JWT would affect supported helper
features. Inspected installed consumers use JWT signing and verification. The
inspection found no consumer of `PyJWKClient` outside PyJWT itself in the Python
distribution package tree. This is a bounded static observation, not proof that
the affected API can never be reached through any optional configuration.

The upstream JWT advisory concerns redirects from a configured JWKS endpoint:
redirected responses can become trusted verification keys. Bookworm and Trixie
both still lack the Debian fix. A blanket base migration therefore does not
resolve this issue. The candidate backports the upstream redirect rejection,
retaining the installed 2.6 constructor, error and cache behavior.

Sources: [upstream PyJWT advisory](https://github.com/jpadilla/pyjwt/security/advisories/GHSA-9v7f-9g4p-ffgj),
[Debian PyJWT status](https://security-tracker.debian.org/tracker/CVE-2026-102267),
[Debian SQLite status](https://security-tracker.debian.org/tracker/CVE-2025-7458).

## Source and binary controls

The [JWT backport helper](../../deploy/backport_monitor_jwt.py) requires source
SHA-256 `d0b7b197e853626e74551a3452e15962fb43966818fd5d3c67c7ad37121cf5dd`
for `jwt/jwks_client.py`. It adapts upstream commit
`0a795b8e1f6ef08f634aa7086fc41cc6d5ce3e56` to the older API by using a private
no-redirect urllib opener only for JWKS fetches. Other urllib consumers retain
their existing redirect behavior. Old bytecode for this module is removed.
The receipt `/opt/numforge/system-jwt-backport.json` records source hashes,
upstream identity and the adaptation. Unknown source is rejected before writes.

Source: [upstream fix](https://github.com/jpadilla/pyjwt/commit/0a795b8e1f6ef08f634aa7086fc41cc6d5ce3e56).

The system SQLite artifact is the already qualified
[accounts backport](SECURITY_SQLITE_BACKPORT_20261104.md). The original monitor
library has the identical preimage SHA-256
`2e6eef9a727f081f0d453b4e5e6cbd8b9ef8b6f86cbf7681cbad444d3b0b55c8`.
The replacement is pinned to
`f4b32d83fadc4787cabff8a115e4aca3ac808efe9d9521b9630f1d8815f129b5`,
with the original source receipt copied alongside it. This repairs the bounded
DISTINCT/ORDER BY optimizer regression; it does not claim to resolve every
SQLite advisory against 3.40.1.

The [maintenance recipe](../../deploy/monitor-system.Dockerfile) uses the frozen
APT-maintained monitor and accounts artifacts, rejects an unexpected system
library, and restores the parent's `node` image user. All build steps can run
without a network. The [regular monitor build](../../deploy/monitor.Dockerfile)
also builds this Debian backport from source and applies the same JWT helper,
so later ordinary builds retain both repairs.

## Functional qualification

The frozen maintenance candidate is
`sha256:6ab6140062a4b5d565311fefe5dfcb66e2d987b564159dca45008c013eb291c6`.
Its parent's complete image Config and layer prefix are preserved. An inventory
of all **29,548** application files, modes and symlinks has the identical digest;
installed package versions and the existing Node binary/receipts are unchanged.

The [system helper suite](../../tests/containers/check_monitor_system.py) uses
only local disposable HTTP endpoints and verifies:

- 301/302/303/307/308 redirects are rejected, the destination receives no request
  or credentials, and failed fetches cannot populate the JWKS cache;
- normal RSA key fetching, JWT signature validation, signing-key caching and
  refresh for an unknown key still work;
- HTTP and malformed-response errors clear cached key material;
- redirect behavior of unrelated urllib callers remains unchanged;
- oauthlib signing/verification and rejection of an invalid signature work;
- the actual Kuma Node provider starts Apprise and delivers the expected title
  and body to a local JSON notification endpoint;
- loaded source and system library match their receipts and pinned artifacts,
  and the actual sqlite3 CLI also passes the optimizer regression.

All seven tests and the four existing system SQLite regressions pass in a
network-disabled, read-only, capability-dropped container with bounded memory,
process count and temporary storage. Negative controls against the original
monitor fail on the actual optimizer assertion and all five redirect statuses.
The source helper's host test also confirms rejection of an unknown preimage
without changing it or writing a success receipt.

The disposable persisted-data fixture passes all 230 existing Node checks on
each image selection, authenticated provisioning, up/down and push detection,
restart, candidate upgrade, rollback and reapplication. User/configuration and
bounded historical heartbeat digests are preserved. New system tests run on
the candidate, while the explicitly selected rollback parent remains a negative
control. All fixture containers, networks and volumes are removed.

The regular source build,
`sha256:5531bbacca727f5a1155e194cdf9c99d859b4317f0257d8eb17b631c60047d5e`,
also passes a separate clean-volume fixture: 230 Node checks, all eleven native
and helper tests, authenticated provisioning, up/down/push detection, restart
and stored-state checks. Its owned containers, network and volume are removed.

## Matched vulnerability reports

Trivy 0.76.1 uses its database updated **5 November at 13:09:41 UTC**.
The matched parent/candidate reports each contain five CRITICAL, 77 HIGH,
210 MEDIUM, 193 LOW and 17 UNKNOWN records, with no listed fixes at any severity.
Type/package/path/advisory keys, installed versions and severities match exactly;
report DiffIDs match the respective image layers. No advisory is added, removed
or suppressed. Debian version-based attribution retains both privately
backported records, so the regression tests and source/binary receipts are
necessary evidence of these two specific repairs.
The separately rebuilt regular image has the same advisory/version/severity
inventory and verified layer identities, also with no listed fixes.

## Remaining migration work

PostgreSQL directly links its installed libxml2 and has XML/XSLT support.
Bookworm and Trixie remain vulnerable to CVE-2026-6653. The cited upstream fix
reworks entity accounting across seven files and changes public parser/entity
structures; it is not a small isolated source clamp comparable to the SQLite
repair. Removing the library or copying an unqualified replacement is unsuitable.
A future candidate must preserve PostgreSQL XML functionality and pass parser,
data restore and rollback checks before promotion. No libxml2 fix is claimed here.

The PostgreSQL image also inventories the original system SQLite library, but
the inspected server executable does not directly link it and that image has
neither Python nor a sqlite3 CLI. This limits the identified consumer surface;
it does not justify suppressing a version-based record or asserting that no
other library or helper could load it.

Sources: [Debian libxml2 status](https://security-tracker.debian.org/tracker/CVE-2026-6653),
[upstream entity-accounting change](https://github.com/GNOME/libxml2/commit/463bbeeca1805b5c4828f50d0fefc4eebaf620df).

The archived Node SQLite binding remains a separate maintenance concern. Its
engine repair is preserved. Replacing the wrapper requires qualification of both
Knex consumers, callback/error behavior, WAL and persisted upgrade/rollback.
The newer workspace application and schema are also outside this helper repair.

Evidence is under ignored `build/security-system-libraries-20261105/`: frozen
image identities, consumer inspection, tree and package inventories, positive
and negative regression logs, monitor fixture results, regular build logs and
matched vulnerability reports. Version-based records for privately backported
libraries must remain visible; no scanner suppression is introduced.
