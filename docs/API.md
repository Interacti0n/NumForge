# NumForge API overview

This is the short reference for the public C library and the local calculator
HTTP endpoint. Function signatures and all edge-case constraints remain in
the public headers: `include/numforge/bigint.h` and
`include/numforge/bigdecimal.h`.

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
accepts decimal numbers, optional scientific exponent notation, whitespace,
parentheses, unary `+`/`-`, and binary `+`, `-`, `*`, `/`.

```text
expression  := term (('+' | '-') term)*
term        := unary (('*' | '/') unary)*
unary       := ('+' | '-') unary | primary
primary     := NUMBER | '(' expression ')'
```

Examples: `0.1 + 0.2`, `-(2.5e-1) * 8`, and `(12.5 - 2.5) / 4`.
Exponentiation, modulo, variables, functions, constants, and implicit
multiplication are not implemented yet. The default division policy produces
34 decimal places and uses half-even rounding.

## Local HTTP API

`numforge_web` serves the calculator and exposes one local endpoint:

```text
POST /api/evaluate
Content-Type: text/plain; charset=utf-8

0.1 + 0.2
```

A successful response is HTTP 200:

```json
{"ok":true,"result":"0.3"}
```

Invalid expressions and arithmetic errors return HTTP 400:

```json
{"ok":false,"error":"division by zero at column 3"}
```

The local server accepts expressions up to 4096 bytes and listens only on
`127.0.0.1:8765`.
