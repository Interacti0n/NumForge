# Browser calculator guide

## First calculation

1. [Download or build NumForge](../../README.md#get-started) and launch `numforge_web`.
2. Open the local address printed by the application, normally `http://127.0.0.1:8765`.
3. Enter `2+2` and press **Enter** or **Calculate**. The result is `4`.

For an exact fraction, choose **Full** precision and **Fraction** notation, then
try `1/3+1/6`: the result is `1/2`.

## Controls and behavior

Type expressions directly or use the function library. Function buttons insert
parentheses and place the cursor inside. Search accepts English and Slovak terms;
selecting a result inserts the supported function name. A decimal point or comma
is accepted; separate function arguments with `;`.

The calculator previews after two seconds without edits. **Enter** or **Calculate**
confirms immediately; only confirmations update history and `ans`. Editing again
discards an outdated preview. The result labels previews and confirmations. Its exact/approximate indicator comes
from the C value; an exact value can still have a rounded decimal display.
On phones, confirmation also dismisses the keyboard and reveals the result.

Settings stay expanded above the expression, including on phones. Try the clickable
examples in an empty result panel, or browse the horizontally scrolling mobile
function categories. Use **RAD/DEG** before trigonometric calculations. The setting applies to the
whole expression. **Copy** copies the result; **Show all / Zobraziť všetko** expands
long output. Language, precision and notation settings survive navigation in the
same browser tab.

The header also opens [units](UNIT_CONVERTER.md), [graphs](GRAPHS.md) and
[equations](EQUATIONS.md). Footer links to sign-in and registration are informational pages;
accounts are not implemented.

## Precision and result format

- **Auto precision:** display up to 10 decimal places, rounded half-even.
- **Custom precision:** choose 0–10000 output decimal places.
- **Full precision:** skip final display rounding; working precision and resource limits still apply.
- **Fraction notation:** show a reduced exact fraction when possible.
- **Plain / Scientific / Mathematical:** choose decimal text, `1.23E+45`, or `1.23 × 10^45`.
- **Auto notation:** prefer a short exact fraction, otherwise plain or scientific output.

For example, select **Custom: 40** and **Plain**, then calculate `sqrt(2)` for
40 output places. More output places do not guarantee that every digit of a
composed approximate expression is accurate.

Exact arithmetic retains rational values: `(1/3)*3-1` is exactly `0`.
Changing precision can reformat a retained rational value. An approximate result
cannot recover digits already lost to rounding. **Full** uses 34 working digits
where approximation is needed; it does not mean unlimited accuracy.

Auto notation switches to scientific if the rounded exponent is outside -9..9
or plain output would exceed 80 characters. A fraction may have a separate
`≈` hint at up to 10 places. Copy converts mathematical notation to parser-compatible
`E` form when possible. Display text is not a lossless serialization format at
extreme scales. See the [expression reference](../reference/CALCULATOR_EXPRESSIONS.md)
for domains and precision rules.

## Variables and history

The last confirmed result is available as `ans`. Assign a variable with `x=5`,
then use `x` in another expression. See [Variables](VARIABLES.md) for editing,
deletion and typed values.

The session retains the last 16 calculator confirmations. Reload, language
changes and navigation in the same tab retain an existing session.
**New session** resets it. Server restart or FIFO eviction can discard its values;
there is no disk persistence. Save important results outside the application.

## Application limits

- Calculations have a **10-second** budget, including parsing and output;
  cancellation is cooperative.
- Expression/result text is limited to **4096/65536 UTF-8 bytes**.
- Each session retains **32 variables** and **16 calculator confirmations**.
- The server retains **64 sessions**, with idle expiry and
  [memory limits](HOSTING.md#capacity-and-state). Clear history/variables or start
  a new session if its retained-memory limit is reached.

Full precision does not remove limits. Expensive calculations may fail even when
their inputs are valid. For example, the complete decimal output of `100000!`
exceeds the result-text limit; use finite precision and scientific notation.
The [expression reference](../reference/CALCULATOR_EXPRESSIONS.md) defines
calculation memory, nesting and argument limits; the [HTTP reference](../reference/HTTP_API.md)
and [session reference](../reference/SESSION_HTTP_API.md) cover transport and paging.
These application limits do not impose implicit budgets on public numeric C calls.

## Automatic complex results

Supported expressions such as `sqrt(-1)` and negative logarithms select principal
complex results automatically. Real odd roots remain real. Complex angles use
radians even in DEG mode. `sum`, `product` and `mean` accept complex arguments;
other statistics are real-only. Poles and integer-only constraints remain errors.
See [complex domains](../reference/CALCULATOR_EXPRESSIONS.md#automatic-complex-domains).

## Guide and API parts

The in-app **Guide / API** page has Calculator, Units, Graphs, Equations and API
parts. Choose a part and then a section; phones use a compact part selector.
For example, `/api?section=calculator&lang=en` opens the English calculator guide.
Older links such as `/api?lang=en#variables` still open their matching section.
The API part contains developer details; everyday controls and examples are in
the other four parts.
