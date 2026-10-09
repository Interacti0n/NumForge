# Browser calculator guide

## Controls and behavior

The shared header links the calculator, unit converter and guide and previews
future graph and equation areas. Sign-in and registration
currently show informational pages; there is no authentication or payment API.

The local browser page has active keypad buttons for this grammar, including
power, square, cube, factorial and an argument separator. Named functions are
organized in six categories with one active panel: Basic, Statistics,
Integers, Powers & logs, Trigonometry, and Hyperbolic. Function buttons insert
both parentheses and place the caret inside; Enter calculates and moves the
caret to the end of the expression. Root, logarithmic and exponential
and trigonometric controls are active. A RAD/DEG selector on the right beside precision settings
applies to the complete expression and is remembered by
the browser. There is no separate angle indicator beside the expression.
The active mode is highlighted in the shared purple palette; the inactive mode is dark.
Switching language or visiting the guide and returning in the same tab keeps
the server session, confirmed `ans`, history, expression, output settings and
recent tools through session storage when available. A normal reload or New
session starts fresh. Server restart or eviction still loses the in-memory value.
Web history numbers successful confirmations in order, retains the last 16
entries, and keeps the sequence when navigating within the same tab.
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
On phones, the expression fills the row; Calculate, Clear and the library
shortcut share one row below it. Input requests a text virtual keyboard so
variables and separators such as semicolons remain available. Automatic
capitalization and correction are disabled for expressions;
the operating system and browser determine its layout and whether autofocus opens it.
On phones, Calculate or Enter dismisses the input keyboard and reveals the result;
live previews do not move the page.
The on-screen keypad is hidden. The library includes a
searchable Constants category with pi, Euler's number and the golden ratio.
Function search also accepts common English and Slovak names and abbreviations
(for example avg/average/priemer, NSD/GCD, tg/tangent and deg2rad).
These are library search terms: clicking a result inserts the supported function name.
In landscape on phones, Session and Settings share two equal-width columns,
with Session on the left and Settings always expanded on the right.
In portrait, Settings sit below history and variables. Precision, notation and
angle controls are always visible in every layout; the panel cannot be collapsed.
Separately, the notation selector offers Auto, plain, scientific,
mathematical and fraction output. Auto first chooses a short exact fraction
under the rule above; otherwise it selects scientific when the rounded exponent
is outside -9..9 or plain output would exceed 80 characters. Scientific uses
`1.23E+45`; mathematical uses `1.23 × 10^45`. The copy button converts
mathematical notation to parser-compatible `E` form when possible. A plain
result over the 65536-byte application output limit returns an error. Language
switching preserves both selectors and the custom precision value.
When the primary result is a fraction, a smaller `≈` line shows a decimal
hint at up to 10 places, independently of the selected output precision.

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
flags. See the evaluation policy in [CALCULATOR_DESIGN.md](../design/CALCULATOR_DESIGN.md).
No rigorous whole-expression error bound is claimed.
Constants are prepared at max(34, N+4) significant working digits for an N-place
output request. Stored 500-place values cover ordinary requests; larger requests
calculate additional digits dynamically. A forward trigonometric call may
temporarily reevaluate its argument and constants with additional guard digits
based on the argument magnitude, so symbolic π multiples survive angle
reduction. Public `bigdecimal_div` retains
its original explicit decimal-scale policy, independent of this calculator mode.

## Application limits

| Resource | Limit |
| --- | --- |
| Factorial input | Integer 0–100000 |
| Root degree | Integer 1–10000 |
| Output decimal places | 0–10000; default 10 |
| Calculation time | 5 s, including parsing and output |
| Allocation budget | 64 MiB cumulative requests; 512 KiB per allocation |
| Expression / output | 4096 / 65536 UTF-8 bytes |
| Parser depth / arguments per call | 256 / 256 |
| Variables | 32 per session; names up to 31 ASCII letters |
| Calculator / conversion history | 16 / 16 confirmed entries per session |
| Server sessions | 8, with FIFO eviction; restart clears them |
| HTTP request / receive time | 8192 bytes total / 2 s |
| Session API response / page | 128 KiB JSON; 1–32 entries, default 8 |

Time cancellation is cooperative. Freed allocations still count toward the
64-MiB cumulative allocation budget; this is not process RSS. Allowed inputs
are not a completion guarantee: `100000!` has 456574 digits, exceeds the full
plain output bound and may exceed the five-second calculation budget even
with scientific notation. Full precision does not disable resource limits.

These are application limits, not implicit budgets of the public numeric C API.
See [expression rules](../reference/CALCULATOR_EXPRESSIONS.md),
[HTTP transport](../reference/HTTP_API.md) and
[session API](../reference/SESSION_HTTP_API.md) for domains, errors and pagination.

## Automatic complex results

`sum`, `product` and `mean` also work with complex arguments. For example,
`mean(1/3;i;2/3)` retains exact rational components: `1/3 + (1/3)*i`.
Approximate inputs make the result approximate; other statistics remain
real-only. These functions use the shared C evaluator through the same API.

Dimensionless inputs no longer require +0i to extend supported real domains.
Negative logarithms, asin/acos outside [-1,1], acosh below 1, atanh outside
[-1,1], negative even roots and fractional powers of negative bases select
principal complex results. Valid real results remain real; real odd roots
remain real. Complex outputs use radians even in DEG. Poles and integer-only
or ordering/statistical constraints remain errors. See
[domain and branch rules](../reference/CALCULATOR_EXPRESSIONS.md#automatic-complex-domains).
