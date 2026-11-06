# Remaining container findings on 5 November 2026

## Scope and evidence

The local inventory covers **all severities** in the saved Trivy 0.76.1 reports
for the exact deployed web, accounts, PostgreSQL, gateway and monitor images.
The vulnerability database was updated at `2026-11-05T13:09:41Z`. These are existing
same-day scans, not a claim of a second fresh scan. Their layer lists were compared
again to Docker's immutable images before inventorying.

`scripts/security_inventory.py --verify-images` writes the complete rows to
`build/followup-20261105/security/findings.csv` and per-image summaries to
`summary.json`. There are **1,493 package/advisory records**, comprising 633 unique
package/advisory pairs across images. Counts are records, not distinct exploitable
bugs or an estimate of attack probability. Trivy's config-derived archive ID can
differ from Docker's OCI manifest ID; both are recorded separately and layer
matching, not equality of those two identifiers, establishes report applicability.

| Service | CRITICAL | HIGH | MEDIUM | LOW | UNKNOWN | Records with a listed fix |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Web | 1 | 44 | 130 | 149 | 0 | 0 |
| Accounts | 2 | 37 | 117 | 105 | 6 | 0 |
| PostgreSQL | 3 | 47 | 173 | 168 | 6 | 0 |
| Gateway | 0 | 1 | 2 | 0 | 0 | 0 |
| Monitor | 5 | 77 | 210 | 193 | 17 | 0 |
| Total | 11 | 206 | 632 | 615 | 29 | 0 |

Zero listed fixes does not mean zero vulnerabilities. No ignores, VEX exemptions,
severity changes or deletion of package metadata were introduced. Vendor labels
`affected`, `fix_deferred` and `will_not_fix` are retained as data, not accepted
automatically as project risk decisions.

## Account for every CRITICAL record

| Exact advisory / package scope | Records | Assessment |
| --- | ---: | --- |
| CVE-2023-45853 / zlib1g in four Debian images | 4 | Vendor package-applicability distinction; see below. Not a private repair. |
| CVE-2025-7458 / accounts libsqlite3-0; monitor libsqlite3-0 and sqlite3 | 3 | Targeted private backport verified in deployed image lineage; package-version records remain. |
| CVE-2025-7458 / PostgreSQL libsqlite3-0 | 1 | Open: no repair of that system SQLite library has been promoted. |
| CVE-2026-6653 / PostgreSQL libxml2 | 1 | Replaced native library and regression-qualified source repair; dpkg version record remains. |
| CVE-2026-102267 / monitor python3-jwt | 1 | Targeted redirect backport verified; installed version still yields a record. |
| CVE-2026-102268 / monitor python3-jwt | 1 | Open; distinct from the redirect issue. |

