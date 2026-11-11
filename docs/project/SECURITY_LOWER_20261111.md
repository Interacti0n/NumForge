# Lower-severity update qualification on 11 November 2026

## Four available fixes outside the old HIGH/CRITICAL gate

The pre-update deployed-image baseline contained four records with listed fixes:

| Component | Advisory | Scanner severity | Qualified update |
| --- | --- | --- | --- |
| PostgreSQL gosu Go runtime | CVE-2026-97054 | UNKNOWN | Go 1.27.3 → 1.27.4 |
| Gateway Go runtime | CVE-2026-97054 | UNKNOWN | Go 1.27.3 → 1.27.4 |
| Gateway Alpine zlib | CVE-2026-4738 | MEDIUM | 1.3.2-r1 → 1.4-r0 |
| Monitor fast-xml-parser | CVE-2026-126066 | LOW | 5.8.0 → 5.12.0 |

An initial summary conflated a clean HIGH/CRITICAL gate with no listed fixes at
any severity. The final all-severity check rejected that conclusion before a
security promotion. The inventory was corrected; no records were hidden.
`scan_images.py --fail-on-any-fix` now includes LOW, MEDIUM and UNKNOWN and is
used in container CI. The old `--fail-on-fixable` remains explicitly documented
as a legacy HIGH/CRITICAL-only gate. Its behavior is covered by a regression test.

