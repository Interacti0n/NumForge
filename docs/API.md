# NumForge API overview

This is the short reference for the public C library and the local calculator
HTTP endpoint. Function signatures and all edge-case constraints remain in
the public headers: `include/numforge/bigint.h`,
`include/numforge/bigdecimal.h`, `include/numforge/bigrational.h` and optional
`include/numforge/runtime.h`.

## Public C API scope

The public API consists of these four headers. Existing 1.x numeric signatures
remain source compatible in 2.0, and the new operations are additive.
The calculator implementation and `src/web/web_api.h` are private application
code, not headers for library consumers. `numforge_web` and its loopback HTTP
endpoint are shipped local-tool features; they are not an Internet-facing or
separately versioned remote service.

## Common rules

- `BigInt`, `BigDecimal` and `BigRational` are opaque. Create them with `*_create()` and
  release them with `*_destroy()`; both destroy functions accept `NULL`.
- Mutating functions return a status code. On failure, their output is left
  unchanged unless their public-header comment explicitly says otherwise.
- `bigint_to_string()` returns an owned `char *`; free it with `free()`.
  `bigdecimal_to_string()` writes an owned `char *` through its output
  parameter; free that string with `free()` too.
- Arithmetic functions support output/input aliasing unless documented
  otherwise. `bigint_div_mod()` is the exception: quotient and remainder must
  be different objects.

The strong failure guarantee is exercised by a deterministic test-only
allocator that fails each internal allocation in turn. It is not part of the
public API and is compiled out of non-test builds.

Installed CMake consumers use `find_package(NumForge CONFIG REQUIRED)` and
link `NumForge::numforge`. This contains numeric code and runtime support only.
Parser/evaluator/HTTP code belongs to the non-exported `numforge_client` target.
See [Library guide](LIBRARY_GUIDE.md) for standalone builds and C usage.

## BigInt

Include:

```c
#include <numforge/bigint.h>
```

`BigInt` is a signed arbitrary-precision integer. Decimal input accepts an
optional leading sign and digits only.

| Area | Functions |
| --- | --- |
| Lifecycle and conversion | `bigint_create`, `bigint_destroy`, `bigint_copy`, `bigint_set_string`, `bigint_to_string` |
| Status text | `bigint_status_to_string` |
| Comparison and predicates | `bigint_compare`, `bigint_is_zero`, `bigint_is_one`, `bigint_is_negative`, `bigint_is_even`, `bigint_is_odd` |
| Arithmetic | `bigint_abs`, `bigint_negate`, `bigint_add`, `bigint_sub`, `bigint_mul`, `bigint_div`, `bigint_mod`, `bigint_div_mod`, `bigint_pow` |
| Number theory | `bigint_gcd`, `bigint_lcm`, `bigint_factorial`, `bigint_permutation`, `bigint_combination`, `bigint_isqrt`, `bigint_is_probable_prime`, `bigint_is_perfect_square` |
| Bit operations | `bigint_and`, `bigint_or`, `bigint_xor`, `bigint_not`, `bigint_shift_left`, `bigint_shift_right` |

Division truncates toward zero and the remainder has the dividend's sign.
`bigint_pow()` rejects negative exponents. `BIGINT_FACTORIAL_MAX_N` limits
factorial input to 100000. AND, OR, and XOR accept only non-negative values;
`bigint_not(x)` is defined as `-(x + 1)`. The probable-prime and
perfect-square checks return `BigIntStatus` and write their boolean answer
through an output pointer, so allocation failure cannot be confused with a
valid `false` result.

`bigint_permutation` computes nPr as a falling product and
`bigint_combination` computes nCr with exact division at each step, avoiding
factorial-sized temporaries. Both require `0 <= r <= n`, support aliasing, and
preserve the destination on failure.

`bigint_isqrt` computes floor(sqrt(value)), accepts zero and rejects negative
input with `BIGINT_NEGATIVE_ARGUMENT`. Output/input aliasing is supported.

## BigDecimal

Include:

```c
#include <numforge/bigdecimal.h>
```

`BigDecimal` stores exact base-10 values. `bigdecimal_set_string()` accepts an
optional sign, decimal point, and `e` or `E` exponent; it rejects whitespace
and malformed input. Formatted values use ordinary decimal notation and do not
retain unnecessary trailing zeroes.

