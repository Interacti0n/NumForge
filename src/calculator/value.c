#include "../internal/benchmark_profile.h"
#include "value_internal.h"
#include "formatter.h"
#include "quantity.h"
#include "exact_functions.h"

#include <numforge/runtime.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Typed value ownership, exact projection, and output selection. */
bool calculator_value_is_complex(const CalculatorValue *value)
{
    return value != NULL && (value->kind == CALCULATOR_VALUE_COMPLEX_DECIMAL ||
        value->kind == CALCULATOR_VALUE_COMPLEX_RATIONAL);
}
CalculatorStatus calculator_value_complex_parts_text(const CalculatorValue *value, char **real, char **imaginary)
{
    if (value == NULL || real == NULL || imaginary == NULL) return CALCULATOR_NULL_ARGUMENT;
    if (!calculator_value_is_complex(value) || real == imaginary) return CALCULATOR_INVALID_ARGUMENT;
    char *re=NULL,*im=NULL;
    CalculatorStatus status=CALCULATOR_OUT_OF_MEMORY;
    if (value->kind == CALCULATOR_VALUE_COMPLEX_RATIONAL) {
        BigRational *part=bigrational_create();
        if (part != NULL) {
            status=calculator_from_complex_status(bigrationalcomplex_get_real(part,value->complex_rational));
            if (status==CALCULATOR_OK) status=calculator_from_rational_status(bigrational_to_string(part,&re));
            if (status==CALCULATOR_OK) status=calculator_from_complex_status(bigrationalcomplex_get_imaginary(part,value->complex_rational));
            if (status==CALCULATOR_OK) status=calculator_from_rational_status(bigrational_to_string(part,&im));
        }
        bigrational_destroy(part);
    } else {
        BigDecimal *part=bigdecimal_create();
        if (part != NULL) {
            status=calculator_from_complex_status(bigcomplex_get_real(part,value->complex_decimal));
            if (status==CALCULATOR_OK) status=calculator_from_decimal_status(bigdecimal_format_mode(part,-1,value->context.rounding,BIGDECIMAL_FORMAT_SCIENTIFIC,CALCULATOR_MAX_OUTPUT_BYTES,&re));
            if (status==CALCULATOR_OK) status=calculator_from_complex_status(bigcomplex_get_imaginary(part,value->complex_decimal));
            if (status==CALCULATOR_OK) status=calculator_from_decimal_status(bigdecimal_format_mode(part,-1,value->context.rounding,BIGDECIMAL_FORMAT_SCIENTIFIC,CALCULATOR_MAX_OUTPUT_BYTES,&im));
        }
        bigdecimal_destroy(part);
    }
    if (status == CALCULATOR_OK && (strlen(re)>CALCULATOR_MAX_OUTPUT_BYTES || strlen(im)>CALCULATOR_MAX_OUTPUT_BYTES)) status=CALCULATOR_VALUE_TOO_LARGE;
    if (status == CALCULATOR_OK) {*real=re;*imaginary=im;} else {free(re);free(im);}
    return calculator_budget_status(status);
}
CalculatorStatus calculator_value_complex_expression_text(const CalculatorValue *value,char **result)
{
    if (result == NULL) return CALCULATOR_NULL_ARGUMENT;
    char *re=NULL,*im=NULL;
    CalculatorStatus status=calculator_value_complex_parts_text(value,&re,&im);
    if (status == CALCULATOR_OK) {
        size_t a=strlen(re),b=strlen(im);
        if (a>CALCULATOR_MAX_INPUT_BYTES || b>CALCULATOR_MAX_INPUT_BYTES || a+b+10U>CALCULATOR_MAX_INPUT_BYTES) status=CALCULATOR_VALUE_TOO_LARGE;
        else {
            char *text=numforge_malloc(a+b+11U);
            if (text == NULL) status=CALCULATOR_OUT_OF_MEMORY;
            else {(void)snprintf(text,a+b+11U,"complex(%s;%s)",re,im);*result=text;}
        }
    }
    free(re);free(im);return calculator_budget_status(status);
}

