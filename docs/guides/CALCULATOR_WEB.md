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
