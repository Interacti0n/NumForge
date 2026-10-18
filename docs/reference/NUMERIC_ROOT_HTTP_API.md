# Numerical interval root

`POST /api/equations/solve?mode=numeric&min=0&max=1&precision=34` accepts
one equation line, e.g. `cos(x)=x`, and searches for **one real root**. This
stateless private C operation is not an installed numeric SDK API and does not
enumerate roots or prove continuity.

## Request and work limits

- Use lowercase unknown x, exactly one `=`, radians and calculator functions.
  Approximate real functions/constants are supported. Saved variables, ans,
  assignments and random calls are unavailable.
- Required min/max are finite decimal literals, including scientific notation,
  each at most 47 ASCII characters, with min < max. No bound expressions.
- Precision is 16–100 significant digits, default 34; evaluation adds 16 guard
  digits. Only mode/min/max/precision are accepted; duplicate/unknown options fail.
- The body is at most 4096 UTF-8 bytes, without CR/LF or NUL. Existing framing
  limits apply: 8192 bytes including headers and a two-second receive deadline.
- Preparation, solving and serialization share five cooperative seconds, 64 MiB
  cumulative allocations and 512 KiB single allocations. Response limit: 128 KiB.
- At most 96 iterations and 192 evaluated coordinates. Each coordinate evaluates
  both original sides, not just one primitive math operation.

The browser's Numerical interval root mode defaults to 0..10. Polynomial and
coefficient modes retain their existing exact semantics and precision ranges.

## C method and acceptance

`application_numeric_root_solve` in `src/application/numeric_root.h` returns an
owned `ApplicationNumericRoot`, released by `application_numeric_root_destroy`.
Bounds/context are borrowed, failures preserve the destination, and nested calls
inherit an outer budget without resetting it. Both expressions are prepared once
through the shared C evaluator. Coordinates and bisection use BigRational;
values/residuals use BigDecimal. Exact integer/rational sides are subtracted
rationally before projection. No double conversion is used.

Exactly zero typed rational samples at endpoints or midpoints can be accepted
immediately. Otherwise opposite nonzero endpoint signs are required. Approximate
endpoint zeros alone do not establish a bracket. Sampled undefined, Quantity or
non-real values stop the search with a domain diagnostic.

For other candidates, absolute interval width and absolute residual must both
be <=1E-12, and residual must improve strictly on both original endpoints.
This rejects tested tiny constant jumps and scaled poles that would pass an
absolute residual check alone. Approximately zero midpoints also need resolvable
opposite-sign quarter samples. More requested precision does not tighten these
fixed tolerances or raise work limits.

These checks assume a continuous bracket; they are not certified existence or
error bounds. Hidden poles/jumps can lie between samples. Even-multiplicity
roots without a sign change can be missed. Extremely scaled/ill-conditioned
functions and very wide intervals may fail to converge. Exact-zero acceptance
establishes only the typed sampled equation value, not uniqueness/completeness.

The [numerical audit](../benchmarks/NUMERIC_ROOT_AUDIT_2026_10_18.md) records a
concrete unresolved case: a sufficiently small jump can return found despite
having no actual root. Width, residual and endpoint improvement do not establish
continuity. Conservative domain verification remains a prerequisite for stronger
existence claims.

## Response and errors

HTTP 200 numerical results have ok:true, schema_version:1, kind:"numeric" and
**complete:false**, including nonconvergence:

| Field | Meaning |
| --- | --- |
| outcome | found, not_bracketed, domain_gap, iteration_limit or precision_limit. |
| count, roots | One root when found; otherwise zero and an empty array. |
| lower, upper | Final rational search bounds as strings, not certified enclosures. |
| iterations, evaluations | Refinement steps and evaluated coordinate pairs. |
| x_tolerance, residual_tolerance | Both "1E-12"; exact-zero samples are the exception. |
| roots[0].x | Full decimal coordinate checked by C, as a string. |
| roots[0].approximation | Rounded display coordinate, as a string. |
| roots[0].residual | Absolute original-equation difference at full x, as a string. |
| roots[0].exact_zero | Whether the typed rational difference at the sample is zero. |

Copy point copies full x. The preview may round to a different coordinate;
residuals apply to full x. There is no exact symbolic-root/multiplicity field or
lossy JSON numeric coordinate. A non-found outcome never means “no roots”.

Malformed options return HTTP 400/numeric_options_error. Syntax, unresolved-name,
preparation and resource failures use numeric_equation_error and standard status
and UTF-8 character columns. Malformed query parsing may use request_error.
Numerical nonconvergence is separate from structural/resource failure; no partial
root is published. Existing low-memory server fallback errors remain available.

Multiple-bracket scanning, tangencies, configurable tolerances and graph
integration remain future work. See the [equation guide](../guides/EQUATIONS.md).
