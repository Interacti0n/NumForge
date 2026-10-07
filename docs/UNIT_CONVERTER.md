# Browser unit converter

Open **Units / Jednotky** in the local application, or `/units?lang=en` /
`/units?lang=sk`. The page shares the calculator's purple workbench, navigation,
form controls and background. All assets are embedded in `numforge_web`.

1. Choose a quantity. On mobile, open the quantity selector above the converter.
2. Enter a number or calculator expression, such as `1/3` or `sqrt(2)`.
3. Choose source and destination units. Common units appear first, followed by
   the remaining catalogue entries and prefixes. Both menus contain only units
   in the selected compatible category.
4. The result previews automatically. Press Enter or **Convert / Previesť** to
   confirm the displayed conversion. **Swap** exchanges units and keeps the input
   value. **Copy** copies the result number; the destination symbol is displayed
   separately. Long results wrap and can be scrolled with keyboard focus.

The desktop sidebar offers ten quantity categories and examples. On narrow
screens the categories collapse behind a button; choosing one closes the menu
and returns keyboard focus to that button. Native selects, visible focus,
labelled controls and live status messages support keyboard/screen-reader use.
Shift+Enter inserts a line break; Enter converts.

On desktop viewports at least 1200 pixels wide and 740 pixels high, the page
uses one screen: expanded settings sit beside the input, with the result and
footer remaining visible. Long numeric results scroll within their field.
Narrower or shorter windows use normal page scrolling, including around
961–1199 pixels where the calculator's desktop styles would otherwise lock it.

**Precision and display** separates working significant digits from displayed
decimal places. Defaults are 34 working digits and 10 displayed places, with
automatic notation and nearest/ties-to-even rounding. Full/custom output,
plain/scientific/fraction notation and all six rounding modes are available.
RAD/DEG controls functions inside the expression; source/destination units
control the conversion. Approximation notes distinguish approximate expression
evaluation and pi-based angle factors. Exact factors do not imply unrounded
displayed text or certified rounding of approximate calculations.

**About units and sources** links to the definitions for the selected units and
explains distinctions such as temperature points versus intervals, MB versus
MiB, and US versus UK volume units. Values below absolute zero are converted
mathematically; physical-domain validation is outside this converter's scope.

When arriving from the calculator in the same tab, conversions can read its
confirmed `ans` and session variables. They never change them, calculator
history, random state or pending previews. Confirmation currently marks the
conversion in the UI only. Assignments and `rand()` are rejected. If the session
expires, return to the calculator to start a new one. A direct visit works
without a calculator session. Selections/settings/input survive language and
tool navigation in the same tab; each result is recomputed. A page reload can
restore converter fields, while calculator sessions follow their own lifecycle.

Requests use the [unit HTTP API](UNIT_HTTP_API.md). Editing input/settings or
changing category cancels the pending request and clears the old copy value.
Outdated responses cannot overwrite a newer result. Network/catalogue failures
offer retry without changing calculator state. This stage adds no conversion
history, `convert(...)` parser syntax or arithmetic on typed quantities.
