# Current scope and roadmap

Development order, not a delivery calendar. [API index](../API.md) owns current
availability; [architecture](../design/ARCHITECTURE.md) owns module boundaries.

## Available in the current source

Standalone numeric types, a public expression engine, exact linear/quadratic
solving, rational matrices/general systems, statistics/regression, grids/sampling,
bounded roots and finite-interval integration. C/C++ and experimental Python
share C kernels. The SK/EN application adds calculator sessions, units/currencies,
graphs with files/image export, equations and a matrix editor.

Typed math needs no web/session/database. See the [API index](../API.md) for exact
interface coverage; published downloads may lag source.

## Execution order and completion gates

1. **Stages 1–4 complete:** modular structure, documentation/testing workflow,
   C/Python integration and equation/graph/package foundations.
2. **Stage 5 open: operating validation on the Windows preview PC.** Workers,
   bounded queues, session quotas/expiry, hidden recovery, package restart/rollback
   and [local/HTTPS load checks](../performance/baselines/SERVER.md) exist. The
   4-worker/64-slot BelowNormal profile has no hard CPU/RAM cap. Real reboot/login
   validation remains deferred; do not mark this stage complete before it passes.
3. **Review the unified UI** on actual desktop/mobile devices; fix concrete
   usability findings without replacing mathematical algorithms.
4. **Next selected feature: PostgreSQL accounts and saved work.** Follow the
   [account proposal](../design/APPLICATION_DESIGN.md#proposed-accounts-and-saved-work).
   Backend implementation is pending; current sessions are anonymous/memory-only.
5. Add subsequent mathematics through one owning module and the
   [feature checklist](FEATURE_CHECKLIST.md). Push/publication require their
   own authorization.

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
