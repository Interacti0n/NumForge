#ifndef NUMFORGE_UNITS_H
#define NUMFORGE_UNITS_H

#include <numforge/bigrational.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum NumForgeUnitQuantity
{
    NUMFORGE_UNIT_LENGTH, NUMFORGE_UNIT_AREA, NUMFORGE_UNIT_VOLUME,
    NUMFORGE_UNIT_MASS, NUMFORGE_UNIT_TIME, NUMFORGE_UNIT_SPEED,
    NUMFORGE_UNIT_TEMPERATURE, NUMFORGE_UNIT_TEMPERATURE_INTERVAL,
    NUMFORGE_UNIT_INFORMATION, NUMFORGE_UNIT_ANGLE
} NumForgeUnitQuantity;

typedef enum NumForgeUnitStatus
{
    NUMFORGE_UNIT_OK = 0, NUMFORGE_UNIT_NULL_ARGUMENT,
    NUMFORGE_UNIT_UNKNOWN_UNIT, NUMFORGE_UNIT_INCOMPATIBLE_UNITS,
    NUMFORGE_UNIT_INVALID_ARGUMENT, NUMFORGE_UNIT_OUT_OF_MEMORY,
    NUMFORGE_UNIT_VALUE_TOO_LARGE, NUMFORGE_UNIT_SCALE_OVERFLOW,
    NUMFORGE_UNIT_NON_RATIONAL_CONVERSION
} NumForgeUnitStatus;

/* Read-only entries have static lifetime. IDs are case-sensitive ASCII;
 * no implicit aliases or prefix guessing. Unknown IDs/indices return NULL. */
typedef struct NumForgeUnitInfo
{
    const char *id;
    const char *symbol;
    NumForgeUnitQuantity quantity;
    const char *name_en;
    const char *name_sk;
    const char *source_url;
    /* Exact canonical scale includes pi raised to this power (0 or 1). */
    int pi_power;
} NumForgeUnitInfo;

size_t numforge_unit_count(void);
const NumForgeUnitInfo *numforge_unit_at(size_t index);
const NumForgeUnitInfo *numforge_unit_find(const char *id);
bool numforge_units_compatible(const char *from, const char *to);
/* False for unknown/incompatible units or a conversion requiring pi. */
bool numforge_unit_conversion_is_exact(const char *from, const char *to);
const char *numforge_unit_status_to_string(NumForgeUnitStatus status);

/* Exact affine conversion, supporting result/value aliasing. Output stays
 * unchanged on failure. Temperature points and intervals are incompatible.
 * Signed values, including below-zero kelvin, are accepted: scale conversion
 * does not validate physical domains. Compatible angle units with different
 * pi powers return NON_RATIONAL_CONVERSION, leaving output unchanged.
 * No quantity arithmetic yet. Names are UTF-8 display labels, not aliases. */
NumForgeUnitStatus numforge_unit_convert_rational(
    BigRational *result, const BigRational *value,
    const char *from, const char *to);

/* Converts the stored decimal exactly first, with no intermediate rounding.
 * Positive significant digits and rounding apply to recurring results;
 * terminating results stay exact, as in bigrational_to_bigdecimal.
 * Angle conversions requiring pi use guarded decimal approximation and round
 * to digits. They are not a certified correctly-rounded/error-bound API.
 * The client applies final display rounding. Same alias/failure guarantees. */
NumForgeUnitStatus numforge_unit_convert_decimal(
    BigDecimal *result, const BigDecimal *value,
    const char *from, const char *to, int64_t digits,
    BigDecimalRoundingMode rounding);

#ifdef __cplusplus
}
#endif
#endif
