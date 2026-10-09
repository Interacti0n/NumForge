# Public C API

This reference describes the public C numeric library. Function signatures and
all edge-case constraints remain in
the public headers: `include/numforge/bigint.h`,
`include/numforge/bigdecimal.h`, `include/numforge/bigrational.h`,
`include/numforge/bigcomplex.h`, `include/numforge/bigrationalcomplex.h`,
`include/numforge/units.h` and optional
`include/numforge/runtime.h`.

## Public C API scope

The public API consists of these seven headers. Existing 1.x numeric signatures
remain source compatible in 2.x, and the new operations are additive.
The calculator implementation and `src/web/web_api.h` are private application
code, not headers for library consumers. `numforge_web` and its loopback HTTP
endpoint are shipped local-tool features; they are not an Internet-facing or
separately versioned remote service.

## Common rules

- `BigInt`, `BigDecimal`, `BigRational`, `BigComplex` and `BigRationalComplex` are opaque. Create them
  with `*_create()` and release them with `*_destroy()`; destruction accepts `NULL`.
- [BigComplex](BIGCOMPLEX.md) provides standalone finite-decimal complex
  arithmetic, exact rational complex arithmetic, complex exponential,
  component extraction, conjugation, squared modulus, principal complex square
  root through `bigcomplex_sqrt`, principal natural logarithm through
  `bigcomplex_ln`, principal `bigcomplex_log` / `bigcomplex_pow` and three display forms.
  Private calculator/session/HTTP adapters support typed complex values; those
  adapters remain outside the public numeric C library ABI.
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
The non-exported `numforge_calculator` target owns expression evaluation,
`numforge_application` owns sessions/client storage, and `numforge_client` owns
HTTP adapters. See [Application design](../design/APPLICATION_DESIGN.md). Their private
headers are not a public C ABI.
See [Library guide](../guides/LIBRARY_GUIDE.md) for standalone builds and C usage.

## BigInt

Include:

```c
#include <numforge/bigint.h>
```

`BigInt` is a signed arbitrary-precision integer. Decimal input accepts an
optional leading sign and digits only.

| Area | Functions |
| --- | --- |
| Lifecycle and conversion | `bigint_create`, `bigint_destroy`, `bigint_copy`, `bigint_set_string`, `bigint_to_string`, `bigint_set_string_base`, `bigint_to_string_base` |
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

### Numeral bases 2–36

`bigint_set_string_base(value, "FF", 16)` reads 255;
`bigint_to_string_base(value, 2, false, &text)` returns owned `"11111111"`.
Use `free(text)` after success. Both functions return `BigIntStatus`; the value
and output pointer remain unchanged on failure. A successful formatter does not
free the previous pointer, so manage its ownership before reusing it.

The base is explicit, from 2 through 36. Input accepts ASCII `0–9`, `A–Z` and
`a–z` case-insensitively, an optional sign and leading zeros. It rejects invalid
base/digits, whitespace and separators. No prefix is recognized: `0xFF` is invalid
in base 16, but `x` is an ordinary digit if valid in the chosen base. Output has
no prefix or leading zeros, uses `-` for negatives and `"0"` for zero. The
`uppercase` argument selects uppercase or lowercase letters. These are integer
library APIs; the calculator grammar still uses decimal numeric literals.

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