| Area | Functions |
| --- | --- |
| Lifecycle and conversion | `bigdecimal_create`, `bigdecimal_destroy`, `bigdecimal_copy`, `bigdecimal_set_string`, `bigdecimal_to_string`, `bigdecimal_from_bigint`, `bigdecimal_to_bigint` |
| Status text | `bigdecimal_status_to_string` |
| Comparison and predicates | `bigdecimal_compare`, `bigdecimal_is_zero`, `bigdecimal_is_negative`, `bigdecimal_is_integer`, `bigdecimal_sign`, `bigdecimal_min`, `bigdecimal_max` |
| Exact arithmetic | `bigdecimal_abs`, `bigdecimal_negate`, `bigdecimal_add`, `bigdecimal_sub`, `bigdecimal_mul`, `bigdecimal_pow` |
| Signed integer powers | `bigdecimal_pow_signed` |
| Sequence aggregates | `bigdecimal_sum`, `bigdecimal_product`, `bigdecimal_mean` |
| Statistics | `bigdecimal_median`, `bigdecimal_geometric_mean`, `bigdecimal_harmonic_mean`, `bigdecimal_variance_population`, `bigdecimal_standard_deviation_population`, `bigdecimal_standard_deviation_sample` |
| Rounded arithmetic | `bigdecimal_rescale`, `bigdecimal_floor`, `bigdecimal_ceil`, `bigdecimal_trunc`, `bigdecimal_round`, `bigdecimal_div`, `bigdecimal_div_significant`, `bigdecimal_div_exact_or_significant` |
| Real roots | `bigdecimal_sqrt`, `bigdecimal_cbrt`, `bigdecimal_root` |
| Exponential and logarithmic | `bigdecimal_exp`, `bigdecimal_ln`, `bigdecimal_log10`, `bigdecimal_log` |
| Trigonometric | `bigdecimal_sin`, `bigdecimal_cos`, `bigdecimal_tan`, `bigdecimal_asin`, `bigdecimal_acos`, `bigdecimal_atan` |
| Hyperbolic | `bigdecimal_sinh`, `bigdecimal_cosh`, `bigdecimal_tanh`, `bigdecimal_asinh`, `bigdecimal_acosh`, `bigdecimal_atanh` |
| Constants and display | `bigdecimal_set_constant`, `bigdecimal_set_constant_significant`, `bigdecimal_format`, `bigdecimal_format_mode` |

Addition, subtraction, and multiplication are exact. Division and rescaling
take an explicit target scale and one of these rounding modes:
`TOWARD_ZERO`, `AWAY_FROM_ZERO`, `FLOOR`, `CEILING`, `HALF_UP`, or
`HALF_EVEN` (each prefixed with `BIGDECIMAL_ROUND_`). A positive target scale
keeps decimal places; a negative scale rounds to tens, hundreds, and so on.

### Additional numeric operations

- `bigdecimal_to_bigint` requires an integer-valued decimal (e.g. `12.00`),
  rejecting fractional values instead of truncating. `from_bigint` is exact.
- `bigdecimal_sign` writes -1/0/1; `is_integer` writes a bool. Neither allocates.
- `bigdecimal_min`/`max` copy a selected operand without rounding; ties select
  the first operand. Aliasing is supported.
- `bigdecimal_floor`, `ceil`, and `trunc` round to an integer in the named
  direction. `bigdecimal_round` uses half-even at a requested decimal-place
  count; negative places select tens, hundreds, and larger powers of ten.
- `bigdecimal_pow` takes a non-negative BigInt exponent and computes exactly;
  `0^0 = 1`. `bigdecimal_pow_signed` additionally accepts negative BigInt
  exponents, preserves terminating reciprocals exactly, and rounds recurring
  reciprocals using its explicit significant-digit count and rounding mode.
  Zero to a negative exponent returns `BIGDECIMAL_DIVISION_BY_ZERO`.
- `bigdecimal_sum` and `bigdecimal_product` aggregate one or more values
  exactly. `bigdecimal_mean` forms the exact sum first, then preserves a
  terminating quotient exactly or rounds a recurring quotient to the requested
  positive significant-digit count. All three accept output/input aliasing.
- Population variance and standard deviation divide by `n` and accept one or
  more values. Sample standard deviation divides by `n−1` and requires at least
  two values. Exact sums and squared sums prevent cancellation from a rounded
  intermediate mean; final recurring division and roots use explicit precision.
- `bigdecimal_median` sorts references without modifying inputs and returns an
  exact middle value or exact average of the two middle values. Geometric mean
  accepts non-negative inputs; harmonic mean requires strictly positive inputs.
  Their final non-exact root or quotient uses explicit precision and rounding.
