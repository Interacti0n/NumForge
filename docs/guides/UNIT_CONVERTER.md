# Browser unit converter

Open **Units / Jednotky** (`/units`).

1. Choose a category, source and destination unit.
2. Enter a number or expression, e.g. `1/3` or `sqrt(2)`.
3. Wait two seconds for a preview, or press **Enter / Convert** to confirm.

**Swap** exchanges units while keeping the input; **Copy** copies the number
without its unit. Mobile confirmation closes the keyboard and reveals the result.
Small windows scroll normally; desktop results/history scroll inside their cards.

**Precision and display** separates working digits (default 34) from displayed
places (default 10). Full/custom output, notation and six rounding modes are
available. RAD/DEG affects functions in the expression, not the selected units.
Exact and approximate factors/results are identified; a rounded display is not
an error bound. **About units and sources** explains definitions and distinctions
such as temperature points/intervals, MB/MiB and US/UK volumes.

Conversions can read the same tab's confirmed calculator variables/ans, but do
not change them. Assignments and random calls are rejected. A quantity-valued
input needs an explicit numeric conversion first. The calculator also accepts
`convert(90;"km/h";"m/s")` → `25` and
`convert(1/3;"km";"m")` → exact `1000/3`.

## Conversion history

Only successful confirmations add entries: up to 16 per session. Select an entry
to restore its original inputs and saved value without reevaluating changed
variables. Its copy button uses the full saved number; the main Copy uses the
displayed number. Editing resumes preview; Enter creates a new confirmation.

Clear removes converter history while preserving calculator state. Reload and
tool/language navigation retain the session; expiry, reset/eviction or server
restart remove stored values. Inputs/settings are tab-local, not a permanent
archive. [Session contract](../reference/SESSION_HTTP_API.md).

## Currencies

Choose **Currencies / Meny**, enter a decimal amount (point or comma), choose a
pair and press Convert. Auto uses the target currency's usual decimal places;
2/4/8 places can be selected. Negative amounts and up to 200 integer digits are
accepted; expressions and exponents are not.

The free [Frankfurter API](https://frankfurter.dev/) provides daily
[ECB reference rates](https://www.ecb.europa.eu/stats/policy_and_exchange_rates/euro_reference_exchange_rates/html/index.en.html),
**not live trading quotes**. Publication date, last check and source are shown;
the searchable table quotes units per 1 EUR. Banks may apply different rates/fees.

Opening this tab fetches/caches a validated table and rechecks after an hour
while visible. Manual refresh has a one-minute cooldown. Failures may use a
saved table with a warning; unusable/ten-day-old data disables conversion.
Amounts go only to NumForge, never to the provider. Large amounts are calculated
in C, with one ties-to-even rounding step. Currency rates are separate from fixed
units and Quantity expressions.

### Supplied-rate HTTP arithmetic

The stateless endpoint accepts supplied rates; it does not fetch/authenticate
them. See the [currency HTTP contract](../reference/UNIT_HTTP_API.md#currency-arithmetic)
for body, rounding and size/error rules. Fixed-unit conversion uses the
[unit HTTP API](../reference/UNIT_HTTP_API.md).
