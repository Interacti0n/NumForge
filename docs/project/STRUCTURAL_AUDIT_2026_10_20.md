# Structural cleanup audit — 20 October 2026

Status: stage 1 of the [roadmap](ROADMAP.md#execution-order-and-completion-gates)
is closed for this bounded audit. Baseline: `d725a56`, after exact covariance and
the agreed milestone sequence. This record describes the accompanying cleanup;
it does not certify the absence of every possible duplicate or unused branch.

## Scope and method

Reviewed the source ownership/manifests and direct includes of runtime, numeric
core, mathematical modules, engine, application, HTTP/CLI/server and native Python
adapters. The architecture guard currently checks 141 implementation C/header
files plus installed public headers. Also searched for identical normalized
function bodies and functions with few source references, then inspected their
call sites, visibility, error policies and ownership before changing them.
Public APIs are retained even when the repository has no internal caller.

No directory migration, new mathematical capability, public API expansion or
server concurrency redesign is part of this audit. The existing architecture
remains the [source ownership contract](../design/ARCHITECTURE.md).

## Resolved findings

| Finding | Resolution |
| --- | --- |
| Constant and formatting adapters repeated the decimal status map. | Both use the existing engine API helper with the same historical INVALID_ARGUMENT policy for unmapped statuses. |
| Conversion computation and HTTP serialization repeated the same rational status map. | One private application conversion helper serves both; NULL/division-by-zero still map to INVALID_ARGUMENT. |
| Constants and hyperbolic functions duplicated transactional half-even add/subtract/multiply helpers. Three decimal families duplicated uint64 text conversion. | Shared private helpers in the existing decimal operations source preserve arithmetic order, rounding, alias handling and allocation sequence. No installed header changes. |
| Session and unit queries duplicated ASCII percent/plus decoding. | One allocation-free bounded HTTP helper serves both, preserving nonempty/printable-ASCII/capacity rules. Exhaustive encoded-byte and malformed/boundary checks cover the shared parser. |
| Single-root and multi-root adapters repeated context/equation validation. | Shared private validation preserves status precedence, input limits, error offsets and operation budgets. |
| A superseded boolean Brent experiment entry point had no callers. | Removed its declaration and wrapper. The method-selecting benchmark/test API and its actual algorithms remain available. |
| Architecture checks did not cover every implementation manifest or reject bare private-header includes/core-storage access outside the core. | Extend ownership/manifest checks to every existing layer, missing registered files and unowned C files. Comments cannot register sources/modules. Restrict core internal headers to the core, the existing profiler and two exact-root helper consumers. Add isolated guard regression fixtures. |

The application/core cleanup removes more production lines than it adds. The
additional guard/tests provide continuing checks rather than another runtime
framework. Reduced source size alone is not a performance claim.

## Explicitly retained or deferred

| Area | Decision and reason | Revisit when |
| --- | --- | --- |
| Application access to parser/evaluator/value contracts | Retain the existing prepared application expressions, snapshots, quantity handling, domain analysis, coefficient extraction and fast graph compiler. They inspect or bind expression nodes in ways the opaque public prepared-expression API does not expose. Replacing them now would require new contracts and broader behavioral migration. Lower layers still do not depend on applications; transport/Python cannot import these internals. | A concrete public expression metadata/visitor or user-function requirement is designed and tested. |
| Exact rational square-root helper | Retain the shared private core function used by polynomial solving and exact expression evaluation. It exposes typed operations, not numeric storage; the guard permits only these existing consumers. | A supported public exact-root query is actually needed. |
| Brent/hybrid strategy implementation | Retain the tested method-selection adapter and narrow module callback hooks. Production solving continues to select bisection. These sources are used by comparisons/failure tests, so they are not dead code. Separating their build would require another adapter/target split. | An algorithm is promoted to production or the research path is deliberately retired. |
| Similar status/projection/rounding helpers with different behavior | Retain Python's public-boundary status conversion; legacy decimal/integer/unit maps with different fallback policies; strict fast-expression projection versus graph NAN projection; and work-buffer-only arithmetic that intentionally avoids transactional allocations. Combining these merely by name would change errors, ownership or cost. | A deliberate contract change or measured optimization provides migration evidence. |
| Additional source moves, public unused APIs and future module scaffolding | No further move is required by a confirmed dependency finding. Public functions are not removed based on low local reference counts. Future modules are added with an implemented use case, not empty directories. | A new capability has a defined owner and completion contract. |

These decisions exhaust the findings of this audit. They are not hidden work
needed before stage 2. Documentation consolidation is next; linear regression
and numerical integration remain stage 3 work rather than being added during cleanup.

## Verification

- MSVC C17 Release: all **75 CTest groups pass**, including allocation/deadline,
  numerical references, expressions, formatting, equations, graphs, unit queries,
  sessions, CLI and embedded-server checks.
- The shared query decoder checks all 256 encoded byte values, malformed escapes,
  lowercase hex, plus/space handling, nonempty inputs, control/non-ASCII rejection,
  exact capacity boundaries and NULL inputs.
- **42 isolated architecture guard cases pass**. Fixtures live in a fresh temp
  directory, with verified cleanup bounds; they never modify working sources.
- The numeric-only MSVC build succeeds with engine/apps/tests disabled. The GCC
  Release engine target builds without applications, with warnings as errors.
  Ten installed C/C++ consumer checks and eleven SDK examples pass against the
  updated installed numeric-only library.
- Source archive → wheel → isolated installation passes all **68 Python tests**
  and the standalone example. No global installation or publication occurs.
- **35 targeted Chromium cases pass**, with no retries: converter, unit HTTP API,
  session API, equations and root scans, including both languages and responsive
  layouts. This is focused regression coverage, not a new full browser-matrix run.

## Performance comparison

Use the existing [decimal benchmark](../benchmarks/DECIMAL_MATH_BENCHMARKS.md)
and its committed `benchmarks/references/decimal_math_references.tsv` corpus.
Build `decimal_math_benchmark` in GCC Release with benchmark instrumentation.
Keep the baseline executable from `d725a56`, then rebuild the changed sources
with the same configuration. For each case below, run both executables with
`--quick --case NAME`, alternating their order over three rounds. Each quick
invocation uses three timing samples and separate memory/profile probes;
independent exact references are checked outside timing.

Cases: `pi_factory_p1000`, `phi_factory_p1000`, `exp_ordinary_p1000`,
`sinh_ordinary_p1000`, `cosh_large_p1000`, `tanh_ordinary_p1000`,
`sin_ordinary_p1000`, `atanh_near_boundary_p1000`.

All eight retain identical validated status/results, allocation counts, requested
bytes, baseline/live storage and peak tracked storage. Temporary storage is
released back to its baseline. Process RSS and wall-clock timings vary on the
shared machine and are not treated as exact invariants. Raw local CSVs remain
under ignored `build/structural-decimal-*.csv`; this cleanup makes no speedup claim.

Ranges below are minima/maxima of the three invocation medians in milliseconds,
not statistical confidence intervals. Median ratios across rounds range from
0.958 to 1.060; these short local runs do not prove universally unchanged speed.

| Case, 1000 digits | Before ms | After ms |
| --- | ---: | ---: |
| pi factory | 7.682–7.949 | 7.801–8.831 |
| phi factory | 0.340–0.377 | 0.331–0.363 |
| exp(1) | 32.585–33.868 | 32.298–34.162 |
| sinh | 37.883–39.239 | 38.010–38.640 |
| cosh, large argument | 40.803–41.544 | 39.078–39.460 |
| tanh | 33.899–35.510 | 32.460–34.618 |
| sin | 22.782–23.644 | 23.130–23.948 |
| atanh near boundary | 55.870–59.396 | 57.030–58.597 |
