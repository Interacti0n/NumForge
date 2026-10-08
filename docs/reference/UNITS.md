# Unit conversion foundation

The additive public C API in [units.h](../../include/numforge/units.h) provides a
static registry, compatibility checks and rational/decimal conversion.
The calculator exposes these conversions through `convert(...)` expressions.
The client calculator also supports [typed Quantity expressions](QUANTITIES.md),
including compatible sums, products, quotients and integer powers.

## Related documentation

This document is the entry point for the implemented unit-conversion contracts.
Detailed contracts are maintained in:

- [Quantity expressions](QUANTITIES.md): typed arithmetic, dimensions, temperature
  semantics, session behavior and current limitations.
- [Unit catalogue](UNIT_CATALOG.md): factors, SK/EN names and provenance.
- [Unit HTTP API](UNIT_HTTP_API.md): requests, errors, limits and snapshots.
- [Browser converter](../guides/UNIT_CONVERTER.md): interaction, layout and conversion history.
- [Testing](../TESTING.md): regression coverage and verification workflows.

Future implementation work is tracked in the local project review.

## Calculator expressions

`convert(value; "from"; "to")` is available in CLI, web calculator and numeric
expressions passed to `/api/evaluate` and `/api/convert`. It returns an ordinary
number, so `x = convert(1; "km"; "m")` stores numeric `1000`. Confirmation has
the usual calculator history/ans behavior; preview does not change session state.
The separate Units page keeps its own conversion snapshots.
With a Quantity operand, `convert` validates the declared source dimensions
and returns the coordinate in the target unit. See [Quantity conversion](QUANTITIES.md#conversion-api-and-limits).

The first argument is any numeric expression, including variables, `ans` and
nested calls. The other two arguments must be double-quoted catalogue IDs,
with case preserved, 1–31 ASCII characters. No escapes, localized names,
Unicode symbols, variable unit names or general-purpose strings are accepted.
Use `"m/s"`, `"m2"`, `"m3"`, `"um"` and `"degC"`; no implicit parsing of
`"m^2"` or arbitrary compound units. `MB`, `MiB`, `B` and `bit` stay distinct.
The built-in name `convert` is reserved; variables named `m`, `s` or `kg` remain
ordinary numeric variables and never collide with quoted IDs.

Unknown IDs report `unknown unit` at the opening quote of that argument.
Incompatible categories report `incompatible units` at the target argument.
Temperature points and intervals are separate categories. Malformed literals
report lexical errors at the invalid character or end of an unclosed quote;
missing quotes/separators/arguments report syntax errors at the unexpected token.
Unit identity and compatibility are checked before numeric evaluation.

Exact numeric operands and rational conversion factors retain reduced fractions:
`convert(1/3; "km"; "m") = 1000/3` and
`convert(100; "degC"; "degF") = 212`.
Approximate operands and angle factors requiring pi use the existing decimal
evaluation policy and working precision/rounding. RAD/DEG affects functions
inside the first argument, not the explicit conversion units. Approximation
is not certified correctly rounded, and returning a number does not establish
quantity arithmetic or dimensional checking of surrounding expressions.

The call uses the ordinary input, AST depth, allocation, output and cooperative
time budgets; it cannot start a fresh budget inside a nested expression.
The catalogue and conversion implementations are shared with the public C API.

## Catalogue and identity

The [full catalogue](UNIT_CATALOG.md) lists all **233 entries**, their English
and Slovak display names, exact factors/offsets and primary-source provenance.
It is generated with the C registry by
[scripts/generate_unit_catalog.cjs](../../scripts/generate_unit_catalog.cjs).
Run the generator with --check to verify that checked-in data match the source.
No generator or network access is needed by library users.

| Quantity | Scope |
| --- | --- |
| Length, area, volume | All 24 SI prefixes on metres, square/cubic metres and litres; international inches/feet/yards/miles and their square/cubic forms; hectare, acre, nautical mile |
| Mass | All SI prefixes on grams, tonne, avoirdupois pound/ounce |
| Time | All SI prefixes on seconds; minute, hour, fixed 24-hour day |
| Speed | m/s, km/h, mph, knot |
| Temperature point / interval | K/degC/degF and deltaK/deltaC/deltaF, separately |
| Information | bit/B; decimal k/M/G/T/P/E/Z/Y/R/Q multiples; binary Ki/Mi/Gi/Ti/Pi/Ei/Zi/Yi multiples |
| Plane angle | rad, deg, arcmin, arcsec, turn, gon |

IDs are case-sensitive ASCII strings, independent of UI language. Symbols and
name_en/name_sk are UTF-8 display labels, not accepted input aliases. For
micro use ASCII u in IDs (um, us, uL); symbols display µ. Names are singular
labels, without locale-dependent pluralization. Registry indices/order are not
stable identifiers: retain IDs instead. Entries are borrowed, immutable and
have static lifetime. Unknown IDs and out-of-range indices return NULL.
There are no implicit prefixes, whitespace normalization or user-defined units.

MB means decimal megabyte; MiB means binary mebibyte. A byte is eight bits.
KB, bare gal/pt/qt/floz and localized names are rejected. Use gal_US/gal_UK,
pt_US/pt_UK, qt_US/qt_UK and floz_US/floz_UK explicitly. US liquid units and
UK Imperial units have separate factors. lb/oz refer to avoirdupois mass,
not troy mass or force. ft/mi/acre use the international foot definition;
historical survey units are not included. A day is fixed duration, not a
calendar-aware operation. Month/year/currency and density-dependent conversions
are outside this catalogue.

## Compatibility and exactness

Use numforge_unit_count/at/find to enumerate and inspect units.
numforge_units_compatible returns false for unknown IDs, NULL or different
quantities. Conversion distinguishes NULL_ARGUMENT, UNKNOWN_UNIT and
INCOMPATIBLE_UNITS. Temperature points and intervals are incompatible.
Signed values are permitted, including below-zero kelvin: this API converts
scales and does not validate physical domains.

Canonical values use a rational scale and offset. Angle scales also keep an
exact symbolic pi exponent, exposed as pi_power. numforge_unit_conversion_is_exact
checks compatibility and cancellation of that exponent; it describes the unit
factor, not whether a particular value (such as zero) happens to be exact.

numforge_unit_convert_rational supports all rational unit ratios. It returns
NON_RATIONAL_CONVERSION for compatible angles whose pi powers differ, even for
zero input, leaving output unchanged. For example, degree to arcminute is exact,
while degree to radian requires decimal projection. 1/3 km becomes exactly
1000/3 m. No rounded table value is used as a purported exact factor.

numforge_unit_convert_decimal takes a positive significant-digit count and a
valid rounding mode. Rational conversions first convert the stored finite
decimal exactly; only recurring results are rounded, using
bigrational_to_bigdecimal semantics. Terminating results stay exact. The client
applies final display rounding separately.

Angles requiring pi use an exact rational coefficient and the existing pi
implementation at requested precision + 24 digits. Guarded projection is then
rounded to the requested significant digits in any of the six modes. Zero
remains zero. This is an approximate numerical path, not a certified
correct-rounding guarantee or rigorous error bound for all inputs. Precision
addition is checked for overflow. General correctly rounded approximations
remain a separate roadmap item.

Both conversion APIs support input/output aliasing and preserve output on
failure. Registry validation precedes numeric allocation (after decimal
precision validation). The public registry exposes source_url for every entry;
[UNIT_CATALOG.md](UNIT_CATALOG.md) records the derivation as well.

## Example

The local server exposes the catalogue and read-only expression conversion via
[`/api/units` and `/api/convert`](UNIT_HTTP_API.md). The SK/EN browser converter
is available at `/units`; see [the user guide](../guides/UNIT_CONVERTER.md).
Calculator `convert(...)` uses the same catalogue and conversion implementation.

~~~c
#include <numforge/units.h>

/* value is an initialized BigDecimal containing 90. */
NumForgeUnitStatus status = numforge_unit_convert_decimal(
    value, value, "km/h", "m/s", 30, BIGDECIMAL_ROUND_HALF_EVEN);
/* On success value is exactly 25. kg -> m returns INCOMPATIBLE_UNITS. */
~~~

## Verification and sources

Tests cover independent known conversions, extreme prefixes, large values,
all registry pairs and exact round trips. In-place rational, decimal and both
pi-direction paths undergo allocation-failure injection. Frozen angle references
use mpmath 1.3.0 at requested +240/+400 digits, with Decimal rounding; see
[the generator](../../tests/generate_unit_angle_references.py). These finite cases
support regression testing, not a proof of rounding for arbitrary inputs.

SI powers and prefix rules follow
[NIST SP 330 section 3](https://www.nist.gov/pml/special-publication-330/sp-330-section-3).
Accepted units/angles follow
[section 4](https://www.nist.gov/pml/special-publication-330/sp-330-section-4).
British definitions follow the
[Weights and Measures Act schedule](https://www.legislation.gov.uk/ukpga/1985/72/schedule/1).
US liquid-volume factors are derived from the exact gallon definition in
[NIST Handbook 44 Appendix C](https://www.nist.gov/system/files/documents/2023/01/30/appc-23-HB44.pdf),
rather than rounded decimal conversion tables. Additional sources are linked
per entry in the full catalogue.