The [Debian CVE-2023-45853 tracker](https://security-tracker.debian.org/tracker/CVE-2023-45853)
explains that the vulnerable MiniZip contribution is not built by Bookworm's zlib
source into the affected binary packages. Separate MiniZip packages have their own
status. This narrows these four **zlib1g** records; it does not establish that every
vendored MiniZip copy anywhere in an application is absent or safe, and it does
not dismiss other zlib advisories.

Private repairs are supported by source preimages, installed-file hashes/receipts
and old-versus-fixed regression controls in the
[SQLite backport](SECURITY_SQLITE_BACKPORT_20261104.md),
[monitor helper repair](SECURITY_SYSTEM_HELPERS_20261105.md) and
[PostgreSQL XML qualification](SECURITY_POSTGRES_XML_20261105.md) records.
They address only the named issues, not all records for those packages.

## Open items requiring the next focused qualification

The [6 November PyJWT promotion](SECURITY_MONITOR_JWT_20261106.md) follows up
the monitor item below with an unmodified supported release, now deployed. The tables here
remain the dated 5 November scanner baseline; package-version records must not
be interpreted as a fresh scan or a blanket exemption.

Fresh scans on 6 November used database `2026-11-06T12:55:02Z`, verified all five
exact image layer lists and retained 1,493 records: 11 CRITICAL, 219 HIGH,
648 MEDIUM and 615 LOW, with zero UNKNOWN or listed fixes. Severity metadata changed
without new package/advisory pairs or package updates. Full current inventory is
in `build/followup-20261106/security/`; the promoted monitor has the same OS rows
and an additional correctly detected supported PyJWT distribution with no findings.

### PyJWT consumers and asymmetric/HMAC confusion

The [upstream CVE-2026-102268 advisory](https://github.com/jpadilla/pyjwt/security/advisories/GHSA-ffc3-869f-jxw9)
requires a mixed symmetric/asymmetric algorithm allow-list and raw public-key PEM
formatting accepted by the cryptography loader but missed by the old detector.
[Debian still lists Bookworm's installed version as vulnerable](https://security-tracker.debian.org/tracker/CVE-2026-102268).
The previous redirect backport does not change this detector.

Next gate: enumerate actual decode call sites and key/algorithm configuration in
the installed Apprise/oauthlib dependencies, reproduce the old detector failure,
then compare a pinned supported upgrade or narrow upstream repair. Test RSA/EC
signing/verification, accepted/rejected key formats, the existing redirect fix and
real Node-to-Apprise delivery before promotion. Current static observations do
not prove this separate advisory unreachable. Other HIGH/MEDIUM PyJWT rows in
the full inventory remain open too; do not repeatedly fix only the top severity.

### Gateway Go JOSE

The gateway binary has one HIGH and two MEDIUM records in Go JOSE v3:

- [CVE-2026-116587](https://github.com/go-jose/go-jose/security/advisories/GHSA-mgcc-cww5-fp2q): excessive CPU while parsing crafted RSA JWKs.
- [CVE-2026-116585](https://github.com/go-jose/go-jose/security/advisories/GHSA-4w3g-jfj2-cw2q): panic in CBC-HMAC decryption.
- [CVE-2026-116586](https://github.com/go-jose/go-jose/security/advisories/GHSA-vvm8-gf78-28g4): crash while parsing deeply nested JSON.

The vendor advisories list no repaired v3 release; fixes exist on v4. This explains
why the v3 records have no listed fix; it does not close them. Determine which
installed Caddy/ACME modules call the affected operations and whether input is
untrusted. Qualify a compatible gateway release/dependency migration or a narrow
backport with TLS/CA, routing/auth boundary and zombie-process regressions.
No claim of public exploitability or unreachability is made by package presence alone.

### PostgreSQL's separate system SQLite

The server's repaired XML library and the monitor's repaired Node SQLite binding
do not repair PostgreSQL's installed system SQLite. The scoped linkage inspection
found no direct server dependency and no Python/SQLite CLI consumer in that image.
Treat that as bounded evidence while inspecting optional modules/helpers; decide
between removing an unused dependency, qualified replacement or documented
acceptance. Do not remove packages blindly from a running database image.

## Medium, low and unknown findings

All **1,276 MEDIUM/LOW/UNKNOWN** rows are preserved in the CSV. They have not each
received a source-level reachability audit. Start with shared libc/curl/util-linux
families, then TLS/Kerberos, system Python helpers and package-specific tools.
Several binary packages repeat one source defect, so group investigation while
retaining each image's installed version and consumers.

For each group record the advisory, triggering operation, actual entry point,
vendor rationale, installed-code evidence, action and regression gate. A
`will_not_fix` result remains open unless an applicability argument or explicit
risk decision exists. UNKNOWN requires classification, not automatic dismissal.
Refresh scans before deploying any candidate, serially when sharing the Trivy cache.

Whole-base migration is a candidate, not a default cure. The tested Alpine
PostgreSQL alternative passed ordinary tests but changed Unicode collation/case
semantics; the Trixie accounts alternative increased other HIGH records. Revisit
only with new evidence and the same compatibility gates. See the staged
[local plan](LOCAL_PLAN_20261105.md) for the next decisions.