- `bigdecimal_div_significant` rounds to a positive significant-digit count.
  `bigdecimal_div_exact_or_significant` preserves terminating quotients exactly,
  even beyond that count, and rounds only non-terminating quotients.
- `bigdecimal_root` takes a positive uint32_t degree; sqrt/cbrt are degree 2/3
  wrappers. Negative values require odd degree. Digits is a positive significant
  digit count with explicit rounding. Exact finite roots stay exact; others
  round to that count. Degree 1 is identity. Calculator caps are not embedded
  here; size/scale overflow and exhausted resources still return errors.
- `bigdecimal_exp` computes e^x. `bigdecimal_ln` requires x > 0;
  `bigdecimal_log10` uses base 10; `bigdecimal_log` accepts an explicit base
  greater than zero and different from one. Each takes a positive significant
  digit count and an explicit rounding mode. Results use guarded decimal
  series and argument reduction without binary floating-point conversion.
- Trigonometric library calls always use radians. `sin`, `cos`, `tan`, and
  `atan` accept any finite value; `asin` and `acos` require an argument in
  `[-1, 1]`. Inverse results are radians. Forward reduction calculates enough
  digits of π for both the requested precision and the argument magnitude,
  then evaluates sine and cosine together on `[-π/4, π/4]`. Each call takes a
  positive significant-digit count and an explicit rounding mode.
- `bigdecimal_set_constant` accepts `BIGDECIMAL_CONSTANT_PI`, `_E`, or `_PHI`
  and returns the complete stored 500-decimal-place approximation for backward
  compatibility. `bigdecimal_set_constant_significant` takes a positive
  significant-digit count and an explicit rounding mode. It rounds the stored
  value through 500 digits and calculates larger requests dynamically without
  binary floating point. Both APIs return approximations, not exact irrational
  values, and preserve the destination on failure.
- `bigdecimal_format` takes places >=0 or -1 for all stored digits. Scientific
  notation is used for decimal exponent magnitude >=10, with places applying
  to the mantissa; otherwise places applies to the ordinary decimal part.
  The number is not changed. Caller frees the resulting string with `free()`;
  unlike the client formatter, this API preserves `*result` on failure.
- `bigdecimal_format_mode` adds `BIGDECIMAL_FORMAT_AUTO`, `_PLAIN`,
  `_SCIENTIFIC` and `_MATHEMATICAL`. Auto selects scientific notation when the
  rounded result has exponent >=10 or <=-10, or its plain representation would
  exceed 80 characters. Scientific output uses an explicit exponent sign, such
  as `1.23E+45`; mathematical output uses `1.23 × 10^45`. Zero is always `0`.
  Every mode uses the same rounding result as `bigdecimal_format`; only the
  representation changes. `max_output_bytes` bounds the result excluding its
  terminating NUL. An oversized result returns `BIGDECIMAL_VALUE_TOO_LARGE`
  without changing `*result`. The caller frees a successful string with `free()`.

All follow the common null-argument, aliasing and strong failure contracts.
Pointers must refer to live initialized objects; dangling pointers and concurrent
mutation are not validated. See the public header for complete signatures.

### Optional resource scope

`<numforge/runtime.h>` provides `numforge_budget_begin`, `check`, `failure`,
and `end`. Only a caller for which begin returned true owns/ends the scope;
nested calls reuse it. Scopes are thread-local. Zero duration expires immediately,
UINT64_MAX is practically unbounded, and SIZE_MAX is the largest byte allowance.
Limits count cumulative requested bytes and a single allocation, not live memory.
Ordinary numeric calls start no scope.

Cancellation returns the numeric operation's allocation-failure status. Query
`numforge_budget_failure()` before ending the scope to distinguish TIME/MEMORY.
Cancellation is cooperative, not a hard deadline. Budget-aware
`numforge_malloc/calloc/realloc` include client allocations; free them normally.
`numforge_monotonic_ms` returns monotonic milliseconds or UINT64_MAX on failure.
Independent objects can be used by separate threads; shared mutation requires
caller synchronization. Arithmetic is not constant-time or cryptographically audited.

## BigRational

Include `<numforge/bigrational.h>` and link `NumForge::numforge`. This is an
opaque exact fraction independent of the calculator. `bigrational_create()`
returns zero; `bigrational_destroy(NULL)` is safe. `bigrational_set_fraction`
accepts `BigInt` numerator and denominator and rejects denominator zero. Values
are reduced with a positive denominator and zero is internally `0/1`.
`bigrational_to_string` writes `a/b` or just `a` when the denominator is one;
the caller frees the successful string with `free()`. No fraction text parser is
part of this initial API.

