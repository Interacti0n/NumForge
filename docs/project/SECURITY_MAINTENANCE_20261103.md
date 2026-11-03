# Container dependency maintenance — 3 November 2026

The four deployed non-monitor maintenance images remove **19 records with listed
fixes: 11 MEDIUM, one LOW and seven UNKNOWN**. Each has zero remaining records
with a listed fix, at any severity, and no added fixable record. The monitor's
separately recorded disputed legacy minimatch finding remains. Unfixed findings
are retained; this is not a claim that the stack has no vulnerabilities.

## Scope and preserved application behavior

The original [review](SECURITY_REVIEW_20261103.md) used the 2 November database
and counted 18 other-image records. Matched scans here use Trivy **0.76.1** and
the database updated **3 November at 13:13:42 UTC**. That database adds the
OpenPGP module attribution in the old gateway, bringing this baseline to 19.
Every old/new report's DiffIDs match its exact image layers. No suppression,
ignore rule or severity-gate relaxation was introduced.

Ordinary builds from current HEAD also include the newer workspace feature and
its database migration. Those builds are tested separately. The maintenance
web/accounts candidates instead derive from frozen live images: the web and
calculator binaries and every `/service` file retain their original hashes.
The Python runtime and all installed application dependency versions stay the
same; only pip is removed. PostgreSQL remains **18.6**, Caddy remains **2.11.7**
and the gosu launcher source remains commit
`6456aaa0f3c854d199d0f037f068eb97515b7513`. Maintenance does not require the
workspace schema migration.

The preserved-image recipes and base identities are in the qualification
evidence. Web temporarily selects root, runs the Bookworm package upgrade and
returns to `10001:10001`. Accounts does the same, verifies existing Python
requirements, uninstalls pip and removes its standard-library ensurepip
bootstrap directory, then returns to the same non-root user. Removal concerns
the merged runtime filesystem; historical parent image layers are retained.
Future dependency changes require a rebuilt image rather than pip in a running
container. Canonical Dockerfiles apply these maintenance policies to new builds.

## Qualified dependency changes

| Image | Change | Removed fixable records |
| --- | --- | ---: |
| Web | libsasl2 packages `2.1.28+dfsg-10+deb12u1`, tzdata `2026f-0+deb12u1` | 2 MEDIUM, 1 UNKNOWN |
| Accounts | tzdata update, runtime pip and ensurepip removal | 5 MEDIUM, 1 LOW, 1 UNKNOWN |
| PostgreSQL | libsasl2/tzdata updates; gosu rebuilt with moby/sys/user `0.4.1` and x/sys `0.44.0` | 3 MEDIUM, 2 UNKNOWN |
| Gateway | chi `5.4.0`, x/net `0.62.0`, x/crypto `0.58.1-0.20261009181718-2f1f834330b2` | 1 MEDIUM, 3 UNKNOWN |

Gosu and Caddy use the existing Go 1.27.3 build environment. Their runtime
images contain the binaries and module build-information receipts; compiler
tools stay in separate stages. Go module checksums and actual installed module
versions are retained in inventory/build logs. The gosu UID/GID probe succeeds
with only SETUID/SETGID capabilities in the disposable inspection container;
dropping those capabilities first correctly prevents changing identity.

The OpenPGP upstream change splits an unmaintained package into a separate
module. Its removal from the selected module's inventory is not a repair of
OpenPGP itself, nor evidence that the old gateway invoked it. The other
advisories likewise retain their feature/platform qualifications in the original
review. The SASL plugin and Windows-only x/sys findings were not demonstrated
as reachable attacks on this Linux deployment.

An initial pip 26.2.2 replacement was rejected: its vendored packages added five
HIGH, two MEDIUM and one LOW fixable records. Keeping only the previous pip
advisory count would have missed that regression. The final preserved candidate
removes the unused installer; canonical builds use pip 26.3 during installation,
run `pip check`, then remove pip/ensurepip before selecting the runtime user.
The final all-severity matched scans establish the clean fixable inventory.

## Verification and exact artifacts

The final current-source build passes **22 account tests**, actual TLS/CSRF/
cookie/login/logout and spoofed-forwarding checks, DB restart/session recovery,
backup restore of all twelve fixture tables/rows and DB-independent calculation.
Its new workspace schema is confined to the disposable fixture. The earlier
current-source candidate additionally passed all three account/preferences/
workspace browser suites; those earlier browser results are not attributed to
the final changed dependency artifacts.

