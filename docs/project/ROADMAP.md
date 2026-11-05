# Development priorities

Open work in recommended order, not a delivery calendar. Current capabilities
belong to the [API index](../API.md), boundaries to [architecture](../design/ARCHITECTURE.md).
Completed foundations are not repeated as tasks here.

The [local review plan dated 5 November 2026](LOCAL_PLAN_20261105.md) expands this
order into eight stages with acceptance gates, current deployment distinctions
and decisions for the next review. Daily backups now include monitor SQLite;
fresh-host recovery remains open. Workspace source is fixture-qualified but has
not been promoted to the live pilot.

## Execution order and completion gates

1. **Close stage 5: reliable pilot operation.** Verify a real Windows reboot/login,
   preserve the backup recovery key separately and address remaining upstream
   security findings. The [local stack measurements](../performance/baselines/SERVER.md#mixed-docker-pilot-30-october-2026)
   do not certify long-term/internet capacity or another server's resource limits.
2. **Account-scoped preferences.** These defaults are already deployed to the
   pilot. Preserve ownership/revisions, explicit guest import and logout clearing
   during subsequent changes. Gate: restart/second-browser persistence, two-user
   isolation and concurrent-tab checks for the selected release.
3. **Named workspaces.** Manual create/load/rename/duplicate/delete of
   inputs/settings is implemented and fixture-qualified, with schema versions,
   quotas and opening without calculation. Rehearse migration/rollback and review
   the [rollout proposal](WORKSPACE_ROLLOUT_20261105.md) before promotion. Then
   optional autosave with offline recovery and conflict handling. Gate: different
   devices, forbidden IDs, limits and failed/concurrent writes.
4. **Account lifecycle.** Profile/password controls, device revocation, export/
   deletion and backup-retention policy. Public registration follows tested email
   verification/reset and reauthentication for sensitive changes.
5. **Typed calculation restore.** Bind private C sessions to authenticated owners,
   then add bounded transactional value/context import. Never replay expressions
   or random calls to reconstruct state. Gate: exact values, cross-user denial
   and unchanged state on malformed/oversized/failed import.

[Account contracts](../guides/ACCOUNTS_STORAGE.md) ·
[Storage design](../design/APPLICATION_DESIGN.md#proposed-accounts-and-saved-work) ·
[Compose operation](../guides/CONTAINERS.md).

## Checks alongside development

Finish outstanding numerical/regression/CI changes with independent references
and comparable measurements. Validate touch keyboard, copying, login and graph
gestures on physical devices; browser viewport emulation does not close that gate.
After an explicitly requested push, verify CI for that exact revision, including
account/container jobs. Older green CI does not validate newer local work.

[Concurrency ownership](../design/ARCHITECTURE.md#concurrent-server-prerequisites) ·
[Operating limits](../guides/HOSTING.md#capacity-and-state) ·
[Restart/rollback checks](RELEASE_PACKAGING.md#restart-and-rollback-rehearsal)

## Accuracy and performance priorities

Approximate working precision is not a universal correct-rounding guarantee.
Preserve existing contracts until stronger independent evidence supports changes.
Graph refinement/root discovery remain heuristic and potentially incomplete.
Candidate work: stronger rounding enclosures, lower interval-allocation traffic,
broader contact discovery and configurable root tolerances.

Optimize only from independently validated, repeated Release measurements;
structural cleanup alone is not a speedup. [Measurement method](../testing/PERFORMANCE.md).

## Later, according to use cases

Symbolic assumptions/rewrites, ODE/scientific models, wider matrix domains,
distributions, native apps and collaboration need separate contracts/use cases.
The calculator AST is not a public CAS; accounts never become a math dependency.
