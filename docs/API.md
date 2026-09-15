# NumForge API overview

This is the short reference for the public C library and the local calculator
HTTP endpoint. Function signatures and all edge-case constraints remain in
the public headers: `include/numforge/bigint.h` and
`include/numforge/bigdecimal.h`.

## Stable 1.x scope

The stable C library API consists of exactly those two public headers.
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

The strong failure guarantee is exercised by a deterministic test-only
allocator that fails each internal allocation in turn. It is not part of the
public API and is compiled out of non-test builds.

Installed CMake consumers use `find_package(NumForge CONFIG REQUIRED)` and
link `NumForge::numforge`. The installed package exposes only the two public
headers described here; calculator and HTTP headers remain internal.

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
`bigint_not(x)` is defined as `-(x + 1)`. The probable-prime and
perfect-square checks return `BigIntStatus` and write their boolean answer
through an output pointer, so allocation failure cannot be confused with a
valid `false` result.

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
primary     := NUMBER | CONSTANT | '(' expression ')' | call
call        := FUNCTION '(' arguments ')' | '√' '(' expression ')'
arguments   := expression (';' expression)*
CONSTANT    := π | e | φ
```

Examples: `0.1 + 0.2`, `π / 2`, `πe`, `10π`, `2(3 + 4)`,
`-(2.5E-1) * 8`, `(12.5 - 2.5) / 4`, `1.5^3`, `12²`, `2³`, and `5!`. Each
constant currently has 200 stored decimal places. Standalone lowercase `e` means
Euler's constant, so `5e`
means `5 * e` and `1e3` means `1 * e * 3`. Scientific notation always uses
uppercase `E`: `5E-1` means `0.5` and `1E3` means `1000`. Powers use binary
exponentiation with exact BigDecimal multiplication, so decimal bases are valid
when the exponent is a non-negative whole number. `2^3^2` means `2^(3^2)`;
`0^0` is `1`. Negative and decimal exponents, modulo, variables, and most named
function calculations are not implemented yet. Squaring and cubing use exact BigDecimal
multiplication too.

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
use semicolons, not commas: `pow(1,5;2)` is `2.25`. The registry recognizes 24
names; recognition is separate from numerical implementation:

| Calls | Current calculation support |
| --- | --- |
| `pow(x;y)`, `factorial(n)` | Active aliases of `x^y` and `n!`, with identical domains and limits. |
| `abs(x)`, `sign(x)`, `min(a;b;…)`, `max(a;b;…)` | Active: absolute value, sign −1/0/1 and minimum/maximum of at least two arguments. |
| `gcd(a;b)`, `lcm(a;b)` | Integer arguments; non-negative GCD/LCM. `gcd(0;0) = 0`; LCM is zero if either argument is zero. |
| `mod(a;b)` | Integer remainder after division truncating toward zero; nonzero remainder has the dividend's sign. `mod(-7;3) = -1`; zero divisor is an error. |
| `isqrt(n)` | Floor of the square root of a non-negative integer: `isqrt(15) = 3`. |
| `sqrt(x)`, `cbrt(x)`, `root(x;n)` | Not implemented; `√(x)` aliases `sqrt(x)`. |
| `exp(x)`, `ln(x)`, `log(x)`, `log(x;b)` | Not implemented. Planned bases: e for ln, 10 for one-argument log, b for two-argument log. |
| `sin(x)`, `cos(x)`, `tan(x)`, `asin(x)`, `acos(x)`, `atan(x)` | Not implemented. Planned angle unit: radians. atan takes only one argument. |
| `radians(x)`, `degrees(x)` | Not implemented; planned degree/radian conversions. |

Wrong arity returns `wrong number of arguments` at the function name; unknown
names return `invalid token`. A well-formed pending call returns `not implemented`
before evaluating its arguments. Nesting and implicit products work, for
example `pow(2;factorial(3))` and `2pow(2;3)`.

Basic calls accept all finite decimal values without introducing rounding;
their arguments follow the normal working-precision policy. Min/max evaluate
every argument left to right and propagate all errors. Ties retain the first
value. Negative zero has sign 0.

The tokenizer reads complete letter sequences: `exp` is one name, whereas
`1e3` remains `1*e*3`, `πe` remains `π*e`, `e(2)` remains `e*2`, and `1E3`
is 1000. Separate adjacent ASCII names with `*`: `ee` and `esin` are unknown
names, not products. Numeric suffixes such as `log2` are not supported.

### Browser interface

The local browser page has active keypad buttons for this grammar, including
power, square, cube, factorial and an argument separator. Named functions are
organized in four collapsible groups. Its root, trigonometric, logarithmic,
and exponential controls remain visibly marked as planned and
disabled. The keypad inserts `.`, while directly typed `,` is accepted as the
same decimal separator. The page is available in Slovak and English and
provides a one-click control to copy the displayed result.
The result panel is five lines high by default. Longer output shows a
`Show all`/`Zobraziť všetko` control; clicking it or the result expands the
panel, and the same control collapses it again.

Results default to 10 decimal places, rounded half-even. A caller can request a
non-negative output scale from 0 through 10000,
or `full` to skip the final output rounding. For a numeric scale `N`, non-terminating division
uses `max(34, N + 4)` significant digits. With `full`, division uses its
34-significant-digit half-even policy; `full` cannot make a recurring decimal
exact or recover previously rounded digits. Very
large or small non-zero output uses scientific notation at an absolute exponent
of 10 or greater; its mantissa is rounded to at most the selected number of
decimal places, for example `1.2345678901E-12`.

At extreme internal scales, a formatted exponent can exceed the input parser's
signed 64-bit range. Output is a display representation, not a guaranteed
round-trip serialization format; copying it back may return a range error.

Terminating division preserves the exact intermediate result within resource
limits, including quotients that terminate after reduction, such as `7/28`.
Exceeding the limits returns an error rather than a rounded replacement.
Non-terminating division rounds to its working significant-digit precision,
independently of the magnitude. `1E-40 / 1` therefore remains `1E-40` both by
default and in `full` mode; `1E-40 / 3` displays `3.3333333333E-41` by default.

Output places are not a guarantee of whole-expression accuracy. At default
precision, `(1/3)*3-1` gives `-1E-34`. However, `(1E34+1)/1-1E34` gives exactly
`1`, since finite decimal quotients no longer lose intermediate digits.
`full` uses 34 working
digits, not unlimited accuracy. There are currently no `inexact` or `rounded`
flags. See the evaluation policy in [CALCULATOR_DESIGN.md](CALCULATOR_DESIGN.md).
Rounding at non-terminating division is still approximate: cancellation can expose its
error, and no rigorous whole-expression error bound or inexact flag is claimed.
Constants contain 200 stored decimal places; selecting a higher output limit
does not add mathematical accuracy to them. Public `bigdecimal_div` retains
its original explicit decimal-scale policy, independent of this calculator mode.

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
Malformed HTTP requests return JSON HTTP 400. Oversized bodies return JSON
HTTP 413; a request that does not finish arriving within two seconds returns
JSON HTTP 408. The receive deadline covers the complete headers and body and
is not restarted by each byte. Responses also have a bounded send deadline.
Unknown routes return plain-text HTTP 404; the UI tolerates non-JSON/network
failures and ignores responses superseded by a new calculation or input edit.
Transfer-Encoding is unsupported and rejected; use Content-Length framing.

The local server accepts expressions up to 4096 bytes and listens only on
loopback, using port 8765 by default. `numforge_web --port N` selects another
port from 1 through 65535, and `--no-browser` suppresses automatic browser
launching on Windows. Browser origins must match the selected loopback port.
Error columns are one-based Unicode character positions; the calculator
internals retain zero-based UTF-8 byte offsets so source tokens remain lossless.
The example body above is exactly six UTF-8 bytes and has no trailing newline.
