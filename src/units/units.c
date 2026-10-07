#include <numforge/units.h>
#include <string.h>

/* Canonical value = value * scale + offset, all coefficients exact.
 * Angle scales include symbolic pi. Factor provenance and local names are
 * generated from scripts/generate_unit_catalog.cjs; see docs/UNIT_CATALOG.md. */
typedef struct UnitDefinition
{
    NumForgeUnitInfo info;
    const char *sn, *sd, *on, *od;
} UnitDefinition;

static const UnitDefinition units[] = {
#include "unit_catalog.inc"
};

size_t numforge_unit_count(void) { return sizeof(units) / sizeof(units[0]); }
const NumForgeUnitInfo *numforge_unit_at(size_t index)
{
    return index < numforge_unit_count() ? &units[index].info : NULL;
}
static const UnitDefinition *find_unit(const char *id)
{
    if (id == NULL) return NULL;
    for (size_t i = 0; i < numforge_unit_count(); ++i)
    {
        if (strcmp(id, units[i].info.id) == 0) return &units[i];
    }
    return NULL;
}
const NumForgeUnitInfo *numforge_unit_find(const char *id)
{
    const UnitDefinition *unit = find_unit(id);
    return unit != NULL ? &unit->info : NULL;
}
bool numforge_units_compatible(const char *from, const char *to)
{
    const UnitDefinition *a = find_unit(from), *b = find_unit(to);
    return a != NULL && b != NULL && a->info.quantity == b->info.quantity;
}
bool numforge_unit_conversion_is_exact(const char *from, const char *to)
{
    const UnitDefinition *a = find_unit(from), *b = find_unit(to);
    return a != NULL && b != NULL && a->info.quantity == b->info.quantity &&
        a->info.pi_power == b->info.pi_power;
}
const char *numforge_unit_status_to_string(NumForgeUnitStatus status)
{
    switch (status)
    {
        case NUMFORGE_UNIT_OK: return "success";
        case NUMFORGE_UNIT_NULL_ARGUMENT: return "null argument";
        case NUMFORGE_UNIT_UNKNOWN_UNIT: return "unknown unit";
        case NUMFORGE_UNIT_INCOMPATIBLE_UNITS: return "incompatible units";
        case NUMFORGE_UNIT_INVALID_ARGUMENT: return "invalid argument";
        case NUMFORGE_UNIT_OUT_OF_MEMORY: return "out of memory";
        case NUMFORGE_UNIT_VALUE_TOO_LARGE: return "value too large";
        case NUMFORGE_UNIT_SCALE_OVERFLOW: return "scale overflow";
        case NUMFORGE_UNIT_NON_RATIONAL_CONVERSION: return "conversion requires pi approximation";
        default: return "unknown status";
    }
}
static NumForgeUnitStatus map_status(BigRationalStatus status)
{
    switch (status)
    {
        case BIGRATIONAL_OK: return NUMFORGE_UNIT_OK;
        case BIGRATIONAL_NULL_ARGUMENT: return NUMFORGE_UNIT_NULL_ARGUMENT;
        case BIGRATIONAL_OUT_OF_MEMORY: return NUMFORGE_UNIT_OUT_OF_MEMORY;
        case BIGRATIONAL_VALUE_TOO_LARGE: return NUMFORGE_UNIT_VALUE_TOO_LARGE;
        case BIGRATIONAL_SCALE_OVERFLOW: return NUMFORGE_UNIT_SCALE_OVERFLOW;
        default: return NUMFORGE_UNIT_INVALID_ARGUMENT;
    }
}
static BigRationalStatus set_coefficient(BigRational *result,
    const char *numerator, const char *denominator)
{
    BigInt *n = bigint_create(), *d = bigint_create();
    BigRationalStatus status = BIGRATIONAL_OUT_OF_MEMORY;
    if (n != NULL && d != NULL)
    {
        BigIntStatus ns = bigint_set_string(n, numerator);
        BigIntStatus ds = bigint_set_string(d, denominator);
        if (ns == BIGINT_OK && ds == BIGINT_OK)
            status = bigrational_set_fraction(result, n, d);
    }
    bigint_destroy(n);
    bigint_destroy(d);
    return status;
}
static NumForgeUnitStatus convert_coefficients(BigRational *result,
    const BigRational *value, const char *from, const char *to)
{
    if (result == NULL || value == NULL || from == NULL || to == NULL)
        return NUMFORGE_UNIT_NULL_ARGUMENT;
    const UnitDefinition *a = find_unit(from), *b = find_unit(to);
    if (a == NULL || b == NULL) return NUMFORGE_UNIT_UNKNOWN_UNIT;
    if (a->info.quantity != b->info.quantity) return NUMFORGE_UNIT_INCOMPATIBLE_UNITS;
    if (a == b) return map_status(bigrational_copy(result, value));
    BigRational *work = bigrational_create(), *coefficient = bigrational_create();
    BigRationalStatus status = BIGRATIONAL_OUT_OF_MEMORY;
    if (work == NULL || coefficient == NULL) goto cleanup;
    status = set_coefficient(coefficient, a->sn, a->sd);
    if (status != BIGRATIONAL_OK) goto cleanup;
    status = bigrational_mul(work, value, coefficient);
    if (status != BIGRATIONAL_OK) goto cleanup;
    status = set_coefficient(coefficient, a->on, a->od);
    if (status != BIGRATIONAL_OK) goto cleanup;
    status = bigrational_add(work, work, coefficient);
    if (status != BIGRATIONAL_OK) goto cleanup;
    status = set_coefficient(coefficient, b->on, b->od);
    if (status != BIGRATIONAL_OK) goto cleanup;
    status = bigrational_sub(work, work, coefficient);
    if (status != BIGRATIONAL_OK) goto cleanup;
    status = set_coefficient(coefficient, b->sn, b->sd);
    if (status != BIGRATIONAL_OK) goto cleanup;
    status = bigrational_div(work, work, coefficient);
    if (status == BIGRATIONAL_OK) status = bigrational_copy(result, work);
cleanup:
    bigrational_destroy(work);
    bigrational_destroy(coefficient);
    return map_status(status);
}
NumForgeUnitStatus numforge_unit_convert_rational(BigRational *result,
    const BigRational *value, const char *from, const char *to)
{
    if (result == NULL || value == NULL || from == NULL || to == NULL)
        return NUMFORGE_UNIT_NULL_ARGUMENT;
    const UnitDefinition *a = find_unit(from), *b = find_unit(to);
    if (a == NULL || b == NULL) return NUMFORGE_UNIT_UNKNOWN_UNIT;
    if (a->info.quantity != b->info.quantity) return NUMFORGE_UNIT_INCOMPATIBLE_UNITS;
    if (a->info.pi_power != b->info.pi_power) return NUMFORGE_UNIT_NON_RATIONAL_CONVERSION;
    return convert_coefficients(result, value, from, to);
}
static NumForgeUnitStatus map_decimal_status(BigDecimalStatus status)
{
    switch (status)
    {
        case BIGDECIMAL_OK: return NUMFORGE_UNIT_OK;
        case BIGDECIMAL_OUT_OF_MEMORY: return NUMFORGE_UNIT_OUT_OF_MEMORY;
        case BIGDECIMAL_VALUE_TOO_LARGE: return NUMFORGE_UNIT_VALUE_TOO_LARGE;
        case BIGDECIMAL_SCALE_OVERFLOW: return NUMFORGE_UNIT_SCALE_OVERFLOW;
        default: return NUMFORGE_UNIT_INVALID_ARGUMENT;
    }
}
/* The coefficient stays exact until projection. Pi is computed at requested
 * precision + 24; final rounding is done once on the guarded approximation.
 * This follows the existing approximate-function contract, not a certified
 * error bound. Only exponent -1/+1 is needed by the current angle registry. */
