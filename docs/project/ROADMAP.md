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
| Statistics | [Exact rational batch summaries](../reference/STATISTICS_API.md), including population/sample variance. |
| Numerical | [Exact grids, adaptive sampling rules, phase anchors and bounded root searches](../reference/NUMERICAL_API.md), shared with graph/equation applications. |
| Scientific | [Unit conversion](../reference/UNITS.md), a sourced catalogue and private [quantity expressions](../reference/QUANTITIES.md). |
| Applications | SK/EN CLI/browser calculator, sessions, converter, four-function graphs and bounded numerical equation searches. |
| Integration | Installed C/C++ packages and an experimental source-built [Python adapter](../guides/PYTHON.md), including exact grids and callback-based single/multiple-root searches through the C kernels. |

The [architecture](../design/ARCHITECTURE.md) defines ownership and extension
rules. Numeric-only use does not require an engine, application session or web
server. Source directories now follow those boundaries; each implemented module
owns its source manifest. There are no empty future-module placeholders.

## Recommended next work

1. Extend one concrete mathematical use case at a time, following the
   [feature checklist](FEATURE_CHECKLIST.md). Likely candidates include matrix
   decompositions and multivariate/streaming statistics; each needs its own
   numerical and failure contract.
2. Extend the public numerical-method contracts deliberately: acceleration and
   future sampling methods and expression-metadata adapters must preserve
   convergence, incomplete discovery and residual/error semantics. Phase anchors
   are not general extrema of composed functions.
   Preserve graph/session semantics while gradually adopting public engine handles
   where their supported scope fits.
3. Add session user-function definitions and their graph/solver integration when
   required. Define unit semantics before adding dimensional equations or axes.
4. Broaden adapter publication and platform coverage deliberately. Python wheel
   distribution and parallel/free-threaded execution remain separate work. Public
   REST, WASM and other language bindings are future interfaces.
5. Evolve documentation incrementally: Doxygen syntax in public C headers is the
   contract source; Markdown explains use and mathematical meaning. Generated
   Doxygen/MkDocs publication can follow without becoming a library prerequisite.

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
