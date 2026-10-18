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
factors. Irrational coefficients in exact mode, systems and graph integration
remain subsequent work.

## Numerical interval search

Choose **Numerical interval root / Numericky na intervale** for a real equation
such as `cos(x)=x`. Default bounds are 0..10; 0..1 is a useful bracket for this
example. Use a continuous interval where the difference between equation sides
has opposite signs at the endpoints. Lowercase x and radians are fixed. This
mode searches for one approximate root and does not enumerate all roots.

The result shows a coordinate and residual evaluated by C. **Copy point /
Kopírovať bod** copies the full checked coordinate, which can contain more digits
than the preview. Residuals refer to that full point. Numerical precision is
16–100 digits; both absolute search width and residual target 1E-12, with strict
residual improvement over endpoints. Exact rational zeros may be accepted
immediately. Preview precision does not tighten the fixed tolerances.

Without a confirmed sign change, roots can still exist, especially tangent
roots. Approximate endpoint zeros alone are not confirmed. Sampled domain gaps,
insufficient precision and the 96-step/192-coordinate limits produce explanatory
outcomes instead of “no solutions”. Narrow or shift the interval when needed.
Before reporting an approximate root, C checks the real domain and continuity
of both sides around the candidate. If it cannot verify them, no root is shown;
this can happen for valid expressions outside the current conservative checks.
Safe divisions, sqrt and ln remain supported when the local ranges establish
their domains. Steps, tan and some nested expressions may remain unverified.
Exactly zero typed rational points do not require a continuous surrounding
interval. Old numerical results saved before these checks are discarded while
the tab's inputs are retained; solving again checks the current expression.
The same five-second/memory budget applies. All math uses BigRational/BigDecimal;
finite-precision signs and residuals still do not give certified existence or
root-error bounds. See the [numerical API](../reference/NUMERIC_ROOT_HTTP_API.md).