CalculatorStatus calculator_value_snapshot_text(const CalculatorValue *value, char **result)
{
    if (value == NULL || result == NULL) return CALCULATOR_NULL_ARGUMENT;
    *result = NULL;
    CalculatorStatus status = CALCULATOR_OK;
    if (value->kind == CALCULATOR_VALUE_COMPLEX_RATIONAL) {
        BigComplexStatus converted = bigrationalcomplex_to_string(value->complex_rational, CALCULATOR_MAX_OUTPUT_BYTES, result);
        if (converted != BIGCOMPLEX_OK) status = converted == BIGCOMPLEX_OUT_OF_MEMORY ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_VALUE_TOO_LARGE;
    } else if (value->kind == CALCULATOR_VALUE_COMPLEX_DECIMAL) {
        BigComplexStatus converted = bigcomplex_format(value->complex_decimal, -1, value->context.rounding,
            BIGDECIMAL_FORMAT_SCIENTIFIC, CALCULATOR_MAX_OUTPUT_BYTES, result);
        if (converted != BIGCOMPLEX_OK) status = converted == BIGCOMPLEX_OUT_OF_MEMORY ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_VALUE_TOO_LARGE;
    } else if (value->kind == CALCULATOR_VALUE_INTEGER) {
        *result = bigint_to_string(value->integer);
        if (*result == NULL) status = CALCULATOR_OUT_OF_MEMORY;
    } else if (value->kind == CALCULATOR_VALUE_RATIONAL) {
        BigRationalStatus converted = bigrational_to_string(value->rational, result);
        if (converted != BIGRATIONAL_OK) status = converted == BIGRATIONAL_OUT_OF_MEMORY ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_VALUE_TOO_LARGE;
    } else {
        BigDecimalStatus converted = bigdecimal_format_mode(value->number, -1, value->context.rounding,
            BIGDECIMAL_FORMAT_SCIENTIFIC, CALCULATOR_MAX_OUTPUT_BYTES, result);
        if (converted != BIGDECIMAL_OK) status = converted == BIGDECIMAL_OUT_OF_MEMORY ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_VALUE_TOO_LARGE;
    }
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK && strlen(*result) > CALCULATOR_MAX_OUTPUT_BYTES) status = CALCULATOR_VALUE_TOO_LARGE;
    if (status != CALCULATOR_OK) { free(*result); *result = NULL; }
    return status;
}

void calculator_value_destroy(CalculatorValue *value)
{
    if (value != NULL)
    {
        bigdecimal_destroy(value->number);
        bigint_destroy(value->integer);
        bigrational_destroy(value->rational);
        bigcomplex_destroy(value->complex_decimal);
        bigrationalcomplex_destroy(value->complex_rational);
        memset(value, 0, sizeof(*value));
    }
}

CalculatorStatus calculator_value_copy(CalculatorValue *result, const CalculatorValue *value)
{
    CalculatorValue temporary = {0};
    if (result == NULL || value == NULL)
    {
        return CALCULATOR_NULL_ARGUMENT;
    }
    temporary.number = bigdecimal_create();
    if (temporary.number == NULL || bigdecimal_copy(temporary.number, value->number) != BIGDECIMAL_OK)
    {
        goto failed;
    }
    if (value->integer != NULL)
    {
        temporary.integer = bigint_create();
        if (temporary.integer == NULL || bigint_copy(temporary.integer, value->integer) != BIGINT_OK)
        {
            goto failed;
        }
    }
    if (value->rational != NULL)
    {
        temporary.rational = bigrational_create();
        if (temporary.rational == NULL || bigrational_copy(temporary.rational, value->rational) != BIGRATIONAL_OK)
        {
            goto failed;
        }
    }
    if (value->complex_decimal != NULL) {
        temporary.complex_decimal = bigcomplex_create();
        if (temporary.complex_decimal == NULL || bigcomplex_copy(temporary.complex_decimal, value->complex_decimal) != BIGCOMPLEX_OK) goto failed;
    }
    if (value->complex_rational != NULL) {
        temporary.complex_rational = bigrationalcomplex_create();
        if (temporary.complex_rational == NULL || bigrationalcomplex_copy(temporary.complex_rational, value->complex_rational) != BIGCOMPLEX_OK) goto failed;
    }
    temporary.context = value->context;
    temporary.kind = value->kind;
    temporary.quantity = value->quantity;
    temporary.temperature_point = value->temperature_point;
    memcpy(temporary.dimensions, value->dimensions, sizeof(temporary.dimensions));
    memcpy(temporary.unit, value->unit, sizeof(temporary.unit));
    temporary.independent = value->independent;
    temporary.uses_answer = value->uses_answer;
    temporary.uses_random = value->uses_random;
    temporary.uses_variables = value->uses_variables;
    calculator_value_destroy(result);
    *result = temporary;
    return CALCULATOR_OK;
failed:
    calculator_value_destroy(&temporary);
    return CALCULATOR_OUT_OF_MEMORY;
}

