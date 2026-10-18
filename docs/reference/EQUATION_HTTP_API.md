# Polynomial equation HTTP API

`POST /api/equations/solve` solves `a*x^2 + b*x + c = 0` using the
[exact C application solver](../design/POLYNOMIAL_EQUATIONS.md). This stateless
endpoint also powers the [browser equation editor](../guides/EQUATIONS.md).
The installed numeric SDK does not export these
private application interfaces.

## Request

The optional `mode` selects the input grammar:

- `mode=expression`: one UTF-8 line containing exactly one `=`, for example
  `3x + 1 = 7`, `(x-2)*(x+3)=0` or `x^2=2`.
- `mode=coefficients` (also the default when mode is omitted): exactly three
  coefficient expression lines as documented below. Existing callers are unchanged.
- `mode=numeric`: a separate [numerical interval search](NUMERIC_ROOT_HTTP_API.md),
  requiring min/max with different precision/result semantics.
- `mode=numeric_scan`: [bounded multiple-root discovery](NUMERIC_ROOT_HTTP_API.md#multiple-root-scan)
  with the same numerical min/max/precision options, always incomplete.

In coefficient mode, use a UTF-8 `text/plain` body with exactly three lines: coefficient expressions
`a`, `b`, then `c`. Do not append a fourth line, even an empty one. CRLF is also
accepted. Each line must contain an expression. The total body limit is 4096
bytes, including separators; the server's 8192-byte request framing limit also
includes headers. NUL bytes are rejected by HTTP framing/dispatch validation.

For expression/coefficient modes, the other query option is `precision`, an integer from 1 to 10000 with default
34. Duplicate, unknown and malformed options are errors. Precision controls the
shared evaluator's decimal working projection; accepted coefficients and
solutions remain exact at every setting. Radians are fixed for this endpoint.

Expressions use the existing calculator parser and evaluator. Finite decimal
literals, fractions and calls producing exact real integer/rational results
are accepted, e.g. `0.5`, `1/3`, `sqrt(4)` and `4! / 6`. Approximate results
such as `sqrt(2)` or `π` are rejected with `not implemented`; they are not
silently frozen into rational coefficients. Complex values are rejected with
`invalid argument`, quantities with `invalid quantity operation`. A mathematically real
expression is accepted only if its final typed result is exact and real.

In coefficient mode, variables and `ans` have no session to resolve against. Assignments and every
registered random-call alias are rejected before evaluation, including nested
calls. The endpoint accepts no `client`, revision, display or angle options.
It never creates or changes sessions, histories, cached results or random state.

In expression mode the only unknown is lowercase `x`. Both sides are parsed by
the existing C calculator parser, converted to exact rational polynomials, then
subtracted before solving. Accepted variable operations are unary +/- and
addition/subtraction/multiplication, division by x-free real rational constants,
and powers 0, 1 or 2 (including postfix `²`). Constant subexpressions use the same
exact typed evaluator as coefficient mode. All intermediate polynomials must
have degree at most two: higher-degree terms are rejected even if a later
subtraction would cancel them. Constant-only powers/functions retain their
normal exact-evaluator support.

Calls involving x (including `pow(x;2)`), variable exponents and denominators
are unsupported (`not implemented`). A syntactically x-dependent denominator
is rejected even if it reduces to a constant. `x/x=1` is never simplified into
an all-values result. Other variables and ans are undefined; random calls,
complex/approximate coefficients and quantities are rejected. Missing, repeated
or chained `=` signs and line breaks are errors. UTF-8 byte offsets are retained
internally; HTTP `column` is the one-based character position in the whole line.

For the single-line grammar, send the equation body to
`/api/equations/solve?mode=expression`. Both modes return the same solution schema
and share the same input, response, time and memory limits.

PowerShell example, with a running local server:

```powershell
$body = "1`n0`n-2"
Invoke-RestMethod -Method Post -Uri 'http://127.0.0.1:8766/api/equations/solve' `
    -ContentType 'text/plain; charset=utf-8' -Body $body
```

## Exact response, schema 1

All rational numbers are JSON strings, with reduced `numerator/denominator`
notation when the denominator is not one. There are no lossy JSON numbers for
coefficients or root coordinates. Example for `x^2 - 2 = 0` with `precision=16`:

```json
{"ok":true,"schema_version":1,"kind":"finite","degree":2,"count":2,"imaginary":false,"center":"0","radicand":"2","offset":null,"discriminant":"8","roots":[{"branch":"minus","multiplicity":1,"exact":"-sqrt(2)","approximation":"-1.414213562373095"},{"branch":"plus","multiplicity":1,"exact":"sqrt(2)","approximation":"1.414213562373095"}]}
```

| Field | Meaning |
| --- | --- |
| `kind` | `finite`, `none` or `all`; all means every complex number satisfies the constant equation. |
| `degree` | Effective degree after removing zero leading coefficients; zero for none/all. |
| `count` | Distinct roots: zero, one or two. A double root appears once. |
| `center`, `radicand` | Exact finite roots are `center ± sqrt(radicand)`, or `center ± i*sqrt(radicand)` when `imaginary` is true. |
| `offset` | Exact nonnegative rational square root of radicand, or null for an irrational radical. Null does not mean an unsuccessful solve. |
| `discriminant` | Exact `b*b - 4*a*c` for degree two, otherwise null. |
| `roots` | Branch, multiplicity, `exact` expression string and `approximation` decimal string for each root. `minus` precedes `plus`; `single` means center, with multiplicity one or two. |

Linear solutions have zero radicand/offset and one simple root. Repeated
quadratic roots have zero radicand/offset and multiplicity two. NONE/ALL have
null center/radicand/offset/discriminant, false imaginary and an empty roots
array. Negative discriminants produce successful complex solutions, not NONE.
Each root additionally includes `exact` and `approximation`. For example, the
plus root above has `exact: "sqrt(2)"` and at precision 16
`approximation: "1.414213562373095"`. Previews use significant digits from `precision`,
half-even rounding and guard digits; display options do not affect exact roots.
Real previews avoid subtraction cancellation using an exact rational root
product. Preview last digits are not certified; canonical radical simplification
is not promised. All text/projection work shares the request budget.

## Errors and limits

Successful solutions return HTTP 200. Malformed requests, invalid coefficients
and resource failures return HTTP 400 with the standard error envelope:

```json
{"ok":false,"schema_version":1,"status":"invalid argument","error":"invalid argument","code":"coefficient_b_error","column":3}
```

`code` identifies `coefficient_a_error`, `coefficient_b_error` or
`coefficient_c_error` for coefficient evaluation/type errors. Whole-equation
errors use `equation_expression_error`. In coefficient mode, `column` is the
one-based UTF-8 character column within that coefficient's line. Request validation
failures use `equation_request_error`; malformed query parsing may use the shared
`request_error`. The numerical `status` distinguishes syntax, unresolved names,
division by zero, unsupported approximate coefficients and resource exhaustion.
Errors have no partial root response. If memory exhaustion prevents an envelope,
the server retains its small fallback error response.

Parsing/normalizing whole equations or evaluating all coefficients, exact solving and successful serialization
share one five-second limit, cumulative 64 MiB allocation budget and 512 KiB
single-allocation limit. The response buffer is bounded to 128 KiB. Oversized
values/responses fail rather than truncate exact values. Direct C callers may
install a stricter outer scope; nested operations preserve it. HTTP clients
cannot raise the time/memory limits. All finite solutions are complete for the
accepted polynomial; this is separate from approximate graph root candidates.
