# Development priorities

Open work in recommended order, not a delivery calendar. Current capabilities
belong to the [API index](../API.md), boundaries to [architecture](../design/ARCHITECTURE.md).
Completed foundations are not repeated as tasks here.

## Execution order and completion gates

1. **Close stage 5: reliable pilot operation.** Automate Docker/pilot-tunnel
   startup and recovery, including changed tunnel origin and public HTTPS health.
   Schedule protected backups, off-PC copies, restore drills and expired-session
   cleanup. Verify a real Windows reboot/login and representative mixed load on
   the complete C/Django/PostgreSQL stack. Existing C-server measurements do not
   certify this stack. Host responsiveness and resource limits need that evidence.
2. **Account-scoped preferences.** Persist language/precision/notation/rounding/
   angle defaults, validate ownership and revisions, explicitly import guest data
   and clear account state on logout. Gate: restart/second-browser persistence,
   two-user isolation and concurrent-tab checks.
3. **Named workspaces.** Save inputs/settings first: create/load/rename/duplicate/
   delete with schema versions and quotas, restoring without calculation. Then
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