The preserved application variant uses the deployed release's test sources
from `bdf5a39`, with only fixture paths adjusted, plus the current process-reaping
check and installer-absence assertion. It covers **15 account/preference tests**,
all eleven restored tables/rows, persistence/restarts, real TLS/CSRF/cookies/
session revocation, forged forwarding headers, and both account/preferences
browser suites. The browser checks include SK/EN, mobile layout, separate clients,
cross-tab logout, stale revisions, offline retry and delayed-response isolation.
Forty actual gateway HTTPS probes leave no zombie processes. Test projects use
their own credentials and data volumes and are verified absent after cleanup.
Host Compose/backup/pilot checks also pass (3 + 4 + 8 tests), along with the
mixed-stack helper check.

| Candidate | Exact image ID (`sha256:`) |
| --- | --- |
| Preserved web | `2d6d5f7141a099efaf8668b118749a774fbd8a082cb61e1f03dfba95c0e58207` |
| Preserved accounts | `a9fdf6b585f8731a4951ec144b275561073b899429bb4a8278bc1785b123fe55` |
| PostgreSQL | `e972ceb4e5232dbbfd7207e3d264acee77840cd404f374a8d40acb439398604d` |
| Gateway | `5f1fa8a8c8bc79c55858633dd18eea887fe5e4754538a61b34b1b19b4f0acddc` |

| Candidate scan | CRITICAL | HIGH | MEDIUM | LOW | UNKNOWN | Fixable |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Web | 1 | 43 | 130 | 150 | 0 | 0 |
| Accounts | 2 | 36 | 117 | 105 | 7 | 0 |
| PostgreSQL | 3 | 46 | 173 | 168 | 7 | 0 |
| Gateway | 0 | 1 | 2 | 0 | 0 | 0 |

CI now scans web and accounts as well as gateway, PostgreSQL and monitor.
Its failure threshold remains fixable HIGH/CRITICAL; reports retain every
severity and unfixed record. Raw evidence is under ignored
`build/security-stack-20261103/`: frozen identities, original/rejected/final
reports, exact inventories, recipes, build logs, matching test snapshots,
final fixture results and qualified source hashes. Initial fixture setup put
gateway enforcement before direct Django client tests; that attempt failed and
was retained separately. The corrected sequence tests the direct API before
enabling the HTTPS gateway. No production database is used in these fixtures.

## Promotion

On **3 November 2026 at 15:38:48 UTC**, the exact four maintenance images in
the table above were deployed as `numforge-web:maintenance-20261103`,
`numforge-accounts:maintenance-20261103`,
`numforge-postgres:maintenance-20261103` and
`numforge-gateway:maintenance-20261103`, from maintenance source commit
`efcb67d`. Preserved application artifacts still come from the frozen deployed
images, rather than the newer workspace source at HEAD. There was no rebuild
between qualification and promotion.

Under the supervisor operation lock, effective Compose service definitions
changed only image selections for web/accounts/db/gateway and the matching
accounts setup image. No workspace migration was run. PostgreSQL remained 18.6;
all eleven account tables and their complete rows matched the pre-change
read-only aggregate hashes. The application role correctly cannot read the
migration ledger; the complete comparison used the existing local administrative
connection in a read-only transaction, without granting additional permissions.
Web/calculator binaries and all account source files matched their original
hashes. The existing monitor container/image, SQLite 3.54.1 engine, stored
settings/history, gateway CA, named volumes and public tunnel identity/URL were
preserved. Non-root/read-only/capability bounds and gateway init/PID bounds
were retained. Local pinned-TLS and public calculation/session checks passed.

The first rollout automatically reverted after a gateway mount assertion.
Original images and the exact saved runtime profile were verified restored and
healthy before retry. The successful attempt validates every mount descriptor
by destination; all final before/after descriptors matched, including order.
The first mismatch's descriptors were not retained, so its precise cause is
unconfirmed. Its log and rollback evidence remain available. The successful
attempt and later supervisor check independently verify the final mounts and
runtime protections.

Before and after the successful replacement, standard account/configuration/
committed-Git backups and separate consistent monitor SQLite snapshots went
to the existing external encrypted repository. Both pairs passed byte-for-byte
restore, all eleven account table/row comparisons, SQLite integrity/stored-state
verification and repository checks. All four restore points were retained at
completion. Standard daily retention can later rotate standard points; the
monitor's SQLite snapshots remain a separate operation.

