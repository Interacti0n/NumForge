# Workspace rollout proposal on 5 November 2026

This is a reviewable deployment proposal, not a deployment record. Live accounts
still use the qualified preferences release with 11 tables. Current source has
manual named workspaces and `identity.0004_workspace`, exercised in isolated
12-table fixtures, including 22 API tests and three browser suites. Those earlier
fixtures do not qualify an arbitrary future revision automatically.

## Scope and candidate

Ship web and accounts together: manual create/open/rename/duplicate/delete for
calculator, graph, equation and matrix inputs/settings. Opening must never run a
calculation. Results, history, C variables and `ans` are excluded; autosave is a
separate decision. The [storage contract](../guides/ACCOUNTS_STORAGE.md#named-workspaces)
defines ownership, revisions, quotas and schema version 1.

Pin the selected source commit and exact candidate image IDs, retaining the
existing accounts SQLite backport and other qualified system repairs. Build
through the ordinary Dockerfiles, scan exact layers with a current vulnerability
database, and rerun the current stack/API/browser checks. Hold on new unexplained
findings, ownership leaks, changed calculation behavior or restore regressions.

## Migration rehearsal before promotion

1. Restore a verified current live dump into a newly named fixture database.
   Record all original table row hashes/counts, roles, grants and migration ledger.
2. Run migrations using the candidate setup image and the migration role, never
   the HTTP application's role. Confirm `identity.0004_workspace`, the twelfth
   table, owner foreign key/index and quota/revision behavior.
3. Verify original users, sessions, preferences and grants are unchanged apart
   from the intended migration ledger entry, workspace schema/grants and exact
   Django metadata additions (one workspace content type and four permissions).
4. Exercise two users and two browser contexts: create, open without calculation,
   rename, duplicate, delete, logout/account switching and delayed responses.
5. Exercise foreign UUIDs, stale revisions, concurrent quota writes, oversized and
   malformed payloads, lost responses and offline saves. Failed saves preserve
   the visible draft and never silently truncate it.
6. Save examples for all four tools, restart, restore a new logical dump and check
   their exact inputs/settings. Include non-ASCII names and unfinished expressions.
7. Start the old web/accounts images against the migrated fixture schema and
   verify the old preferences/login contract. This compatibility gate is open
   until the exact selected pair has passed it.

## Production sequence after review

Use the pilot operation lock and a short controlled write pause. Record current
image IDs, runtime profile and source hashes. Create verified pre-change backups
of accounts and monitoring. Recheck that the live profile/images still match the
reviewed baseline before running migration.

Apply the additive migration using setup, select the qualified web/accounts pair,
and wait for health. Run private HTTPS login/preferences/workspace/calculate smoke
checks, monitor checks and restart persistence. Record the 12-table schema and
new exact images. Create and restore post-change backups containing workspace
data before declaring promotion complete. Keep rollback images and pre/post
snapshots separately identified through the observation period.

## Rollback and preservation of new writes

Prefer application rollback to the old images while leaving the additive table
intact, **only after** the compatibility rehearsal passes. Disable new workspace
writes during rollback; retain a verified dump of all new workspace data. The old
interface may not expose it, but it must remain recoverable.

Reversing `0004_workspace` drops the workspace table. Do not automate that as
routine rollback. Restoring a pre-change database loses subsequent account,
preference and workspace writes; it needs a recorded recovery choice and a
preserved latest dump. If additive-schema compatibility fails, stop promotion
until a migration/rollback strategy with an explicit data-preservation test exists.

## Decisions and acceptance

Agree on deployment/observation windows, who performs two-device acceptance,
where preserved rollback data lives and when its retention may end. No release
is accepted solely because container health is green. Autosave, account deletion,
public registration and synchronizing calculation state remain separate stages
in the [local plan](LOCAL_PLAN_20261105.md).

## Completed migration rehearsal on 11 November 2026

The reusable [rollout fixture](../../tests/containers/check_workspace_rollout.py)
restored verified daily snapshot
`7b08a4806951f472bf4c3da5d631f3a42aaa55cc4b77efb2c12d932180021ce4`
into a new internal-only Compose project with no published ports. It compared
old account image `sha256:3080228383f4dc95c8f11940f10f9fe81c389b95cca39b706979ba422ace30eb`
and freshly built candidate
`sha256:c8d8b30c894d577ffb296073691f1752d750c64c11dd149a2a7f539e70dbe777`.

The successful result is `build/closeout-20261111/workspace-rollout-2/result.json`:

- migration from 11 to 12 tables preserved original application rows, roles and
  grants; existing content-type and permission rows also remained equivalent;
- Django added only the workspace content type, its add/change/delete/view
  permissions and the expected migration ledger entry;
- synthetic unfinished calculator input, exact matrix strings and non-ASCII
  workspace names survived saving, old-app rollback and candidate reapplication;
- the old application's real Django login/preferences request stack remained
  functional with the additive table present, including a new preference write;
- a post-migration dump restored all twelve tables and every row, including the
  old-app write and candidate workspace data;
- native SQLite backport regressions passed; all fixture resources were removed.

The first attempt rejected Django's expected metadata additions under an overly
strict unchanged-table hash check. That fixture was cleaned up; the refined check
allows only the exact additions above and still rejects changes to old metadata.
Four host guard tests, also in CI, cover original data/grant/role changes,
unexpected tables/ledger entries and unauthorized metadata additions.

Run locally against a verified pilot backup and an available candidate image:

```sh
python tests/containers/check_workspace_rollout.py --candidate IMAGE --output build/rollout-UNIQUE
```

This closes the additive migration/application rollback gate for this pair. It
does not deploy workspaces or certify a future web/accounts pair, browser UX,
two-device behavior or browser transport through the gateway. Rerun the full
API/browser and exact-image security gates for the pair selected for promotion.
The candidate's current Trivy report has no listed fixes at any severity; the
remaining system advisory records are not suppressed.