static NumForgeUnitStatus project_angle(BigDecimal *result,
    const BigRational *exact, int pi_power, int64_t digits,
    BigDecimalRoundingMode rounding)
{
    if (digits > INT64_MAX - 24) return NUMFORGE_UNIT_VALUE_TOO_LARGE;
    BigDecimal *work = bigdecimal_create(), *pi = bigdecimal_create();
    BigDecimal *one = bigdecimal_create();
    NumForgeUnitStatus status = NUMFORGE_UNIT_OUT_OF_MEMORY;
    if (work == NULL || pi == NULL || one == NULL) goto cleanup;
    status = map_status(bigrational_to_bigdecimal(work, exact, digits + 24, BIGDECIMAL_ROUND_HALF_EVEN));
    if (status != NUMFORGE_UNIT_OK) goto cleanup;
    bool zero = false;
    status = map_decimal_status(bigdecimal_is_zero(&zero, work));
    if (status != NUMFORGE_UNIT_OK) goto cleanup;
    if (zero)
    {
        status = map_decimal_status(bigdecimal_copy(result, work));
        goto cleanup;
    }
    status = map_decimal_status(bigdecimal_set_constant_significant(
        pi, BIGDECIMAL_CONSTANT_PI, digits + 24, BIGDECIMAL_ROUND_HALF_EVEN));
    if (status != NUMFORGE_UNIT_OK) goto cleanup;
    if (pi_power > 0) status = map_decimal_status(bigdecimal_mul(work, work, pi));
    else status = map_decimal_status(bigdecimal_div_significant(
        work, work, pi, digits + 24, BIGDECIMAL_ROUND_HALF_EVEN));
    if (status != NUMFORGE_UNIT_OK) goto cleanup;
    status = map_decimal_status(bigdecimal_set_string(one, "1"));
    if (status == NUMFORGE_UNIT_OK) status = map_decimal_status(
        bigdecimal_div_significant(result, work, one, digits, rounding));
cleanup:
    bigdecimal_destroy(work);
    bigdecimal_destroy(pi);
    bigdecimal_destroy(one);
    return status;
}
NumForgeUnitStatus numforge_unit_convert_decimal(BigDecimal *result,
    const BigDecimal *value, const char *from, const char *to,
    int64_t digits, BigDecimalRoundingMode rounding)
{
    if (result == NULL || value == NULL || from == NULL || to == NULL)
        return NUMFORGE_UNIT_NULL_ARGUMENT;
    if (digits <= 0 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO ||
        rounding > BIGDECIMAL_ROUND_HALF_EVEN) return NUMFORGE_UNIT_INVALID_ARGUMENT;
    const UnitDefinition *a = find_unit(from), *b = find_unit(to);
    if (a == NULL || b == NULL) return NUMFORGE_UNIT_UNKNOWN_UNIT;
    if (!numforge_units_compatible(from, to)) return NUMFORGE_UNIT_INCOMPATIBLE_UNITS;
    BigRational *exact = bigrational_create();
    if (exact == NULL) return NUMFORGE_UNIT_OUT_OF_MEMORY;
    NumForgeUnitStatus status = map_status(bigrational_from_bigdecimal(exact, value));
    if (status == NUMFORGE_UNIT_OK)
        status = convert_coefficients(exact, exact, from, to);
    if (status == NUMFORGE_UNIT_OK)
    {
        if (a->info.pi_power == b->info.pi_power)
            status = map_status(bigrational_to_bigdecimal(result, exact, digits, rounding));
        else status = project_angle(result, exact, a->info.pi_power - b->info.pi_power, digits, rounding);
    }
    bigrational_destroy(exact);
    return status;
}
