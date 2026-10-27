# Browser calculator guide

## First calculation

1. [Download or build](../../README.md#get-started), then launch `numforge_web`.
2. Open its printed address, normally `http://127.0.0.1:8765`.
3. Enter `2+2` and press **Enter** or **Calculate**: `4`.

Choose **Full / Fraction** for `1/3+1/6` → `1/2`.

## Controls and behavior

Type directly or search the function library in English/Slovak. Function buttons
insert at the caret; arguments use `;`. Decimal points and commas are accepted.
**RAD/DEG** applies to the whole expression; complex functions use radians.

A preview appears after two seconds without edits. **Calculate / Enter** confirms
immediately and updates `ans` and history. Editing cancels outdated previews.
**Copy** copies the result; **Show all** expands long output. On phones,
confirmation dismisses the keyboard and reveals the result.

Desktop lists scroll inside aligned panels; smaller windows scroll normally.
Settings survive tool/language navigation in the same tab. Header links open
Units/Currencies, Graphs, Equations and Matrices. Accounts are planned.

## Precision and result format

| Setting | Meaning |
| --- | --- |
| Auto precision | Up to 10 output decimal places, ties-to-even rounding |
| Custom | 0–10000 output decimal places |
| Full | No final display rounding; approximations use 34 working digits |
| Fraction | Reduced exact fraction where possible |
| Plain / Scientific / Mathematical | Decimal text / `1.23E+45` / superscript exponent |
| Auto notation | Short exact fraction, otherwise plain or mathematical scientific display |

For 40 output places of `sqrt(2)`, select **Custom: 40 / Plain**. More output
places do not certify whole-expression accuracy. Exact values can have rounded
displays; approximate values cannot recover discarded digits by reformatting.
Auto switches beyond exponents −9..9 or 80 plain characters. Copy uses
parser-compatible `E` notation where possible.

## Variables and history

Confirm `x=2/3`, then use `x*3` → `2`. The last confirmed value is `ans`.
The Session panel retains 32 variables and 16 calculator confirmations.
Reload/navigation retain the tab's session; **New session**, expiry, eviction
or server restart discard it. Save important work elsewhere.
See [variables](VARIABLES.md) for value and deletion rules.

## Application limits

Processing has a cooperative **10-second** budget, including parsing and output;
valid inputs can still exceed time/memory limits. Input/output text is bounded
to 4096/65536 UTF-8 bytes. Use finite precision and scientific notation for huge
factorials. **Full** does not remove these limits.

[Expression domains and budgets](../reference/CALCULATOR_EXPRESSIONS.md) ·
[Hosting and retained memory](HOSTING.md#capacity-and-state)

## Automatic complex results

`sqrt(-1)` and negative logarithms select principal complex values; real odd
roots remain real. Poles and integer-domain restrictions still apply.
See [complex domains](../reference/CALCULATOR_EXPRESSIONS.md#automatic-complex-domains).

## Guide and API parts

**Guide / API** opens help for the current tool. Choose another part for
Calculator, Units, Graphs, Equations, Matrices or API details.
