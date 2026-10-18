# Browser polynomial equations

Open **Equations / Rovnice** (`/solve?lang=en` or `/solve?lang=sk`). Enter a
whole equation, then choose **Solve / Vyriešiť** or press Enter. For example:

- `3x + 1 = 7` gives `x = 2`.
- `(x - 2)*(x + 3) = 0` gives `x = -3` and `x = 2`.
- `x^2 = 2` gives exact `-sqrt(2)` and `sqrt(2)`.

Use one lowercase unknown `x`, one `=` sign and real rational constants. Both
sides support addition, subtraction, multiplication, parentheses, division by
constants and powers 0, 1 and 2 (including the square postfix `²`). Implicit
multiplication such as `3x` is supported. Every intermediate polynomial must
have degree at most two; `x^3-x^3=0` is deliberately unsupported. Calls involving
x, x-dependent exponents and denominators are rejected. In particular, `x/x=1`
is not simplified to an identity that would lose the excluded value zero.

Choose **Coefficients a, b, c / Koeficienty a, b, c** in the input selector to
use the original three fields for `a*x^2 + b*x + c = 0`. With `a = 0` this solves
`b*x + c = 0`; `a = 0`, `b = 3`, `c = -1` gives `x = 1/3`.

Coefficients accept exact real numbers and calculator expressions, including
finite decimal literals, fractions, integer powers and exact roots such as
`sqrt(4)`. Approximate coefficients (`sqrt(2)`, `π`), complex typed results and
quantities are rejected. Only the polynomial unknown x is available in whole
equations; saved variables and ans are unavailable. Random calls and chained
equalities are forbidden. The solver does not change calculator sessions.

Each root has a copyable exact expression. Irrational and complex roots are
supported even though coefficients must be real rational values. A separate
`≈` line gives a decimal preview when it differs from the exact expression.
**Preview precision / Presnosť náhľadu** controls 1–1000 significant digits,
default 16; exact roots do not change. A repeated quadratic root appears once
with multiplicity two. Constant equations report no solutions or all complex
values. Errors identify and focus the failing coefficient when available.

Editing clears old results and aborts pending requests; calculations run only
on explicit submission. Clear empties the active input mode. The mode, both inputs, precision
and completed results are retained in the same tab across tool/language changes
when session storage is available. Results larger than the 24000-character
storage allowance are not retained; solving them again remains possible.
Closing the tab clears this state. On phones, a completed solve scrolls to the
result. Clipboard access requires a supported secure browser context.

The browser calls the [documented polynomial HTTP API](../reference/EQUATION_HTTP_API.md)
with `mode=expression` or `mode=coefficients`.
All mathematics and formatting run in C. The complete coefficient body fits in
4096 UTF-8 bytes. Evaluation, solving, root previews and serialization share the
cooperative five-second budget, cumulative 64 MiB allocation budget and 512 KiB
single-allocation limit. Exact strings are bounded to 65536 bytes each and the
HTTP response to 128 KiB. More requested digits do not guarantee completion.

Previews use guard digits and a stable real-root projection to avoid subtracting
nearly equal numbers. They are approximate, without a certified rounding
interval. Exact radicals are not necessarily simplified by extracting square
factors. Non-polynomial equations, irrational coefficients, systems, numerical
interval roots and graph integration remain subsequent work.