`bigrational_copy`, `add`, `sub`, `mul`, `div`, `negate`, `abs`, and `compare`
are exact. Division by a zero rational returns `BIGRATIONAL_DIVISION_BY_ZERO`.
Every mutating operation supports output/input aliasing and leaves its output
unchanged on failure. `bigrational_get_numerator` and `_get_denominator` copy
into initialized caller-owned `BigInt` objects; they do not expose private
storage. Multiplication and division cancel cross factors before multiplying.

`bigrational_from_bigint` is exact. `bigrational_from_bigdecimal` converts the
*stored finite decimal* exactly: `0.1` becomes `1/10`. It cannot tell whether
that decimal approximated an irrational value. The calculator's typed result
keeps such approximations on the Decimal path instead of converting them back
to a supposedly exact fraction.
`bigrational_to_bigdecimal(result, value, digits, rounding)` requires a positive
significant-digit count and a `BigDecimalRoundingMode`. Terminating quotients
remain exact; recurring quotients are rounded at that precision. A calculator
client passes its current working precision and applies final output rounding
afterwards. The rational API itself has no fixed calculator size or time limit;
callers may use the optional runtime budget.

## Calculator expressions

The calculator is currently an application layer, not a public C header. It
accepts decimal numbers with `.` or `,` as the decimal separator, optional
uppercase-`E` scientific exponent notation, `π`, `e`, and `φ` constants,
whitespace, parentheses, unary `+`/`-`, postfix `²`, `³`, and `!`, explicit or
implicit multiplication, and binary `+`, `-`, `*`, `/`, `^`.

```text
expression  := term (('+' | '-') term)*
term        := unary (('*' | '/' | IMPLICIT_MULTIPLY) unary)*
unary       := ('+' | '-') unary | power
power       := postfix ('^' unary)?
postfix     := primary ('²' | '³' | '!')*
primary     := NUMBER | CONSTANT | '(' expression ')' | call
call        := FUNCTION '(' arguments ')' | '√' '(' expression ')'
arguments   := expression (';' expression)*
CONSTANT    := π | e | φ
```

Examples: `0.1 + 0.2`, `π / 2`, `πe`, `10π`, `2(3 + 4)`,
`-(2.5E-1) * 8`, `(12.5 - 2.5) / 4`, `1.5^3`, `12²`, `2³`, and `5!`. Each
constant is prepared at the calculator's working precision, using the stored
500-place value for ordinary requests and dynamic calculation above it.
Standalone lowercase `e` means
Euler's constant, so `5e`
means `5 * e` and `1e3` means `1 * e * 3`. Scientific notation always uses
uppercase `E`: `5E-1` means `0.5` and `1E3` means `1000`. Powers use binary
exponentiation with exact BigDecimal multiplication, so decimal bases are valid
when the exponent is a whole number. The typed calculator keeps rational bases
and integer exponents exact, including negative exponents; an approximate base
uses a reciprocal at working precision. `2^3^2` means
`2^(3^2)`; `0^0` is `1`, and zero to a negative exponent is a division-by-zero
error. Decimal exponents and variables are not implemented. Integer functions
use the public BigInt core. Exact rational powers and proven roots use BigInt
and BigRational operations; approximate roots use the public BigDecimal core.

The calculator retains exact BigInt or canonical BigRational values for
arithmetic trees made of decimal literals, `ans`, unary signs and `+`, `-`,
`*`, `/`, integer powers and postfix square/cube/factorial. Decimal literals,
including comma input, are converted exactly; `0.1+0.2` is internally `3/10`,
and a denominator of one is stored as BigInt. `sqrt`, `cbrt` and `root` keep a
fraction when both numerator and denominator have proven integer roots, so
`sqrt(4/9)` is internally `2/3`.

Exact typed calls also cover `pow`, `abs`, `sign`, `min`, `max`, `sum`, `product`,
`mean`, `median`, `geomean`, `harmean`, `variance`, `stdevp`, `stdev`,
`floor`, `ceil`, `trunc`, `round`, `factorial`, `isqrt`, `gcd`,
`lcm`, `mod`, `npr` and `ncr` when
their arguments are exact and satisfy the existing domains. Irrational roots,
constants and other functions use the decimal path. Exact subtrees are converted
at the current working precision where that path needs them. Auto output uses
a reduced fraction when its denominator is at most 10000, its text is at most
16 characters including sign and slash, and it is at least two characters
shorter than the rounded decimal display. Fraction mode shows exact non-integers
as `a/b` up to 16 characters; longer fractions and approximate results use Auto
decimal notation. Exact integers have no `/1`. Decimal output alone does not prove
mathematical exactness.

