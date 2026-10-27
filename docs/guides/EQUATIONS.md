# Browser equations

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

Choose **Coefficients a, b, c / Koeficienty a, b, c** in the mode selector to
use the original three fields for `a*x^2 + b*x + c = 0`. With `a = 0` this solves
`b*x + c = 0`; `a = 0`, `b = 3`, `c = -1` gives `x = 1/3`.

### Editor and examples

**Functions & constants / Funkcie a konštanty** opens an editor for the current
equation or last-focused coefficient. Keep typing while it is open. In coefficient
mode, its a/b/c selector switches fields without losing edits. The catalogue uses
the same C availability registry and localized descriptions/search aliases as
the calculator and graphs. Functions insert balanced parentheses with the caret
inside; selected text becomes the first argument, and required further arguments
use semicolon separators. Search supports names, abbreviations and translated
aliases. Categories and pagination keep the catalogue within ordinary phone,
landscape and desktop viewports.

Exact modes offer notation and functions for **constant exact real values**;
functions of x and irrational constants remain unsupported there. Numerical
modes additionally offer trigonometric/hyperbolic functions, statistics and
π/e/φ. RNG and calculator variables are not inserted. Function availability
does not guarantee that every argument/domain will pass numerical verification.

The editor updates the original field immediately. Escape/Close returns to that
field and keeps its caret. Clear affects the selected field. Solve/Enter closes
the editor and submits the whole form, including all coefficients or interval
bounds. Opening, searching or selecting an example does **not** run a solve.
If the catalogue cannot load, manual input still works and reopening retries.
The editor closes cleanly on tool/language navigation without losing input.

**Examples / Príklady** provides four inputs per mode, including complex
quadratic roots, a fractional coefficient, a cosine bracket, sine roots and a
rational contact. Examples fill the appropriate coefficients/bounds and clear
old results; explicitly choose Solve to evaluate them.

Roomy desktop windows align input/solutions above examples/recent tasks; each
long result list scrolls inside its panel. Smaller windows use page scrolling.
**Recent equations / Posledné rovnice** keeps up to eight completed input sets
in this tab, including mode, coefficients, interval and precision. Selecting one
cancels pending work and clears the result; Solve is always explicit. Entries
can be removed individually or cleared together. It is input-only local storage,
not account history; [shared limits](../design/WEB_DESIGN.md#shared-workbench-layout) apply.

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
cooperative ten-second budget, cumulative 128 MiB allocation budget and 8 MiB
single-allocation limit. Exact strings are bounded to 65536 bytes each and the
HTTP response to 128 KiB. More requested digits do not guarantee completion.

Previews use guard digits and a stable real-root projection to avoid subtracting
nearly equal numbers. They are approximate, without a certified rounding
interval. Exact radicals are not necessarily simplified by extracting square
factors. Irrational coefficients in exact mode and systems
remain subsequent work.

## Show a solution in the graph

For a real root within the browser's displayable numeric range, **Show in graph /
Zobraziť v grafe** adds `(left side)-(right side)` to Graphs. Coefficient mode
uses `(a)*x^2+(b)*x+(c)`. Solutions therefore lie on the x axis, even when the
original right side was nonzero. This keeps the original expression's domain.
Complex roots have no graph action.

The camera centers approximately on the selected root, with y from -10 to 10.
Exact-mode and multiple-root scan previews position a broad camera only;
single numerical mode uses the full checked decimal coordinate. Neither
becomes a verified graph marker automatically:
intercepts use the existing independent C graph verification, which may omit
unresolved points. The plot is an approximate visualization, not a second
equation proof. Existing functions are retained; matching expressions are reused.
If all four graph slots are occupied, remove a function before adding another.
Transfer requires session storage in the same tab; input and solver results
remain available when returning to Equations.

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
Before publishing a non-exact result, C checks the original equation again at
higher precision. If the residual or sign is unstable, it asks for more precision
instead of showing an apparent root. The displayed residual uses that final check.

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
The same ten-second/memory budget applies. All math uses BigRational/BigDecimal;
finite-precision signs and residuals still do not give certified existence or
root-error bounds. See the [numerical API](../reference/NUMERIC_ROOT_HTTP_API.md).

Open **Checked point / Overený bod** below a numerical result to read the full
coordinate and residual. These details stay collapsed to keep result lists
readable. If clipboard access fails, the details open for manual selection.
Live announcements use the result status and errors rather than rereading the
whole root list when a copy button changes its label.

## Multiple roots on an interval

Select **Multiple interval roots / Viac koreňov na intervale**. For example,
`sin(x)=0` on -10..10 finds seven roots, each with its checked coordinate,
residual, copy and graph action. Calculation precision is 16–100 digits,
as in the single-root mode; the fixed tolerances stay at 1E-12.

C samples 128 equal interval sections and refines sign changes using the same
checked single-root solver. It can also retain exact sampled zeros. A grid
sample merely rounded to zero needs an opposite-sign neighbour on both sides.
Extra midpoint/local-quadratic candidates can find simple off-grid contacts,
such as `(x-1/3)^2=0`, and close pairs such as `(x-0.01)*(x-0.02)=0` even when
they lie in one section. Candidates are checked in the original equation;
small minima alone do not qualify. A positive `(x-0.1234)^2+1E-100` has no zero.
Undefined/non-real samples leave gaps; an unverified bracket contributes no root,
while the scan can continue elsewhere.

Scan **Copy point** retains the exact rational coordinate (for example `1/3`),
while its decimal preview is approximate. The residual refers to that copied
rational point. This also keeps recurring fractions exact across API and graph
navigation; the graph camera uses the preview and verifies markers independently.
Older grid-only cached scan results are discarded while preserving the input.

**The list may be incomplete**, even after the scan finishes. General tangent roots
between samples, several roots in one section, approximate endpoint zeros and
very dense oscillations can be missed. An empty list means this scan confirmed
no root, not that none exist. Try a narrower/shifted interval. The scan stops at
32 results or its 4096-coordinate work ceiling and explains which limit was
reached. It shares one ten-second/memory budget, rather than granting ten
seconds per root. Resource failures discard the whole pending result.