A normal supervisor Ensure then preserved the selected images, original account
data/schema, monitor and URL. All **five services were healthy and six monitors
up**. Forty actual HTTPS probes and subsequent process inspection found no
retained gateway zombies. Both fixture projects were verified absent, and all
four running images matched their scanned artifacts. No push or reboot was
performed. Deployment evidence is under
`build/security-stack-20261103/deployment/`: before/rollback records,
`mount-validation.json`, pre/post backup records, `deployment.json` and
`final-check.json`. Sensitive database copies remain in the protected operations
directory.

## Follow-up decisions

### Base migration assessment after maintenance

A subsequent isolated, network-disabled inspection of the exact maintenance
images confirms that PostgreSQL directly links Bookworm's `libxml2.so.2` and
was configured with XML/XSLT support. Removing that library would break the
server; an XML-capable replacement and XML regression checks are required.
The accounts Python standard-library SQLite module loads system SQLite 3.40.1.
This is separate from the monitor's rebuilt Node engine. The inspected web and
calculator binaries link libc/libm, without a direct SQLite/XML/zlib dependency;
that observation does not cover every helper process or dynamically loaded code.

The monitor retains a separate system Python SQLite module and sqlite3 CLI.
Its installed reverse-dependency metadata connects python3-jwt to python3-oauthlib
and system SQLite to Python/its CLI. The Node executable's linkage does not
establish whether optional monitor helpers invoke these packages. Removing
Python or JWT requires an optional-feature/consumer review, rather than treating
the fixed Node database engine as coverage of the system packages.

The current Debian tracker marks SQLite fixed in Trixie, while PyJWT
CVE-2026-102267 and libxml2 CVE-2026-6653 remain vulnerable there. A blanket
Bookworm-to-Trixie switch therefore does not meet the complete remediation goal.
The next qualification should first target the accounts system SQLite through
a same-Python-major base candidate, then separately address PostgreSQL's XML
library and monitor helper dependencies. Keep the existing native binding until
a replacement passes both Knex consumers, callbacks, WAL, feature/limit checks,
persisted upgrade/rollback and restore; changing only the package name is not
a qualified replacement.

At this inspection Docker's engine was stopped and the supervisor reported a
failed check. Starting it through `docker desktop start --detach` and running
the existing Ensure recovered local/public HTTPS. Subsequent inspection confirmed
five healthy services, six up monitors and the same maintenance image identities.
The cause of the stopped engine was not established. No base image or database
binding was replaced during this assessment. Evidence: ignored
`build/security-base-review-20261103/runtime-linkage.json` and `recovery.json`.

Sources: [SQLite release status](https://security-tracker.debian.org/tracker/CVE-2025-7458),
[PyJWT release status](https://security-tracker.debian.org/tracker/CVE-2026-102267),
[libxml2 release status](https://security-tracker.debian.org/tracker/CVE-2026-6653).

1. Plan a supported base-image migration for the remaining unfixed OS findings,
   especially SQLite/PyJWT/libxml2. Preserve vendor applicability notes, verify
   real linked libraries and qualify data restore/rollback before replacement.
2. Plan replacement of the archived Node SQLite binding. The deployed 3.54.1
   source rebuild covers the engine; it does not restore upstream maintenance
   of the wrapper.
3. Keep the disputed legacy minimatch record visible. Prepare the bounded
   reproduction and consumer inventory for an upstream range clarification or
   an explicitly qualified replacement; do not replace its callable API solely
   to silence the scanner.
4. Decide separately whether to deploy the newer workspace functionality and
   schema, with its own feature acceptance and migration/rollback plan.

Primary references: [Debian SASL fix](https://security-tracker.debian.org/tracker/CVE-2026-107161),
[moby parser advisory](https://github.com/moby/sys/security/advisories/GHSA-mjcv-p78q-w5fw),
[chi forwarding advisory](https://github.com/go-chi/chi/security/advisories/GHSA-j6qf-3m9c-vwj3),
[Go HTML parser report](https://pkg.go.dev/vuln/GO-2026-6860),
[OpenPGP status](https://pkg.go.dev/vuln/GO-2026-5932),
[module split source](https://github.com/golang/crypto/commit/2f1f834330b2),
[pip release notes](https://pip.pypa.io/en/stable/news/),
[pip uninstall](https://pip.pypa.io/en/stable/cli/pip_uninstall/).