Sources: [Go 1.27.4 release](https://go.dev/doc/devel/release#go1.27.minor),
[Alpine vendor status](https://security.alpinelinux.org/vuln/CVE-2026-4738),
[XML parser advisory](https://github.com/NaturalIntelligence/fast-xml-parser/security/advisories/GHSA-474x-hrwr-fgx4).
Package presence does not by itself prove an exploitable route in the pilot.

## Go launchers and Alpine packages

The ordinary PostgreSQL and gateway recipes rebuild their existing upstream
gosu/Caddy versions and identical Go module versions with Go 1.27.4. PostgreSQL's
server files, entrypoint, Debian package versions, XML library and probe remain
identical. The Go launcher still switches to UID 65534 in a bounded fixture with
the required SETUID/SETGID capabilities.

The gateway remains Caddy 2.11.7 on Alpine 3.23. Vendor package refresh updates
zlib, alpine-release 3.23.7→3.23.8 and apk-tools/libapk 3.0.8→3.0.9; other package
versions and image defaults remain identical. The refreshed APK command is part
of the recipe so the prior cached package-upgrade layer cannot mask the update.

The isolated database comparison passes XML/XSLT, logical dump/restore, writes
carried back to the original image and identical libc locale metadata, Unicode
ordering and case classification. A preserved production-contract stack passes
15 account API checks, two browser suites, eleven-table/all-row restore, TLS,
CSRF/cookie boundaries, database-independent calculation and the first session
request after a database restart. Its gateway completes 40 HTTPS probes with no
retained zombie processes. It uses fresh fixture credentials/volumes, never live data.

## Unmodified XML parser with a qualified GameDig override

The [XML manifest](../../deploy/monitor-xml.json) pins upstream fast-xml-parser
5.12.0 and a private complete dependency closure by official npm URLs and SHA-512
integrity. Download size, archive expansion and file counts are bounded;
traversal, absolute paths and links are rejected. Four host tests cover these
boundaries. The installer normalizes owner-only author archive modes to ordinary
runtime-readable package-manager modes, preserving vendor source bytes and licenses.

GameDig 5.3.3 declares an exact parser 5.8.0 dependency. A scoped npm override is
therefore qualified explicitly for that exact consumer/version/integrity, moving
within parser major 5. GameDig's JavaScript and package metadata stay unchanged;
the application's override and genuine package-lock/installed-lock entries record
the selected replacement. Unrelated consumer ranges and all other lock entries
must remain compatible and unchanged.

New dependency majors are contained under the parser's private node_modules;
existing shared packages used by other consumers remain intact. The receipt
`/opt/numforge/monitor-xml.json` records the full manifest, actual consumers and
every installed parser/dependency file hash and mode. No npm install scripts or
vendor JavaScript modifications are introduced.

Four new Node tests cover receipt/lock integrity as the normal runtime UID,
Unicode/attributes/entities/string round trips and validation, the actual published
GameDig Farming Simulator protocol, and malformed non-default stop-node handling.
Inspection confirms GameDig's bundled CommonJS entry also requires the external
parser. In a separate negative control, the old parser exceeds a 1.5-second child
process deadline for 100,000 unterminated tag fragments; 5.12.0 rejects that same
input within the budget. This is bounded regression evidence, not a throughput
or universal complexity benchmark.

The monitor passes old → candidate → old → candidate: 238 existing checks on
each old selection and 242 on each new selection, including preserved users,
monitor configuration/history, authentication, up/down/push and restart behavior.
The fresh ordinary build passes 242 Node checks, four system SQLite checks,
seven Python/Apprise helper checks and six PyJWT security checks.

The application tree outside the declared XML module, scoped override, CLI link
and two lockfiles is identical (29,509 files). Native SQLite, maintained binding,
PyJWT 2.15.1, previous system repairs, Debian package versions and image runtime
defaults are unchanged. Candidate and ordinary XML artifact trees compare equal.

## Evidence and deployment boundary

Evidence under `build/closeout-20261111/` includes `lower-inventory.json`,
`lower-audit.log`, `lower-reports/`, `lower-scan.log`, `preserved-result.json`,
`postgres-qualification/xml-result.json`, and `xml/` fresh/upgrade, negative-control
and package-source records. All fixture resources were removed.

All four candidate/ordinary image scans use the same current vulnerability
database as the baseline and have zero records with listed fixes at **any**
severity. Remaining unfixable/version-based system records stay visible. Promotion
requires exact image/layer/source hashes, verified encrypted pre/post backups,
unchanged existing volumes/CA and an automatic return to the old profile if live
acceptance fails. No account/workspace schema migration is part of this update.

## Promoted release

The three ordinary images qualified above were promoted from committed source
`907912a` on 11 November. Web and accounts containers stayed unchanged; the live
database still has eleven tables. Release image IDs are:

| Service / local release tag | Immutable Docker image ID |
| --- | --- |
| PostgreSQL / `numforge-postgres:go-20261111` | `sha256:6cc405fd36e6256df38fc33c37cdce9875df02152ca3ed0d28e34a0c574dedc0` |
| Gateway / `numforge-gateway:lower-20261111` | `sha256:e736a2431078bbd783618a52bdf0109fbbdffa335a7e9b671d2bb27b9e35c06c` |
| Monitor / `numforge-uptime:xml-20261111` | `sha256:aae3f1732eeb900dabc58e73fb6497052b319eedbdd7fc0ed3ae37b01c996d16` |

Live acceptance confirms five healthy services, six UP monitors with fresh
non-push heartbeats, unchanged account rows, monitor users/configuration/history,
native SQLite identity, supported PyJWT, PostgreSQL XML probe and gateway CA.
All four XML tests also pass inside the deployed monitor. HTTPS/public calculation
and forty gateway probes pass. Runtime users, restrictions, ports, restart policy
and original named volumes remain unchanged.

Two earlier acceptance attempts returned all three services to their original
images automatically: a missing optional Docker `Init` field and two equivalent
Windows/Linux representations of the same bind source triggered conservative
checks. The harness now compares missing optional fields consistently and
canonicalizes only Docker Desktop's known host-drive representation. The actual
before/after mounts and equal canonical paths are retained as evidence. No live
data restore was needed; neither failure was waived as an application failure.

`lower-deployment/deployment.json` records successful acceptance and verified
encrypted pre/post standard and supplemental monitor backups. All four restore
points were retained when deployment completed. `post-deployment-security/`
contains layer-verified reports for the exact five deployed images. Compared
with the same-day baseline, exactly the four listed records above disappeared,
with no added advisory keys. Remaining records: **1,811**, comprising 11 CRITICAL,
444 HIGH, 684 MEDIUM, 627 LOW and 45 UNKNOWN; none has a listed fix at any severity.
These remaining records require the applicability review in the triage document;
zero listed fixes is not a claim of zero vulnerabilities.

For the latest recovery capture use the private `recovery-kit-status.json` and
the kit's own manifest, never historical image tags. The final release rehearsal
and closure checks are recorded under `kit-recovery-final/` and `final-check.json`
in the same ignored evidence directory. A separate clean host and Windows reboot
remain distinct acceptance gates.
