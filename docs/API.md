# API guide

Typed mathematics is independent of expressions, sessions and HTTP. Installed
headers define each C package's available API; current source can be newer than
published downloads.

## C library and Python

| Need | Reference |
| --- | --- |
| Numeric types, ownership, budgets | [C API](reference/C_API.md), [complex](reference/BIGCOMPLEX.md) |
| Stateless/prepared expressions | [Engine](reference/ENGINE_API.md) |
| Exact linear/quadratic roots | [Polynomials](reference/POLYNOMIAL_API.md) |
| Rational matrices and systems | [Matrices](reference/MATRIX_API.md), [general systems](reference/LINEAR_SYSTEM_API.md) |
| Summaries, covariance, regression | [Statistics](reference/STATISTICS_API.md) |
| Integration, grids, sampling, roots | [Numerical APIs](reference/NUMERICAL_API.md) |
| Python build and examples | [Python](guides/PYTHON.md) |

Numeric/module calls link `NumForge::numforge`; expressions add
`NumForge::engine`. Python delegates to these C implementations.

## Applications and HTTP

| Need | Reference |
| --- | --- |
| Expression syntax/domains | [Expressions](reference/CALCULATOR_EXPRESSIONS.md) |
| Evaluation, revisions, errors | [HTTP](reference/HTTP_API.md) |
| Variables, history, lifecycle | [Sessions](reference/SESSION_HTTP_API.md) |
| Units and supplied currency rates | [Conversion](reference/UNIT_HTTP_API.md) |
| Polynomial/numerical equations | [Exact](reference/EQUATION_HTTP_API.md), [interval search](reference/NUMERIC_ROOT_HTTP_API.md) |
| Matrix HTTP | [Matrix editor/API](reference/MATRIX_API.md#web-editor-and-http) |
| Graphs | [Uniform](reference/GRAPH_HTTP_API.md), [adaptive](reference/ADAPTIVE_GRAPH_HTTP_API.md), [screen](reference/SCREEN_GRAPH_HTTP_API.md), [verification](reference/GRAPH_ANALYSIS_HTTP_API.md) |
| Interface availability/gaps | [Coverage](reference/WEB_API_PARITY.md) |

## Values and sessions

[Variables](guides/VARIABLES.md) · [Units](reference/UNITS.md) ·
[Quantities](reference/QUANTITIES.md) · [Hosting](guides/HOSTING.md)

Application/session headers stay private. HTTP is a loopback application
interface, not a separately versioned public service.
