# NumForge API overview

This is the short reference for the public C library and the local calculator
HTTP endpoint. Function signatures and all edge-case constraints remain in
the public headers: `include/numforge/bigint.h` and
`include/numforge/bigdecimal.h`.

## Intended 1.0 stability scope

The intended stable C library API consists of exactly those two public headers.
The calculator implementation and `src/web/web_api.h` are private application
code, not headers for library consumers. `numforge_web` and its loopback HTTP
endpoint are shipped local-tool features; they are not an Internet-facing or
separately versioned remote service.

## Common rules

- `BigInt` and `BigDecimal` are opaque. Create them with `*_create()` and
  release them with `*_destroy()`; both destroy functions accept `NULL`.
- Mutating functions return a status code. On failure, their output is left
  unchanged unless their public-header comment explicitly says otherwise.
- `bigint_to_string()` returns an owned `char *`; free it with `free()`.
  `bigdecimal_to_string()` writes an owned `char *` through its output
  parameter; free that string with `free()` too.
- Arithmetic functions support output/input aliasing unless documented
  otherwise. `bigint_div_mod()` is the exception: quotient and remainder must
  be different objects.

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
| Number theory | `bigint_gcd`, `bigint_lcm`, `bigint_factorial`, `bigint_is_probable_prime`, `bigint_is_perfect_square` |
| Bit operations | `bigint_and`, `bigint_or`, `bigint_xor`, `bigint_not`, `bigint_shift_left`, `bigint_shift_right` |

Division truncates toward zero and the remainder has the dividend's sign.
`bigint_pow()` rejects negative exponents. `BIGINT_FACTORIAL_MAX_N` limits
factorial input to 100000. AND, OR, and XOR accept only non-negative values;
`bigint_not(x)` is defined as `-(x + 1)`.

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
| Lifecycle and conversion | `bigdecimal_create`, `bigdecimal_destroy`, `bigdecimal_copy`, `bigdecimal_set_string`, `bigdecimal_to_string` |
| Status text | `bigdecimal_status_to_string` |
| Comparison and predicates | `bigdecimal_compare`, `bigdecimal_is_zero`, `bigdecimal_is_negative` |
| Exact arithmetic | `bigdecimal_abs`, `bigdecimal_negate`, `bigdecimal_add`, `bigdecimal_sub`, `bigdecimal_mul` |
| Rounded arithmetic | `bigdecimal_rescale`, `bigdecimal_div` |

Addition, subtraction, and multiplication are exact. Division and rescaling
take an explicit target scale and one of these rounding modes:
`TOWARD_ZERO`, `AWAY_FROM_ZERO`, `FLOOR`, `CEILING`, `HALF_UP`, or
`HALF_EVEN` (each prefixed with `BIGDECIMAL_ROUND_`). A positive target scale
keeps decimal places; a negative scale rounds to tens, hundreds, and so on.

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
primary     := NUMBER | CONSTANT | '(' expression ')'
CONSTANT    := π | e | φ
```

Examples: `0.1 + 0.2`, `π / 2`, `πe`, `10π`, `2(3 + 4)`,
`-(2.5E-1) * 8`, `(12.5 - 2.5) / 4`, `1.5^3`, `12²`, `2³`, and `5!`. Each
constant currently has 200 stored decimal places. Lowercase `e` always means
Euler's constant, so `5e`
means `5 * e` and `1e3` means `1 * e * 3`. Scientific notation always uses
uppercase `E`: `5E-1` means `0.5` and `1E3` means `1000`. Powers use binary
exponentiation with exact BigDecimal multiplication, so decimal bases are valid
when the exponent is a non-negative whole number. `2^3^2` means `2^(3^2)`;
`0^0` is `1`. Negative and decimal exponents, modulo, variables, and general
functions are not implemented yet. Squaring and cubing use exact BigDecimal
multiplication too.
Factorial uses `bigint_factorial` and requires a non-negative whole number no
greater than 5000 in the calculator, even though the underlying BigInt API has
a higher limit. The default division policy produces 34 decimal places and uses
half-even rounding. Evaluation has an approximately five-second CPU limit; it
returns `CALCULATOR_TIME_LIMIT` and the local web API reports `TLE` when the
limit is reached. A single already-running large BigInt operation cannot be
interrupted mid-operation, so it may finish shortly after the limit.

Parser and AST depth are limited to 256 levels. Inputs that exceed the limit
return `CALCULATOR_VALUE_TOO_LARGE` instead of risking process stack overflow.

The local browser page has active keypad buttons for this grammar, including
power, square, cube, and factorial. Its root, trigonometric, logarithmic,
exponential, and absolute-value controls remain visibly marked as planned and
disabled. The keypad inserts `.`, while directly typed `,` is accepted as the
same decimal separator. The page is available in Slovak and English and
provides a one-click control to copy the displayed result.

Results default to 10 decimal places, rounded half-even. A caller can request a
non-negative output scale up to `INT64_MAX - 4` (subject to available memory),
or `full` to skip the final output rounding. For a numeric scale `N`, division
uses `max(34, N + 4)` decimal places. With `full`, division still uses its
34-place half-even policy; `full` cannot make a recurring decimal exact. Very
large or small non-zero output uses scientific notation at an absolute exponent
of 10 or greater; its mantissa is rounded to at most the selected number of
decimal places, for example `1.2345678901E-12`.

## Local HTTP API

`numforge_web` serves the calculator and exposes one local endpoint:

```text
POST /api/evaluate?precision=10 HTTP/1.1
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
Out-of-memory calculation failures return HTTP 500 with the same JSON fields.
Malformed HTTP requests and unknown routes use plain-text HTTP 400 and 404
responses respectively.

The local server accepts expressions up to 4096 bytes and listens only on
`127.0.0.1:8765`. Error columns are one-based Unicode character positions; the
calculator internals retain zero-based UTF-8 byte offsets so source tokens
remain lossless. The example body above is exactly six UTF-8 bytes and has no
trailing newline.