Repeated decimal separators (`1.2.3`, `1,2,3`) and adjacent numeric tokens
(`2 3`, `2 .3`) are errors. Delimited products like `(2)3`, `3!2` and `2²3`
remain valid. Implicit multiplication shares the left-associative precedence
of `*` and `/`, so `6/2(1+2)` is `9`.
Factorial uses `bigint_factorial` and requires a non-negative whole number no
greater than 10000 in the calculator, even though the underlying BigInt API has
a higher limit. Non-terminating division defaults to 34 significant digits with half-even
rounding. The complete CLI/HTTP calculation has a five-second monotonic time
budget, including parsing and output formatting. Expensive BigInt parsing,
multiplication, division and decimal conversion loops check cancellation too.
Exceeding the deadline returns `CALCULATOR_TIME_LIMIT` (`TLE`). This is
cooperative cancellation, not an OS-enforced hard real-time deadline.

Application limits are 64 MiB of cumulative allocation requests per calculation,
128 KiB per allocation, 65536 output bytes, and 10000 selected output places.
Freed allocations still count toward the cumulative work budget; it is not a
measurement of process RSS. Resource limits return `value too large`, not TLE.
These limits do not change unrestricted public BigInt/BigDecimal calls.
An allowed factorial input (including 10000) may still exceed the time or
memory budget; the input limit is not a completion guarantee.

Parser and AST depth are limited to 256 levels, with at most 256 arguments per call. Inputs that exceed the limit
return `CALCULATOR_VALUE_TOO_LARGE` instead of risking process stack overflow.

### Named calls

Names contain lowercase ASCII letters only and require parentheses. Arguments
use semicolons, not commas: `pow(1,5;2)` is `2.25`. The registry recognizes 46
names; recognition is separate from numerical implementation:

| Calls | Current calculation support |
| --- | --- |
| `pow(x;y)`, `factorial(n)` | Active aliases of `x^y` and `n!`, with identical domains and limits. |
| `abs(x)`, `sign(x)`, `min(a;b;…)`, `max(a;b;…)` | Active: absolute value, sign −1/0/1 and minimum/maximum of at least two arguments. |
| `rand()`, `rand(x)`, `rand(x;y)` | Random decimal in `[0,1)`, `[0,x)` for `x > 0`, or `[x,y)` for `x < y`. Each occurrence draws independently. |
| `sum(a;b;…)`, `product(a;b;…)`, `mean(a;b;…)` | One to 256 decimal arguments. Sum and product are exact. Mean divides the exact sum by the count, preserving a terminating decimal or rounding a recurring result to working precision. |
| `median(a;b;…)`, `geomean(a;b;…)`, `harmean(a;b;…)` | Exact median; geometric mean of non-negative values; harmonic mean of positive nonzero values. One to 256 arguments. |
| `variance(a;b;…)`, `stdevp(a;b;…)`, `stdev(a;b;…)` | Population variance and standard deviation use denominator `n` and accept at least one value. Sample `stdev` uses `n−1` and requires at least two values. |
| `floor(x)`, `ceil(x)`, `trunc(x)`, `round(x)`, `round(x;n)` | Active decimal rounding. `round` uses half-even, defaults to zero places, and accepts a signed integer n; negative n selects tens, hundreds, and larger powers. |
| `gcd(a;b)`, `lcm(a;b)` | Integer arguments; non-negative GCD/LCM. `gcd(0;0) = 0`; LCM is zero if either argument is zero. |
| `mod(a;b)` | Integer remainder after division truncating toward zero; nonzero remainder has the dividend's sign. `mod(-7;3) = -1`; zero divisor is an error. |
| `npr(n;r)`, `ncr(n;r)` | Exact permutations and combinations without repetition. Both arguments are integers and require `0 ≤ r ≤ n`; `npr(5;2) = 20`, `ncr(5;2) = 10`. |
| `isqrt(n)` | Floor of the square root of a non-negative integer: `isqrt(15) = 3`. |
| `sqrt(x)`, `cbrt(x)`, `root(x;n)` | Active real roots; `√(x)` aliases `sqrt(x)`. Square roots require x ≥ 0; cube roots accept negative x. `root` accepts integer n from 1 to 10000, and negative x only for odd n. |
| `exp(x)`, `ln(x)`, `log(x)`, `log(x;b)` | Active. `ln` uses base e, one-argument `log` uses base 10, and the second argument selects an arbitrary base. Logarithm inputs must be positive; a custom base must be positive and not 1. |
| `sin(x)`, `cos(x)`, `tan(x)`, `asin(x)`, `acos(x)`, `atan(x)` | Active. They use the selected RAD/DEG calculator mode; inverse results follow the same mode. `asin`/`acos` require x in `[-1,1]`. Exact degree poles such as `tan(90)` are rejected in DEG mode. |
| `sinh(x)`, `cosh(x)`, `tanh(x)`, `asinh(x)`, `acosh(x)`, `atanh(x)` | Active and independent of RAD/DEG. `acosh` requires x ≥ 1; `atanh` requires -1 < x < 1. |
| `radians(x)`, `degrees(x)` | Active explicit conversions, independent of the selected angle mode. |

