# Exact linear and quadratic equations

The [public rational coefficient solver](../reference/POLYNOMIAL_API.md) owns
degree reduction and exact radical/complex descriptors. Application parsing,
preview formatting and HTTP adapters in `src/application/equations/` stay private.
[Browser guide](../guides/EQUATIONS.md) · [HTTP contract](../reference/EQUATION_HTTP_API.md).

## C operations and ownership

`application_equation_solve` borrows a/b/c (which may alias), validates context
and returns an owned solution. Initialize output pointers; success publishes a
new object without destroying an existing one. Failure preserves inputs/output;
destroy(NULL) is safe. Typed-call error offsets are zero.

`application_equation_root_exact` materializes owned rational-complex components;
irrational radicals return NOT_IMPLEMENTED while their exact descriptors remain
available. Invalid indices/NONE/ALL return INVALID_ARGUMENT. Release each value
with its owning destroy function.

## Degree reduction and exact representation

`application_equation_solve_text` parses one equation in lowercase x, extracts
three rational coefficients from each side and subtracts them. Input ≤4096 bytes,
no line breaks. It preserves full-input byte offsets and retains no AST.

Accept +/−/*, division by x-free constants, powers 0..2 and postfix square.
Every intermediate degree stays ≤2; x-dependent calls/denominators/exponents
are rejected, including `x/x`, to preserve domain holes. Inspect the AST for
random/unresolved names even inside terms multiplied by zero; poll inherited
deadlines during repeated walks. No calculator variables/ans are available.

`application_equation_solve_expressions` evaluates a/b/c in order using exact
real constants, rejecting approximate/complex/quantity values and assignment/
randomness. `failed_coefficient` is 0..2 or SIZE_MAX; its byte offset is local
to that field. All evaluations/solving share a budget.

Root count, ordering, multiplicity, center, discriminant and radical meanings
follow the [numeric contract](../reference/POLYNOMIAL_API.md). A rational made
from an approximation denotes that supplied polynomial, not its original value.

## Resource limits and verification

`application_equation_format_root` publishes owned exact/approximate text;
`application_equation_root_text_destroy` releases it. Rational components use
the shared complex formatter; radicals stay exact without square-factor extraction.
Previews use `division_scale` significant digits, half-even and 24 guards,
independently of display notation/angle. They are not certified intervals.

For real irrational roots, form the same-sign root by adding magnitudes, then
recover its partner from `(center^2-radicand)/stable_root`, avoiding cancellation.
Complex components project separately.

Nested calls inherit scopes; separate solve/materialize calls have separate
budgets unless wrapped. HTTP includes coefficient evaluation, solving, preview
and serialization inside one ten-second/128-MiB cumulative/8-MiB allocation
scope. No partial output/session mutation is published on failure.
`equation_tests` covers exact families, aliases, large cancellation and injected
failures; [Testing](../TESTING.md) owns the wider procedure.

## Next integration

Exact browser/HTTP integration and checked numerical interval/scanning adapters
are implemented. Public kernels remain parser-independent. Production retains
bisection; Brent/hybrid research is private. Local domain enclosures and final
higher-precision reevaluation reject unverified approximate candidates.
[Numerical contract](../reference/NUMERIC_ROOT_HTTP_API.md) ·
[Interpolation decision](../performance/baselines/ROOT_EXPERIMENTS.md).
