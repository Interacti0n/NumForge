# Unit conversion foundation

The additive public C API in [units.h](../include/numforge/units.h) provides a
static registry, compatibility checks and rational/decimal conversion. The
Calculator convert syntax and arithmetic on
quantities are not implemented yet.

## Catalogue and identity

The [full catalogue](UNIT_CATALOG.md) lists all **233 entries**, their English
and Slovak display names, exact factors/offsets and primary-source provenance.
It is generated with the C registry by
[scripts/generate_unit_catalog.cjs](../scripts/generate_unit_catalog.cjs).
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
is available at `/units`; see [the user guide](UNIT_CONVERTER.md).
Calculator `convert(...)` syntax remains planned.

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
[the generator](../tests/generate_unit_angle_references.py). These finite cases
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
