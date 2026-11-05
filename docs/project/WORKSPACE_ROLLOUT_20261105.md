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
   from the intended migration ledger entry and workspace schema/grants.
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
