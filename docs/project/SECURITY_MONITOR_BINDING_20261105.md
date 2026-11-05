# Monitor SQLite binding replacement on 5 November 2026

The qualified replacement uses Microsoft's `@vscode/sqlite3` 5.1.14-vscode
instead of archived `@louislam/sqlite3` 15.1.6. The existing import path is
retained through an npm alias; the installed package retains its real Microsoft
name, version, registry URL and integrity. The SQLite engine remains the
previously qualified **3.54.1**, with identical source ID, compile options,
features and resource limits. The Microsoft archive's bundled 3.39.4 is removed
before compilation and never used at runtime.

## Maintenance and source identities

On inspection both Louislam's fork and TryGhost's upstream repository are
archived. Microsoft's repository is not archived, has activity on 30 October,
and provides a Microsoft security reporting policy. The latest published
package is 5.1.14-vscode, published 22 June. This is a point-in-time maintenance
assessment, not a promise of future support.

Sources: [original fork](https://github.com/louislam/node-sqlite3),
[archived upstream](https://github.com/TryGhost/node-sqlite3),
[Microsoft fork](https://github.com/microsoft/vscode-node-sqlite3),
[Microsoft security policy](https://github.com/microsoft/vscode-node-sqlite3/blob/release/vscode/SECURITY.md),
[published package metadata](https://registry.npmjs.org/@vscode%2fsqlite3/5.1.14-vscode).

The [manifest](../../deploy/monitor-binding.json) pins the published binding
archive and nested node-addon-api 8.9.2 headers by registry SHA-512 integrity.
The existing root node-addon-api 4.3.0 and other consumers remain intact.
[Source preparation](../../deploy/prepare_monitor_binding.py) verifies downloads
before extraction, rejects links/path traversal and bounds source archive
size, member count and expanded bytes while streaming. Five independent host
tests cover integrity, safe extraction, traversal, links and oversized headers.

The existing official SQLite archive and amalgamation SHA3 pins are reused.
Only its dependency version selector/archive changes inside the Microsoft
package; its C++ wrapper, JS API and native loader are unmodified. No package
install hooks or prebuilt native downloads run. Compilation uses local headers
for the pinned Linux x64 Node 22.23.4 runtime with networking disabled. Build
objects are removed; the required `build/Release/vscode-sqlite3.node` remains.

The [installation helper](../../deploy/install_monitor_binding.cjs) refuses an
unexpected original wrapper or Node platform/version, then updates the root
manifest and both lockfiles to the genuine alias. A separate nested header
entry records its actual source integrity. The new native receipt records both
replacement identities, wrapper/gyp hashes, SQLite provenance, binary digest,
compiler and runtime linkage. The preceding monitor patch manifest remains the
input record for the earlier patch stage; the replacement receipt supersedes
its old SQLite consumer identity.

## Identified error API difference

The first persisted upgrade fixture caught two failed assertions: Microsoft's
driver enables extended SQLite result codes. Unique and foreign-key violations
report `SQLITE_CONSTRAINT_UNIQUE`/2067 and `SQLITE_CONSTRAINT_FOREIGNKEY`/787,
rather than the old generic `SQLITE_CONSTRAINT`/19. Primary-key violations
report `SQLITE_CONSTRAINT_PRIMARYKEY`/1555.

Installed Kuma server, root Knex 3 and RedBean/Knex 2 sources were inspected.
No SQLite constraint-code equality consumer was found in that scope. Kuma's
busy/locked handling checks message prefixes, which remain present. Tests now
require each fork's exact code and errno, and exercise real constraint failures,
rollback and continued use through **both** Knex dialects. Error normalization
or a private wrapper source patch was unnecessary. This diagnostic difference
is preserved explicitly rather than described as byte-identical API behavior.

## Functional and artifact qualification

Eight additional native callback checks cover prepared bind/reset/finalize,
serialized order and chaining, `each` completion, the real backup API,
read-only restore, lock timeouts/recovery, error delivery, parameter isolation
and interruption/reuse. They pass on both the original and new bindings.

The complete monitor suite passes **238 checks** at each selection in the
persisted original → replacement → original → replacement exercise. Original
users, configuration and bounded heartbeat history are preserved. Authenticated
provisioning, keyword/push up/down behavior, restart, rollback and reapplication
pass. The ordinary source build also passes a clean-volume fixture with all
238 Node checks, four system SQLite checks and seven JWT/notification checks.

CI requires the replacement receipt to match the committed manifest, even if
an old binding still passes the common API tests. A negative control confirms
that this provenance check rejects the archived binding. Rollback fixtures
explicitly retain support for checking the selected original image.

| Artifact | Image ID |
| --- | --- |
| Frozen deployed parent | `sha256:6ab6140062a4b5d565311fefe5dfcb66e2d987b564159dca45008c013eb291c6` |
| Qualified maintenance replacement | `sha256:b7c69356941c83d47fa18562be5e4eb38bd42cb5a0b835af66cbbf727bec720a` |
| Ordinary source build | `sha256:c8f6010e7f7c49b805ee5d83a64ad465b4c7778571e4f712a8675073749c31e8` |

The originally tested, release and ordinary-build binding trees are identical
by full file/mode digest. The new native SHA-256 is
`85dc709825dd3c75ef69160204cd570474c77eb078ba7783071423891a3dd291`.
All **29,521** other application files, modes and symlinks have identical hashes;
the declared differences are the binding subtree and three manifest/lockfiles.
OS package versions, system SQLite/JWT repairs, previous package-patch receipt
and image Config are preserved. Complete SQLite compile-option arrays match.

The [maintenance overlay](../../deploy/monitor-binding.Dockerfile) starts from
the frozen system-helper image. The [regular monitor build](../../deploy/monitor.Dockerfile)
retains the replacement and independent system repairs for future CI/builds.
The runtime retains its non-root user and existing healthcheck/protections.

## Promotion and recovery verification

The replacement was deployed as `numforge-uptime:binding-20261105` on
**5 November at 22:11:19 CET (21:11:19 UTC)** from source commit `d6440c6`.
Only the monitor was recreated under the supervisor operation lock. Its user,
command, healthcheck, mounts, port bindings and runtime protections are
preserved. Web, accounts, PostgreSQL and gateway retain their exact container
identities. The original monitor image and private runtime profile remain
available for rollback.

Live verification confirms the Microsoft package identity, qualified native
binary hash and unchanged SQLite 3.54.1 source ID. Original monitor users,
configuration and bounded heartbeat history are preserved. Account schema and
row digests remain identical across all eleven tables. System SQLite/JWT
repairs and PostgreSQL's XML repair remain present. Local/public HTTPS and
the existing tunnel work; the workspace application was not deployed.

The authorized encrypted backup workflow ran before and after replacement,
covering application/configuration/committed-history backups and separate
consistent monitor SQLite snapshots. All four restore points passed restore
verification, including eleven-table/row comparison, monitor byte digests and
database integrity, and repository checks. All four points were retained.
Existing routine retention and separate monitor-snapshot limitations apply.

Normal supervisor Ensure and the final check passed: **five healthy services
and six UP monitors**, the maintained binding and original engine identity,
preserved system repairs and other containers, matching runtime/scanner image
layers, and absence of both disposable fixture projects and their resources.
The initial final-check launch was deferred by an automatic approval-review
usage limit; the resumed launch passed through the same review and completed.
No push or reboot was performed.

## Scan scope and remaining work

Matched Trivy reports retain five CRITICAL, 77 HIGH, 210 MEDIUM, 193 LOW and
17 UNKNOWN records. Advisory/package/path keys, installed vulnerable versions
and severities are unchanged, with no listed fixes. Image layer identities
are checked and no suppression is introduced. This replacement addresses
binding maintenance, not every outstanding OS advisory or scanner count.
The Node inventory grows from 647 to 648 packages because of the separate
header dependency. Trivy identifies the aliased package as `@vscode/sqlite3`
5.1.14-vscode and reports no Node package vulnerabilities.
The first parallel ordinary-build scan timed out waiting for the shared cache;
its log is retained and the scan was rerun on its own.

Future Microsoft releases still require source, engine, callback and persisted
rollback qualification. The separate private system-library repairs and their
version-based scanner records remain applicable. The workspace application
and account schema are separate deployment work.

Evidence is under ignored `build/security-monitor-binding-20261105/`: repository
maintenance metadata, registry/source identities, build and callback logs,
failed and successful upgrade attempts, fresh fixture, source guards, native/
file/package inventories and matched scans. Fixtures use owned disposable
volumes; real credentials and database rows are not logged or committed.
The `deployment/` directory contains before-state, pre/post backup and restore
records, live native verification, deployment/final-check results and the
complete console log. Restored private data remains in local operations storage.
