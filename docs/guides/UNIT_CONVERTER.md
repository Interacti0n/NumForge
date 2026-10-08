# Browser unit converter

Calculator expressions also accept `convert(value; "from"; "to")`.
For example, `convert(90; "km/h"; "m/s")` returns `25`, and
`convert(1/3; "km"; "m")` returns the exact number `1000/3`.
See [expression syntax and limits](../reference/UNITS.md#calculator-expressions).
Typed [Quantity expressions](../reference/QUANTITIES.md) are available in the calculator.
This converter still expects a numeric coordinate; use explicit `convert(...)`
to extract a number from a quantity variable or quantity-valued `ans` first.

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
footer remaining visible. The converter uses only its content height; remaining
space goes to the result panel. Long numeric results scroll within their field.
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
history, random state or pending previews. Confirmation adds a record to the
separate converter history. Assignments and `rand()` are rejected. If the session
expires, reset/start a new calculator session. A direct visit starts a session. Selections/settings/input survive language and
tool navigation in the same tab; each result is recomputed. A page reload can
restore converter fields, and the same retained server session is read again.

Previews use the [unit HTTP API](../reference/UNIT_HTTP_API.md); confirmations
and saved history use the [session HTTP API](../reference/SESSION_HTTP_API.md).
Editing cancels previews and clears the old copy value. Outdated responses
cannot overwrite newer results. Sent confirmations remain retryable with their
original revision and input even if the response is lost or the user navigates.

## Conversion history

Only a successful Enter/Convert adds an entry. Previews, errors and restoring an
entry add nothing. The application retains the last 16 confirmations per session;
clear history resets converter numbering without changing calculator state.

Each record owns the expression, source/destination units, original context,
display and a versioned typed value. Exact results preserve integers or reduced
rationals; approximate results preserve all digits of the finite computed decimal,
tagged `decimal_approximation`. This preserves the approximation without making
the mathematical result exact. Numeric text is limited to 65,536 bytes and a full
JSON response to 128 KiB; an oversized confirmation fails before adding an entry.

Click an entry to retrieve and restore its original fields and displayed result,
without reevaluating against changed variables/ans. Its copy button copies the
full saved numeric text (a fraction or finite decimal in scientific notation),
without the unit. The main result copy button copies displayed numeric text.
Editing restored fields starts a fresh preview; Enter creates a new confirmation.

History lives in server memory under the tab's session ID, shared with the
calculator. Reload and tool/language navigation read that same session;
independently started sessions remain isolated. Server restart, reset/release
or FIFO session eviction removes saved values. There is no permanent archive.
Browser session storage keeps presentation fields and a small pending request,
not authoritative history. A lost response is retried with the same revision,
expression and settings; the server returns the saved result without appending
twice. A pending confirmation cannot be submitted twice concurrently.
Examples are collapsible; history scrolls within its card.
