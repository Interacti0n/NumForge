# Current scope and roadmap

This is a development direction, not a release schedule. Source development can
be ahead of published archives. [API.md](../API.md) is the supported-operation
index; selected baselines preserve evidence for their recorded revisions.

## Available in the current source

| Area | Supported foundation |
| --- | --- |
| Arithmetic | Opaque C17 integer, finite-decimal, rational and complex types; exact arithmetic and precision-aware approximations. |
| Expression engine | [Stateless public C API](../reference/ENGINE_API.md), reusable prepared expressions, explicit parameters, typed results and operation budgets. |
| Algebra | [Exact linear/quadratic coefficient solver](../reference/POLYNOMIAL_API.md), shared by application equation adapters. |
| Linear algebra | [Dense rational matrices](../reference/MATRIX_API.md), products, determinants, square solves and [complete rectangular single-RHS systems](../reference/LINEAR_SYSTEM_API.md). |
| Statistics | [Exact rational batch summaries, paired covariance and simple linear regression](../reference/STATISTICS_API.md), with population/sample divisors in C/Python. |
| Numerical | [Exact grids, adaptive sampling rules, phase anchors, bounded roots and finite-interval integration](../reference/NUMERICAL_API.md); graph/equation applications reuse sampling/root kernels. |
| Scientific | [Unit conversion](../reference/UNITS.md), a sourced catalogue and private [quantity expressions](../reference/QUANTITIES.md). |
| Applications | SK/EN CLI/browser calculator, sessions, converter, four-function graphs and bounded numerical equation searches. |
| Integration | Installed C/C++ packages and an experimental source-built [Python adapter](../guides/PYTHON.md), including grids, roots and finite-interval integration through the C kernels. |

The [architecture](../design/ARCHITECTURE.md) defines ownership and extension
rules. Numeric-only use does not require an engine, application session or web
server. Source directories now follow those boundaries; each implemented module
owns its source manifest. There are no empty future-module placeholders.

## Execution order and completion gates

**Current milestone: stage 5, concurrent server preparation.** Structural cleanup,
the documentation workflow, C/Python regression and integration, and the selected
equation-to-graph/package workflow are complete (stages 1–4). Their contracts and
evidence live in the references above, [changelog](../../CHANGELOG.md),
[performance index](../performance/README.md) and [release verification](RELEASES.md).
They are no longer open planning tasks.

The [server audit](../design/ARCHITECTURE.md#concurrent-server-prerequisites),
[load driver](../performance/protocols/APPLICATION.md#multi-client-load) and
[initial Windows baseline](../performance/baselines/SERVER.md) are available.
Request reservations, synchronized metadata, thread-local seed fallback,
shared graph/session reads, bounded FIFO conflict scheduling and a fixed worker
pool with bounded connection waiting are implemented.
Contention tests cover writer fairness, timeouts, limits, snapshots and replay.
Worker/HTTP tests cover overload, queue expiry, shutdown and recovery. There are
still eight session slots and eight legacy cache slots, with no TTL. Session
policy and sustained-load validation remain before the stage 5 gate.

| Next, in order | Completion gate |
| --- | --- |
| Set session capacity, expiry and memory policy | Held state stays valid; count, idle lifetime and per-client/global retained memory have explicit bounds and expiry behavior. |
| Validate the intended concurrent load | Repeat 10/30/60-client scenarios, sustained load, isolation, overload and recovery; record verified results, latency, errors, queueing and memory against the original protocol. Choose capacity from evidence. |
| Complete deployment and rollback | Reproducible target-platform packages, startup/restart, operating limits and rollback are exercised. A local calculator smoke test alone is insufficient. |

Finish this bounded milestone before opening stage 6. Then choose **one** capability
for a demonstrated use case: a matrix decomposition, another numerical method,
broader statistics, user functions or a first symbolic subset. Define the owning
module, interfaces, assumptions and [feature completion gate](FEATURE_CHECKLIST.md)
before implementation. Persistence requires a defined saved-work/account feature.
Push, release publication and deployment remain separately authorized actions;
this is an ordered plan, not a release calendar.

## Accuracy and performance priorities

General correctly rounded approximate functions remain an open goal. Working
precision and guard digits are not universal error guarantees, particularly after
cancellation or poorly conditioned composition. Preserve existing rounding
exceptions until supported by stronger evidence. Independent decimal references
and [measurements](../performance/protocols/DECIMAL.md) cover selected cases
through 2,000 significant digits, not every possible input.

Further optimizations need repeated Release measurements with independently
validated results. Existing arithmetic, factorial and logarithm optimizations are
recorded in the [benchmark guide](../../benchmarks/README.md); their implementation
chronology is not repeated here. Lower interval-allocation traffic and stronger
enclosure-based rounding remain candidates. Reorganization or code reduction
alone does not establish a speedup.

Graph refinement remains heuristic rather than a continuity proof; numerical
root discovery is bounded and does not guarantee completeness. General tangency
discovery, configurable tolerances, broader physical-device validation and
graph export/import remain future work. Release gates are tracked in the
[published release verification](RELEASES.md).

## Later, according to use cases

Symbolic mathematics requires an explicit expression/assumption model; the current
calculator AST is not a public CAS. Scientific simulations, approximate/complex
matrix domains and broader distributions need individual contracts.
Accounts and persistence remain proposals behind
the application layer, independent of mathematical algorithms.
