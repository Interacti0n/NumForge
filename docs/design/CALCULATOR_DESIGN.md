# Calculator design

The calculators combine application sessions with the shared typed expression
engine. [Architecture](ARCHITECTURE.md) owns the source map;
[expression syntax](../reference/CALCULATOR_EXPRESSIONS.md),
[parser design](EXPRESSION_PARSER.md) and
[session ownership](APPLICATION_DESIGN.md) have separate references.

## Modules

| Engine area | Responsibility |
| --- | --- |
| `api/calculator.c`, `api/status.c` | Complete compute pipeline, validation, errors and legacy status policies |
| `api/engine.c` | Public stateless context, results and prepared-expression handles |
| `parser/` | Tokens, owned AST and source locations |
| `evaluator/exact_*` | Exact rational branches and proof-based roots |
| `evaluator/evaluator*`, `complex_evaluator.c` | Approximate/complex dispatch, argument validation and domain promotion |
| `evaluator/functions.c`, `constants.c`, `random.c` | Registry, constant adapter and explicit random state |
| `values/` | Typed numeric/quantity ownership and exact-to-decimal projection |
| `formatting/` | Output projection without replacing stored values |

```text
source -> tokenizer -> parser/AST -> typed evaluator -> owned value -> display
```

CLI and HTTP own transport and presentation. They do not implement arithmetic.
The numeric target remains independently usable; `NumForge::engine` adds
stateless evaluation without exposing calculator storage or application sessions.

## Evaluation policy and errors

Exact subtrees retain BigInt/BigRational values, including recurring fractions.
Approximate branches materialize them at working precision only when needed.
The context separates working significant digits from final output places.
Full output skips final rescaling; it does not undo previous approximation.
User-visible precision, notation and application limits are defined in the
[expression reference](../reference/CALCULATOR_EXPRESSIONS.md).

`significant_division` defaults to true in the private calculator context.
Explicitly false retains the legacy fixed-scale path; this does not change
public BigDecimal fixed-scale division. The significant path derives a
coefficient-ratio exponent and uses checked scale arithmetic, avoiding enormous
powers of ten just to retain relative precision at compact extreme magnitudes.

Constants use an evaluation-local cache keyed by the requested precision.
Recalculation replaces a cache entry when that precision changes, so an earlier
low-precision operand cannot reduce a later operand's precision.

One complete calculation shares a cooperative runtime scope across parsing,
evaluation, formatting and cleanup. Nested calls inherit it. Native numeric calls
outside a scope remain unrestricted. Resource exhaustion and elapsed time keep
their established, distinct calculator status mappings. Ordinary allocation
failure remains out-of-memory; no thread is forcibly terminated.

`CalculatorError` offsets are zero-based UTF-8 bytes. Operators/functions retain
their own error locations. CLI/HTTP presentation converts these to one-based
Unicode character columns. Transport input-size bounds are separate from parser
and AST depth bounds.

## Current implementation status

The shared pipeline is implemented. Exact/approximate/complex type tags are
preserved through results and formatting; they are not a certificate of a whole
expression's error bound. The [roadmap](../project/ROADMAP.md) owns future work.

## Confirmed session variables

Application sessions own named snapshots. Assignment is handled as a top-level
statement; the parser owns names and evaluation borrows resolved values.
Approximate stored values are not recomputed from their original expressions
when output precision changes. Exact values can be rematerialized for display.

Confirmation prepares the result, history display and variable copy before
changing confirmed state. Failure preserves values, history, ans and RNG state;
request revision handling follows the [session protocol](../reference/SESSION_HTTP_API.md).
Session-sensitive expressions cannot reuse an unrelated cached history value.
Reset/destruction releases owned values. See [browser/result design](WEB_DESIGN.md)
for preview, retry and random-state handling.