CalculatorStatus calculator_materialize_exact(
    BigDecimal *result, const CalculatorValue *value, const CalculatorContext *context)
{
    if (calculator_value_is_complex(value)) return CALCULATOR_INVALID_ARGUMENT;
    if (value->kind == CALCULATOR_VALUE_INTEGER)
    {
        BigDecimalStatus decimal_status = bigdecimal_from_bigint(result, value->integer);
        return decimal_status == BIGDECIMAL_OK ? CALCULATOR_OK :
            decimal_status == BIGDECIMAL_OUT_OF_MEMORY ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_VALUE_TOO_LARGE;
    }
    {
        BigRationalStatus rational_status = bigrational_to_bigdecimal(
            result, value->rational, context->division_scale, context->rounding);
        return rational_status == BIGRATIONAL_OK ? CALCULATOR_OK :
            rational_status == BIGRATIONAL_OUT_OF_MEMORY ? CALCULATOR_OUT_OF_MEMORY :
            rational_status == BIGRATIONAL_SCALE_OVERFLOW ? CALCULATOR_SCALE_OVERFLOW :
            rational_status == BIGRATIONAL_INVALID_ARGUMENT ? CALCULATOR_INVALID_ARGUMENT :
            CALCULATOR_VALUE_TOO_LARGE;
    }
}

static CalculatorStatus calculator_format_exact_text(
    const CalculatorValue *value, const CalculatorContext *context, char **result, bool *too_long)
{
    bool owner = numforge_budget_begin(
        (uint64_t)context->time_limit_ms, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    CalculatorStatus status;

    if (too_long != NULL)
    {
        *too_long = false;
    }
    if (value->kind == CALCULATOR_VALUE_INTEGER)
    {
        *result = bigint_to_string(value->integer);
        status = *result == NULL ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_OK;
    }
    else
    {
        BigRationalStatus rational_status = bigrational_to_string(value->rational, result);
        status = rational_status == BIGRATIONAL_OK ? CALCULATOR_OK :
            rational_status == BIGRATIONAL_OUT_OF_MEMORY ? CALCULATOR_OUT_OF_MEMORY :
            rational_status == BIGRATIONAL_VALUE_TOO_LARGE ? CALCULATOR_VALUE_TOO_LARGE :
            CALCULATOR_INVALID_ARGUMENT;
    }
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK && strlen(*result) > CALCULATOR_MAX_OUTPUT_BYTES)
    {
        status = CALCULATOR_VALUE_TOO_LARGE;
        if (too_long != NULL)
        {
            *too_long = true;
        }
    }
    if (status != CALCULATOR_OK)
    {
        free(*result);
        *result = NULL;
    }
    if (owner)
    {
        numforge_budget_end();
    }
    return status;
}