Each inverse trigonometric or hyperbolic calculator call also accepts both
`arc` and `arcus` prefixes: `asin` → `arcsin`/`arcussin`, `acos` →
`arccos`/`arcuscos`, `atan` → `arctan`/`arcustan`, and likewise
`asinh`/`acosh`/`atanh` → `arcsinh`/`arccosh`/`arctanh` and
`arcussinh`/`arcuscosh`/`arcustanh`. These are parser aliases for the
same calculator operations, arity, domain and angle mode.

Wrong arity returns `wrong number of arguments` at the function name; unknown
names return `invalid token`. Nesting and implicit products work, for
example `pow(2;factorial(3))` and `2pow(2;3)`.

`rand()` draws uniformly from the 34-place decimal grid `0/10^34` through
`(10^34-1)/10^34`. The ranged forms apply exact decimal multiplication and
addition to this draw. They are discrete, not a continuous distribution, and
are not cryptographically secure. The internal value is below the upper bound;
rounding the displayed result to fewer places can show that bound. Output
precision does not change the draw. Each call in one expression advances the
private calculator generator independently, in evaluation order.

Selection calls accept all finite decimal values without introducing rounding;
their arguments follow the normal working-precision policy. Min/max evaluate
every argument left to right and propagate all errors. Ties retain the first
value. Negative zero has sign 0. Named rounding calls deliberately change the
value but do not depend on the calculator's display precision.

Roots first preserve exact finite decimal results, even when they exceed working
precision. Other roots are rounded to max(34, N+4) significant digits with the
context rounding mode (normally half-even), then formatted at the requested
output precision. Full output uses 34 working digits for irrational roots; it
does not mean infinite precision. `sqrt(2)` displays `1.4142135624` by default,
`cbrt(-8)` is exactly `-2`, and `root(32;5)` is exactly `2`. Zero/negative/fractional
degrees are invalid; degrees above 10000 are too large. Time and memory limits
still apply, particularly to high degree/high precision combinations. Like
division, rounded roots can introduce small errors in subsequent arithmetic.

The tokenizer reads complete letter sequences: `exp` is one name, whereas
`1e3` remains `1*e*3`, `πe` remains `π*e`, `e(2)` remains `e*2`, and `1E3`
is 1000. Separate adjacent ASCII names with `*`: `ee` and `esin` are unknown
names, not products. Numeric suffixes such as `log2` are not supported.

### Browser interface

The local browser page has active keypad buttons for this grammar, including
power, square, cube, factorial and an argument separator. Named functions are
organized in six horizontal tabs with one active panel: Basic, Statistics,
Integers, Powers and logarithms, Trigonometry, and Hyperbolic. Function buttons insert
both parentheses and place the caret inside; Enter calculates and moves the
caret to the end of the expression. Root, logarithmic and exponential
and trigonometric controls are active. A RAD/DEG selector on the right beside precision settings
applies to the complete expression and is remembered by
the browser. There is no separate angle indicator beside the expression.
The active mode has a yellow-orange background; the inactive mode is dark.
Switching language or visiting the guide and returning in the same tab keeps
the server session, confirmed `ans`, history, expression, output settings and
recent tools through session storage when available. A normal reload or New
session starts fresh. Server restart or eviction still loses the in-memory value.
Function buttons show mathematical labels where useful and expose signatures
and domain hints on hover, keyboard focus and activation (including touch).
The keypad inserts `.`, while directly typed `,` is accepted as the
same decimal separator. The page is available in Slovak and English and
provides a one-click control to copy the displayed result.
The result panel is five lines high by default. Longer output shows a
`Show all`/`Zobraziť všetko` control; clicking it or the result expands the
panel, and the same control collapses it again.

