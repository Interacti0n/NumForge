# Graph sampling HTTP API

The local server supports a bounded uniform sampler through
`POST /api/graph/sample`. This reference describes that stable uniform-grid
operation. The [browser graph page](../guides/GRAPHS.md) uses the separate
[screen sampler](SCREEN_GRAPH_HTTP_API.md), with the
[adaptive precision sampler](ADAPTIVE_GRAPH_HTTP_API.md) as fallback.
An installed public SDK expression evaluator remains future work.

```sh
curl -H 'Content-Type: text/plain' \
  --data 'x*x' \
  'http://127.0.0.1:8765/api/graph/sample?min=-1&max=1&points=3'
```

The body is one expression, up to 4096 UTF-8 bytes. It is parsed once. Assignments
and random calls/aliases are rejected. Other registered calculator functions
retain their existing domains and precision behavior.

## Strict query schema

| Field | Contract |
| --- | --- |
| `min`, `max` | Required finite decimal literals, including decimal scientific notation, with `min < max`. No expressions or fraction syntax. Each decoded query value is at most 47 ASCII bytes. |
| `points` | Integer from 2 to 1024, default 129. Includes both endpoints. |
| `parameter` | Local parameter, default `x`; follows session variable naming rules (1–31 ASCII letters, case-sensitive, reserved names rejected). Shadows the same session name. |
| `precision` | Calculation precision from 1 to 10000, default 34. Low precision also limits decimal plotting projections. |
| `angle` | `rad` (default) or `deg`. Existing real/complex angle rules apply. |
| `client` | Optional existing 32-character lowercase hexadecimal session ID. Omit for a standalone expression. Missing/released sessions fail; reads never create or revive a session. |
| `at` | Optional decimal session revision, allowed only with `client`. A mismatch returns HTTP 409. |

Duplicate/unknown fields, empty values, invalid encodings, GET requests,
`action`/`revision` mutations and unsupported values are rejected. Use normal
URL encoding, including `%2B` for a literal plus in a scientific exponent.

## Response schema 1

Success is HTTP 200 with `ok`, `schema_version`, `parameter`, `count`,
`plot_places: 12` and `points`. Each point contains:

- `x`: a decimal scientific string for the plotting coordinate.
- `kind`: `real`, `domain_error`, `complex` or `quantity`.
- `y`: a decimal scientific string only for `real`; otherwise `null`.
- `source_approximate`: whether the evaluated numeric value was an approximation
  before plotting/serialization. It does not assert that a transmitted string
  is exact: exact fractions also need rounded plotting projections.
- A `domain_error` additionally has `status` and a one-based expression `column`.

Scientific strings use half-even rounding and at most twelve places after the
mantissa's decimal point. Calculation precision may already limit their digits.
They are strings to avoid implicit overflow/underflow or loss of large values
in JSON number parsing. A browser must check its own finite numeric range before
plotting; it must not turn a complex result into a real coordinate implicitly.

Internally, the C sampler retains exact rational coordinates
`min + (max-min)*i/(points-1)` and typed results. Only the HTTP plotting projections
are rounded. Complex-typed results (including zero imaginary components) and
quantities are explicitly classified rather than silently projected.

Division by zero, invalid numeric domains, dimension errors and unsupported
functions mark a point as a gap and allow later points to run. Resource/scale/
size failures abort the whole request; no partial point array is returned.
Handled failures normally return HTTP 400 JSON with `ok:false`, `status`,
`error`, `code` and `column`; stale snapshots use 409. Transport validation may
return its existing framing/origin errors first.

## Isolation and limits

Preparation copies only referenced session variables and `ans`. Sampling does
not modify variables, ans, histories, previews/cache, random draws or revisions.
The source session remains intact after successful or failed requests.

One five-second budget covers range conversion, expression preparation,
coordinate generation, every point and serialization. Cumulative allocation is
64 MiB, a single allocation is at most 512 KiB, and the successful JSON response
must fit the 128 KiB adapter buffer including its terminating null byte. Dense
or expensive requests can fail within these bounds; 1024 is a count limit, not a
completion guarantee. The lower-level C sampler inherits an outer caller budget.

POST retains the server's existing exact Origin policy, Content-Length
requirement and embedded-NUL rejection. A public tunnel still needs its exact
configured origin. This route adds no authentication or persistence.

A uniform grid cannot prove continuity or discover every pole. Consumers must
respect point kinds and finite numeric projection limits. The browser uses
adaptive connection flags and screen refinement instead of joining this uniform
grid blindly; finite adaptive sampling still does not prove continuity.

See the [parametric application design](../design/PARAMETRIC_EXPRESSIONS.md)
and [general HTTP protections](HTTP_API.md).

See the [adaptive graph API](ADAPTIVE_GRAPH_HTTP_API.md) for the automatic web plotter.