static CalculatorStatus calculator_fraction_candidate(
    const BigRational *value, const CalculatorContext *context, bool *candidate)
{
    bool owner = numforge_budget_begin(
        (uint64_t)context->time_limit_ms, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    BigInt *numerator = bigint_create();
    BigInt *denominator = bigint_create();
    BigInt *limit = bigint_create();
    BigInt *negative_limit = bigint_create();
    CalculatorStatus status = numerator == NULL || denominator == NULL || limit == NULL ||
        negative_limit == NULL ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_OK;

    *candidate = false;
    if (status == CALCULATOR_OK)
    {
        status = bigrational_get_numerator(numerator, value) == BIGRATIONAL_OK &&
            bigrational_get_denominator(denominator, value) == BIGRATIONAL_OK ?
            CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK)
    {
        status = bigint_set_string(limit, "99999999999999") == BIGINT_OK &&
            bigint_set_string(negative_limit, "-99999999999999") == BIGINT_OK ?
            CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK)
    {
        *candidate = bigint_compare(numerator, limit) <= 0 &&
            bigint_compare(numerator, negative_limit) >= 0 &&
            bigint_compare(denominator, limit) <= 0;
    }
    if (status == CALCULATOR_OK && *candidate && context->notation == CALCULATOR_NOTATION_AUTO)
    {
        status = bigint_set_string(limit, "10000") == BIGINT_OK ?
            CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
        if (status == CALCULATOR_OK)
        {
            *candidate = bigint_compare(denominator, limit) <= 0;
        }
    }
    bigint_destroy(numerator);
    bigint_destroy(denominator);
    bigint_destroy(limit);
    bigint_destroy(negative_limit);
    status = calculator_budget_status(status);
    if (owner)
    {
        numforge_budget_end();
    }
    return status;
}

static CalculatorStatus calculator_format_value_profile_impl(
    const CalculatorValue *value, const CalculatorContext *context, char **result)
{
    BigDecimal *temporary;
    CalculatorStatus status;
    if (result != NULL)
    {
        *result = NULL;
    }
    if (value == NULL || context == NULL || result == NULL)
    {
        return CALCULATOR_NULL_ARGUMENT;
    }
    if (context->time_limit_ms < 0 || context->notation < CALCULATOR_NOTATION_AUTO ||
        context->notation > CALCULATOR_NOTATION_FRACTION || context->complex_form < BIGCOMPLEX_FORM_CARTESIAN ||
        context->complex_form > BIGCOMPLEX_FORM_EXPONENTIAL)
    {
        return CALCULATOR_INVALID_ARGUMENT;
    }
    if (calculator_value_is_complex(value)) {
        BigDecimalFormatMode mode = context->notation == CALCULATOR_NOTATION_FRACTION ? BIGDECIMAL_FORMAT_AUTO : (BigDecimalFormatMode)context->notation;
        BigComplexStatus formatted;
        if (context->complex_form != BIGCOMPLEX_FORM_CARTESIAN) {
            if (value->kind == CALCULATOR_VALUE_COMPLEX_RATIONAL)
                formatted=bigrationalcomplex_format_form(value->complex_rational,context->complex_form,context->division_scale,
                    context->output_scale,context->rounding,mode,CALCULATOR_MAX_OUTPUT_BYTES,result);
            else formatted=bigcomplex_format_form(value->complex_decimal,context->complex_form,context->division_scale,
                context->output_scale,context->rounding,mode,CALCULATOR_MAX_OUTPUT_BYTES,result);
        } else if (value->kind == CALCULATOR_VALUE_COMPLEX_RATIONAL &&
            (context->notation == CALCULATOR_NOTATION_AUTO || context->notation == CALCULATOR_NOTATION_FRACTION))
            formatted = bigrationalcomplex_to_string(value->complex_rational, CALCULATOR_MAX_OUTPUT_BYTES, result);
        else if (value->kind == CALCULATOR_VALUE_COMPLEX_RATIONAL) {
            BigComplex *decimal = bigcomplex_create();
            if (decimal == NULL) return CALCULATOR_OUT_OF_MEMORY;
            formatted = bigrationalcomplex_to_bigcomplex(decimal, value->complex_rational, context->division_scale, context->rounding);
            if (formatted == BIGCOMPLEX_OK) formatted = bigcomplex_format(decimal, context->output_scale, context->rounding, mode, CALCULATOR_MAX_OUTPUT_BYTES, result);
            bigcomplex_destroy(decimal);
        } else formatted = bigcomplex_format(value->complex_decimal, context->output_scale, context->rounding, mode, CALCULATOR_MAX_OUTPUT_BYTES, result);
        return formatted == BIGCOMPLEX_OK ? CALCULATOR_OK : formatted == BIGCOMPLEX_OUT_OF_MEMORY ? CALCULATOR_OUT_OF_MEMORY :
            formatted == BIGCOMPLEX_INVALID_ARGUMENT ? CALCULATOR_INVALID_ARGUMENT : CALCULATOR_VALUE_TOO_LARGE;
    }
    if (context->notation == CALCULATOR_NOTATION_FRACTION &&
        value->kind == CALCULATOR_VALUE_INTEGER)
    {
        return calculator_format_exact_text(value, context, result, NULL);
    }
    if (context->notation == CALCULATOR_NOTATION_FRACTION &&
        value->kind == CALCULATOR_VALUE_RATIONAL)
    {
        bool candidate = false;
        status = calculator_fraction_candidate(value->rational, context, &candidate);
        if (status != CALCULATOR_OK)
        {
            return status;
        }
        if (candidate)
        {
            status = calculator_format_exact_text(value, context, result, NULL);
            if (status != CALCULATOR_OK)
            {
                return status;
            }
            if (strlen(*result) <= 16U)
            {
                return CALCULATOR_OK;
            }
            free(*result);
            *result = NULL;
        }
    }
    if (value->kind == CALCULATOR_VALUE_DECIMAL)
    {
        return calculator_format_result(value->number, context, result);
    }
    temporary = bigdecimal_create();
    if (temporary == NULL)
    {
        return calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
    }
    status = calculator_materialize_exact(temporary, value, context);
    if (status == CALCULATOR_OK)
    {
        status = calculator_format_result(temporary, context, result);
    }
    bigdecimal_destroy(temporary);
    if (status == CALCULATOR_OK && context->notation == CALCULATOR_NOTATION_AUTO &&
        value->kind == CALCULATOR_VALUE_RATIONAL)
    {
        bool candidate = false;
        const char *exponent_marker = strchr(*result, 'E');
        status = calculator_fraction_candidate(value->rational, context, &candidate);
        if (status == CALCULATOR_OK && candidate && exponent_marker != NULL)
        {
            char *end;
            long long exponent;
            errno = 0;
            exponent = strtoll(exponent_marker + 1U, &end, 10);
            if (errno != 0 || *end != '\0' || exponent < 0 ||
                (unsigned long long)exponent + 5U > strlen(*result))
            {
                candidate = false;
            }
        }
        if (status == CALCULATOR_OK && candidate)
        {
            char *fraction = NULL;
            bool too_long = false;
            status = calculator_format_exact_text(value, context, &fraction, &too_long);
            if (too_long)
            {
                status = CALCULATOR_OK;
            }
            if (status == CALCULATOR_OK)
            {
                if (fraction != NULL && strlen(fraction) <= 16U &&
                    strlen(fraction) + 2U <= strlen(*result))
                {
                    free(*result);
                    *result = fraction;
                }
                else
                {
                    free(fraction);
                }
            }
        }
        if (status != CALCULATOR_OK)
        {
            free(*result);
            *result = NULL;
        }
    }
    return status;
}

CalculatorStatus calculator_format_value(
    const CalculatorValue *value, const CalculatorContext *context, char **result)
{
    NumForgeProfilePhase previous = numforge_profile_enter(NUMFORGE_PHASE_FORMAT);
    bool owner = context != NULL && context->time_limit_ms >= 0 && numforge_budget_begin(
        (uint64_t)context->time_limit_ms, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    CalculatorStatus result_status = calculator_format_value_profile_impl(value, context, result);
    if (result_status == CALCULATOR_OK && value->quantity)
    {
        char symbol[192] = {0};
        result_status = calculator_quantity_symbol(value, symbol, sizeof(symbol));
        size_t length = strlen(*result), unit_length = strlen(symbol);
        if (result_status == CALCULATOR_OK && length + unit_length + 1U > CALCULATOR_MAX_OUTPUT_BYTES)
            result_status = CALCULATOR_VALUE_TOO_LARGE;
        if (result_status == CALCULATOR_OK)
        {
            char *joined = numforge_malloc(length + unit_length + 2U);
            if (joined == NULL) result_status = CALCULATOR_OUT_OF_MEMORY;
            else
            {
                memcpy(joined, *result, length);
                joined[length] = ' ';
                memcpy(joined + length + 1U, symbol, unit_length + 1U);
                free(*result);
                *result = joined;
            }
        }
        if (result_status != CALCULATOR_OK) { free(*result); *result = NULL; }
    }
    result_status = calculator_budget_status(result_status);
    if (result_status != CALCULATOR_OK && result != NULL) { free(*result); *result = NULL; }
    if (owner) numforge_budget_end();
    numforge_profile_leave(previous);
    return result_status;
}

bool calculator_value_matches(const CalculatorValue *value, const CalculatorContext *context)
{
    return value != NULL && value->number != NULL && context != NULL &&
           (value->kind == CALCULATOR_VALUE_DECIMAL ||
            value->context.significant_division == context->significant_division) &&
           value->context.angle_unit == context->angle_unit &&
           value->context.rounding == context->rounding &&
           (value->independent ||
            (value->context.division_scale == context->division_scale &&
             value->context.significant_division == context->significant_division));
}