Results default to 10 decimal places, rounded half-even.
The browser offers Auto (10 places), Full, and Custom; only Custom shows a
numeric field. Full skips final output rounding, not working-precision limits.
Separately, the notation selector offers Auto, plain, scientific,
mathematical and fraction output. Auto first chooses a short exact fraction
under the rule above; otherwise it selects scientific when the rounded exponent
is outside -9..9 or plain output would exceed 80 characters. Scientific uses
`1.23E+45`; mathematical uses `1.23 × 10^45`. The copy button converts
mathematical notation to parser-compatible `E` form when possible. A plain
result over the 65536-byte application output limit returns an error. Language
switching preserves both selectors and the custom precision value.

A caller can request a
non-negative output scale from 0 through 10000,
or `full` to skip the final output rounding. For a numeric scale `N`, non-terminating division
uses `max(34, N + 4)` significant digits. With `full`, division uses its
34-significant-digit half-even policy; `full` cannot make a recurring decimal
exact or recover previously rounded digits. In Auto mode, very large or small
non-zero output uses scientific notation at an absolute exponent of 10 or greater;
its mantissa is rounded to at most the selected number of
decimal places, for example `1.2345678901E-12`.

At extreme internal scales, a formatted exponent can exceed the input parser's
signed 64-bit range. Output is a display representation, not a guaranteed
round-trip serialization format; copying it back may return a range error.

Exact divisions retain a reduced rational intermediate, including recurring
quotients. `(1/3)*3-1` therefore evaluates to exactly `0`. The decimal display
of `1E-40 / 3` is rounded to the current working precision and shows
`3.3333333333E-41` by default. Raising precision can reformat the stored
rational without losing its numerator and denominator. Exceeding resource
limits returns an error rather than a rounded replacement.

Output places are not a guarantee of whole-expression accuracy. Expressions
using irrational roots, constants or other approximate functions keep their
rounded BigDecimal result. `full` uses 34 working digits where approximation is
needed, not unlimited accuracy. There are currently no `inexact` or `rounded`
flags. See the evaluation policy in [CALCULATOR_DESIGN.md](CALCULATOR_DESIGN.md).
No rigorous whole-expression error bound is claimed.
Constants are prepared at max(34, N+4) significant working digits for an N-place
output request. Stored 500-place values cover ordinary requests; larger requests
calculate additional digits dynamically. A forward trigonometric call may
temporarily reevaluate its argument and constants with additional guard digits
based on the argument magnitude, so symbolic π multiples survive angle
reduction. Public `bigdecimal_div` retains
its original explicit decimal-scale policy, independent of this calculator mode.

## Local HTTP API

The calculator layout keeps all controls within the viewport while the result
is collapsed to five lines. Compact spacing and proportional scaling adapt to
short windows. Expanding a long result allows vertical page scrolling; collapsing
it restores the fitted layout. This layout policy does not apply to the API guide.

The web UI shows expression errors beside a short excerpt, marking the reported
token with `⟦…⟧` and including its one-based Unicode character position. An error
after the last character is described as being at the end of the expression.
The marker identifies where the error was detected, not necessarily the only
incorrect character. Resource and connection failures have no expression marker.
The HTTP error fields remain unchanged.

`numforge_web` serves the calculator and exposes one local endpoint:

```text
POST /api/evaluate?precision=10&angle=rad HTTP/1.1
Host: 127.0.0.1:8765
Content-Type: text/plain; charset=utf-8
Content-Length: 6

π / 2
```

A successful response is HTTP 200:

```json
{"ok":true,"result":"1.5707963268"}
```

Invalid expressions, unsupported precision values, and arithmetic errors
return HTTP 400. Calculator errors use this JSON shape:

```json
{"ok":false,"error":"division by zero at column 3","status":"division by zero","column":3}
```

