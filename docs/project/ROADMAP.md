# Current scope and roadmap

This is a development direction, not a release schedule. Source development can
be ahead of published archives. [API.md](../API.md) is the supported-operation
index; dated reports preserve evidence for their recorded revisions.

## Available in the current source

| Area | Supported foundation |
| --- | --- |
| Arithmetic | Opaque C17 integer, finite-decimal, rational and complex types; exact arithmetic and precision-aware approximations. |
| Expression engine | [Stateless public C API](../reference/ENGINE_API.md), reusable prepared expressions, explicit parameters, typed results and operation budgets. |
| Algebra | [Exact linear/quadratic coefficient solver](../reference/POLYNOMIAL_API.md), shared by application equation adapters. |
| Linear algebra | [Dense rational matrices](../reference/MATRIX_API.md), products, determinants, square solves and [complete rectangular single-RHS systems](../reference/LINEAR_SYSTEM_API.md). |
| Statistics | [Exact rational batch summaries and paired covariance](../reference/STATISTICS_API.md), with population/sample divisors in C/Python. |
| Numerical | [Exact grids, adaptive sampling rules, phase anchors and bounded root searches](../reference/NUMERICAL_API.md), shared with graph/equation applications. |
| Scientific | [Unit conversion](../reference/UNITS.md), a sourced catalogue and private [quantity expressions](../reference/QUANTITIES.md). |
| Applications | SK/EN CLI/browser calculator, sessions, converter, four-function graphs and bounded numerical equation searches. |
| Integration | Installed C/C++ packages and an experimental source-built [Python adapter](../guides/PYTHON.md), including exact grids and callback-based single/multiple-root searches through the C kernels. |

The [architecture](../design/ARCHITECTURE.md) defines ownership and extension
rules. Numeric-only use does not require an engine, application session or web
server. Source directories now follow those boundaries; each implemented module
owns its source manifest. There are no empty future-module placeholders.

## Execution order and completion gates

Planning baseline: 20 October 2026, after exact C/Python covariance. Stage 1 is
complete for the [bounded structural audit](STRUCTURAL_AUDIT_2026_10_20.md), with
explicit retained/deferred findings. Stage 2 is next; stages 2–6 remain planned.
Finish each bounded
stage before opening the next; a defect found in a completed stage can interrupt
the sequence. Dates are not promised: progress is determined by the exit evidence.
Do not keep adding mathematical functions while stages 1 and 2 remain open.

| Stage | Work | Gate for moving on |
| --- | --- | --- |
| 1. Close the structural cleanup | Make one focused audit of remaining application/engine/module dependencies, duplicate conversion/error/ownership helpers and obsolete code. Fix confirmed findings in small commits; record justified exceptions and deferred findings. Avoid another wholesale directory move. | Every audit finding is resolved or explicitly deferred with a reason. Numeric-only, engine and application builds still work; affected correctness, failure, consumer and performance checks pass. |
| 2. Close documentation cleanup | Remove conflicting or duplicate instructions and stale API claims. Keep C contracts in public headers, usage in guides, detailed operation meaning in references and measured evidence in reports. Add a minimal Doxygen/MkDocs generation workflow using the existing sources. | One canonical home per contract/topic, valid links, executable examples and reproducible generated documentation. Historical reports stay labelled by revision/date. Library users do not need documentation tools to build the library. |
| 3. Complete one useful mathematical workflow | First add exact simple linear regression with intercept, reusing the statistics foundation; define constant-x/insufficient-data behavior. Then add one bounded numerical integration API for a finite interval, with evaluation limits, domain-failure rules and an explicit approximate-error contract. Complete them separately. | Each capability meets the [feature checklist](FEATURE_CHECKLIST.md): independent C API, thin Python adapter, documented exactness/limits, examples, independent edge/high-precision and failure tests, installed consumers and validated benchmarks. Both are complete before stage 4. |
| 4. Consolidate a usable platform increment | Choose one end-to-end application workflow over the public kernels and complete its user-facing behavior. Validate the supported source/package interfaces and extracted release artifacts on the intended platform matrix. Stabilize the current C/Python scope before adding more language bindings. | The selected workflow has application/HTTP/browser regression evidence where applicable; installed packages and examples work outside the repository. Public scope, experimental interfaces and unsupported features are explicit. Publishing or pushing remains a separate user-authorized action. |
| 5. Prepare concurrent server use | Define an actual load scenario for tens of simultaneous users. Choose bounded execution/queueing, session capacity/isolation, request and calculation limits, and failure/restart behavior. Measure latency, errors, queueing and memory under that scenario before selecting deployment capacity. | The agreed concurrent-load scenario passes with isolated sessions, bounded resource use and working overload/recovery behavior. Document a reproducible deployment and rollback procedure. A local calculator smoke test alone does not pass this gate. |
| 6. Expand according to demonstrated needs | Choose the next single capability: matrix decompositions, additional numerical methods, broader statistics, user functions or a first symbolic subset. Introduce persistent storage only for a defined saved-work/account feature. | Before implementation, name the use case, owning module, supported interfaces, assumptions and completion evidence. Reuse the same feature checklist; do not open all future modules at once. |

Stage 1 is an audit with an explicit findings list, not an indefinite promise to
make the entire codebase smaller. Stage 2 does not rewrite historical reports as
current evidence. Stage 3 proves that the cleaned boundaries support useful new
work; it is not a commitment to implement a complete statistics or calculus suite.
Stages 4 and 5 are separate: packaging a working library/application does not
establish concurrent server readiness. Revisit this sequence only for a concrete
new requirement or a blocking finding, and update this document when it changes.

## Accuracy and performance priorities

General correctly rounded approximate functions remain an open goal. Working
precision and guard digits are not universal error guarantees, particularly after
cancellation or poorly conditioned composition. Preserve existing rounding
exceptions until supported by stronger evidence. Independent decimal references
and [measurements](../benchmarks/DECIMAL_MATH_BENCHMARKS.md) cover selected cases
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
[equation readiness record](EQUATION_RELEASE_AUDIT_2026_10_19.md).

## Later, according to use cases

Symbolic mathematics requires an explicit expression/assumption model; the current
calculator AST is not a public CAS. Scientific simulations, approximate/complex
matrix domains and broader distributions need individual contracts.
[Accounts and persistence](../design/ACCOUNT_DESIGN.md) remain proposals behind
the application layer, independent of mathematical algorithms.
