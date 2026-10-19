# API guide

Choose the interface you use. Arithmetic, application state and HTTP transport
have different ownership and compatibility contracts.

| Reference | What it covers |
| --- | --- |
| [Numerical methods](reference/NUMERICAL_API.md) | Exact grids, adaptive scheduling/curve acceptance, phase anchors and bounded root searches with explicit work/accuracy semantics. |
| [Public C API](reference/C_API.md) | BigInt, BigDecimal, BigRational, BigComplex, BigRationalComplex, units, runtime scopes and ownership. |
| [Standalone engine C API](reference/ENGINE_API.md) | Public stateless scalar expressions, reusable handles, typed results, precision and budgets. |
| [Exact polynomial C API](reference/POLYNOMIAL_API.md) | Standalone linear/quadratic coefficient solver with exact radical/complex descriptors; numeric target only. |
| [Exact matrix C API](reference/MATRIX_API.md) | Dense rational products, determinants and unique square-system solves; numeric target only. |
| [General linear systems](reference/LINEAR_SYSTEM_API.md) | Exact rectangular rank, inconsistency/uniqueness, particular solutions and a complete nullspace basis in C/Python. |
| [Exact descriptive statistics](reference/STATISTICS_API.md) | Rational dataset count, sum, extrema, mean and population/sample variances in C/Python. |
| [Python adapter](guides/PYTHON.md) | Source-built CPython subset for typed values, expressions, principal roots, exact equations, matrices, general linear systems and descriptive statistics. |
| [BigComplex foundation](reference/BIGCOMPLEX.md) | Standalone complex arithmetic, principal functions, precision and calculator integration. |
| [Calculator expressions](reference/CALCULATOR_EXPRESSIONS.md) | Operators, named calls, precision, syntax and limits. |
| [Polynomial equation application](design/POLYNOMIAL_EQUATIONS.md) | Private parsing/preview/context adapters over the public exact coefficient solver. |
| [Polynomial equation HTTP API](reference/EQUATION_HTTP_API.md) | Whole polynomial equations or exact coefficient expressions, algebraic root text and approximate decimal previews. |
| [Numerical interval roots](reference/NUMERIC_ROOT_HTTP_API.md) | One high-precision bracketed root or bounded multiple-root scanning, residuals and diagnostic outcomes. |
| [Local HTTP API](reference/HTTP_API.md) | Evaluation, preview/confirmation, client IDs, revisions and errors. |
| [Session HTTP API](reference/SESSION_HTTP_API.md) | Authoritative variables/history, typed snapshots, reset/release, functions and conversion history. |
| [Unit HTTP API](reference/UNIT_HTTP_API.md) | Catalogue and compatible expression conversions. |
| [Graph sampling HTTP API](reference/GRAPH_HTTP_API.md) | Bounded uniform grids, plotting projections, point classifications and read-only session snapshots. |
| [Screen-space graph HTTP API](reference/SCREEN_GRAPH_HTTP_API.md) | Adaptive display sampling, derivatives, dense ranges and precision fallback. |
| [Graph points and verification](reference/GRAPH_ANALYSIS_HTTP_API.md) | Integer-domain sampling and bounded C confirmation of marker candidates. |
| [Web/API coverage](reference/WEB_API_PARITY.md) | Supported web actions and application API gaps. |

## Values and sessions

- [Variables](guides/VARIABLES.md): confirmed assignments, deletion, snapshots and lifetime.
- [Units](reference/UNITS.md): exact conversion, case-sensitive IDs and sourced definitions.
- [Quantities](reference/QUANTITIES.md): `qty(...)`, dimensions, arithmetic and temperature rules.
- [Browser calculator](guides/CALCULATOR_WEB.md) and [converter](guides/UNIT_CONVERTER.md): user controls.

Public installed interfaces are declared under `include/numforge/`; expression
evaluation links the optional `NumForge::engine` target. Internal calculator/session
headers remain private. The HTTP server is a local loopback tool;
it is not a separately versioned public Internet service. See
[Application design](design/APPLICATION_DESIGN.md) for the boundaries.