`Content-Length` is required for `POST` requests; omitting it returns HTTP 411.
Browser requests that include `Origin` must come from this server's own
`http://127.0.0.1:8765` or `http://localhost:8765` origin; other origins return
HTTP 403. Native local clients may omit `Origin`. `precision` is optional: it
accepts a non-negative whole number or `full`; if omitted, it defaults to `10`.
`angle` accepts `rad` or `deg` and defaults to `rad`; when supplied it follows
`precision` in the query string. Optional `notation=auto|plain|scientific|math|fraction`
follows `angle` and defaults to `auto`. A successful mathematical response adds
`"copy":"1.23E+45"` (or `"copy":null` if the result cannot fit the parser's
input range or size limit). Other modes retain the existing response fields.
Legacy clients may append `&client=<32 lowercase hex digits>&revision=<N>`
after the normal options. `N` is a positive increasing integer up to
9007199254740991. These parameters opt into a bounded per-client cache;
successful responses then also contain `"cached":true` or `false`.
Without them, the endpoint remains stateless and its response shape is unchanged.
The browser generates a fresh random client ID for each page load, not a cookie
or shared local-storage identity. The server holds at most eight clients with
one successful numeric value each; FIFO eviction frees the old value. Older or
duplicate revisions may compute a response but cannot replace the stored value.
Errors preserve the last successful value. This is not authentication or a
public multiuser service.

The browser adds a final `&action=start|preview|commit` after client/revision.
For example, start a page session with a POST to
`/api/evaluate?precision=10&angle=rad&client=<32 lowercase hex digits>&revision=1&action=start`
and an empty body. Start returns `{"ok":true,"result":""}` and is idempotent
for an existing ID. Then send expressions with action `preview` or `commit` and
increasing revisions. Start does not consume an evaluation revision.

Preview does not change `ans` or history. Commit confirms a successful internal
value and adds history atomically; failed calculations preserve both. `ans` is
initially undefined and always refers to the stored value, not its display.
Increasing precision does not recompute its original expression. An exact
stored `ans`, such as `1/3`, can be displayed at a higher precision; a
previously approximated `ans` retains its original digits. Repeating the latest
successful commit with the same revision, expression and settings returns
the original result; conflicting or older revisions return `stale session request`.
An unresolved confirmation must be retried with its original ID before sending
another confirmation. A new intentional Enter/`=` uses a new revision.

For expressions containing `rand`, a preview keeps the same draws across
automatic repeat requests and precision changes. A successful commit adopts
those draws and advances the session generator once per occurrence. A new
intentional commit makes fresh draws. Failed requests and replay of the same
successful commit do not advance it. Each session has its own generator state;
reloading or resetting starts a new sequence. One-shot and legacy HTTP requests
draw afresh and do not cache random expressions.

The session pool is separate from the legacy cache: eight sessions, FIFO
eviction, 16 confirmed entries each and less than 4 MiB retained history per
session. Each entry stores input, internal value, context and display. Evaluation
of an unknown/evicted session returns `session expired; reload the page` and
never starts another session implicitly. Reload and New session use a fresh
random ID; language and guide navigation in the same tab reuse the ID and
revision through session storage. Server restart loses all sessions. IDs are
not authentication. History buttons restore only input, so expressions with
`ans` use the current answer when evaluated again. CLI `history` lists its
session entries; `reset` clears them and ans, retaining precision, angle and
notation settings.

Output precision and notation are configurable in the UI; working precision remains
automatic. Changing notation only reformats the retained numeric value.
Matching input and working context allow reformatting without another
evaluation. Exact typed integer and rational results, including divisions and
proven roots, can also be reformatted when working precision changes.
Approximate constants, irrational roots and transcendental calls require
recalculation when working precision changes.
Changing RAD/DEG also invalidates reuse. `full` keeps the existing 34-digit working
policy, not infinite precision. More requested digits do not certify accuracy
under cancellation; no rounded/inexact guarantee is inferred from the output.

Out-of-memory calculation failures return HTTP 500 with the same JSON fields.
Malformed HTTP requests return JSON HTTP 400. Oversized bodies return JSON
HTTP 413; a request that does not finish arriving within two seconds returns
JSON HTTP 408. The receive deadline covers the complete headers and body and
is not restarted by each byte. Responses also have a bounded send deadline.
Unknown routes return plain-text HTTP 404; the UI tolerates non-JSON/network
failures and ignores responses superseded by a new calculation or input edit.
The UI distinguishes connection failures from unexpected server responses and
offers retry with Enter. Calculation errors retain their status and source
position, with argument rules for recognized function-domain/arity errors.
These hints do not change the HTTP error schema or numerical API.
Transfer-Encoding is unsupported and rejected; use Content-Length framing.

The local server accepts expressions up to 4096 bytes and listens only on
loopback, using port 8765 by default. `numforge_web --port N` selects another
port from 1 through 65535, and `--no-browser` suppresses automatic browser
launching on Windows. Browser origins must match the selected loopback port.
Error columns are one-based Unicode character positions; the calculator
internals retain zero-based UTF-8 byte offsets so source tokens remain lossless.
The example body above is exactly six UTF-8 bytes and has no trailing newline.
