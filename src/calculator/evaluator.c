#include "evaluator_internal.h"
#include "expression_internal.h"
#include "unit_status.h"
#include "random.h"
#include "exact_evaluator.h"
#include "value_internal.h"
#include "complex_evaluator.h"

#include <numforge/bigint.h>
#include <numforge/runtime.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define CALCULATOR_STRINGIFY_VALUE(value) #value
#define CALCULATOR_STRINGIFY(value) CALCULATOR_STRINGIFY_VALUE(value)

/*
------------------------------------------------------------------------------------------------------------------------------
    Evaluator implementation. It recursively evaluates AST nodes into temporary
    BigDecimal values, then copies a final successful value to the caller's
    destination. The caller's result therefore remains unchanged on failure.
------------------------------------------------------------------------------------------------------------------------------
*/

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal helper functions for evaluator operations.
------------------------------------------------------------------------------------------------------------------------------
*/

static CalculatorStatus calculator_evaluate_random_call(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
)
{
    BigDecimal *lower = NULL;
    BigDecimal *upper = NULL;
    BigDecimal *unit = NULL;
    BigDecimal *span = NULL;
    BigDecimal *value = NULL;
    CalculatorStatus status = CALCULATOR_OK;
    int comparison = 0;
    char digits[37];
    size_t count = expression->data.call.count;

    lower = bigdecimal_create();
    upper = bigdecimal_create();
    unit = bigdecimal_create();
    span = bigdecimal_create();
    value = bigdecimal_create();
    if (lower == NULL || upper == NULL || unit == NULL || span == NULL || value == NULL)
    {
        status = CALCULATOR_OUT_OF_MEMORY;
        goto done;
    }
    if (count == 2U)
    {
        BigDecimal *argument = NULL;
        status = calculator_evaluate_expression(&argument, expression->data.call.arguments[0], evaluation, error);
        if (status == CALCULATOR_OK)
        {
            status = calculator_from_bigdecimal_status(bigdecimal_copy(lower, argument));
        }
        bigdecimal_destroy(argument);
    }
    if (status == CALCULATOR_OK && count >= 1U)
    {
        BigDecimal *argument = NULL;
        status = calculator_evaluate_expression(&argument, expression->data.call.arguments[count - 1U], evaluation, error);
        if (status == CALCULATOR_OK)
        {
            status = calculator_from_bigdecimal_status(bigdecimal_copy(upper, argument));
        }
        bigdecimal_destroy(argument);
    }
    if (status == CALCULATOR_OK && count == 0U)
    {
        status = calculator_from_bigdecimal_status(bigdecimal_set_string(upper, "1"));
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(bigdecimal_compare(&comparison, lower, upper));
    }
    if (status == CALCULATOR_OK && comparison >= 0)
    {
        status = CALCULATOR_INVALID_ARGUMENT;
    }
    if (status == CALCULATOR_OK)
    {
        calculator_random_decimal(evaluation->random_state, digits);
        status = calculator_from_bigdecimal_status(bigdecimal_set_string(unit, digits));
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(bigdecimal_sub(span, upper, lower));
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(bigdecimal_mul(value, span, unit));
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(bigdecimal_add(value, lower, value));
    }
done:
    bigdecimal_destroy(lower);
    bigdecimal_destroy(upper);
    bigdecimal_destroy(unit);
    bigdecimal_destroy(span);
    if (status == CALCULATOR_OK)
    {
        *result = value;
    }
    else
    {
        bigdecimal_destroy(value);
        if (error == NULL || error->status != status)
        {
            calculator_error_set(error, status, expression->offset);
        }
    }
    return status;
}

bool calculator_time_limit_reached(
    const CalculatorEvaluation *evaluation
)
{
    (void)evaluation;

    return !numforge_budget_check() && numforge_budget_failure() == NUMFORGE_BUDGET_TIME;
}

CalculatorStatus calculator_from_bigdecimal_status(
    BigDecimalStatus status
)
{
    switch (status)
    {
        case BIGDECIMAL_OK:
            return CALCULATOR_OK;
        case BIGDECIMAL_NULL_ARGUMENT:
            return CALCULATOR_NULL_ARGUMENT;
        case BIGDECIMAL_OUT_OF_MEMORY:
            return CALCULATOR_OUT_OF_MEMORY;
        case BIGDECIMAL_DIVISION_BY_ZERO:
            return CALCULATOR_DIVISION_BY_ZERO;
        case BIGDECIMAL_VALUE_TOO_LARGE:
            return CALCULATOR_VALUE_TOO_LARGE;
        case BIGDECIMAL_SCALE_OVERFLOW:
            return CALCULATOR_SCALE_OVERFLOW;
        default:
            return CALCULATOR_INVALID_ARGUMENT;
    }
}

CalculatorStatus calculator_from_bigint_status(
    BigIntStatus status
)
{
    switch (status)
    {
        case BIGINT_OK:
            return CALCULATOR_OK;
        case BIGINT_NULL_ARGUMENT:
            return CALCULATOR_NULL_ARGUMENT;
        case BIGINT_OUT_OF_MEMORY:
            return CALCULATOR_OUT_OF_MEMORY;
        case BIGINT_VALUE_TOO_LARGE:
            return CALCULATOR_VALUE_TOO_LARGE;
        case BIGINT_DIVISION_BY_ZERO:
            return CALCULATOR_DIVISION_BY_ZERO;
        case BIGINT_NEGATIVE_ARGUMENT:
        case BIGINT_INVALID_ARGUMENT:
        default:
            return CALCULATOR_INVALID_ARGUMENT;
    }
}

static bool calculator_valid_rounding(
    BigDecimalRoundingMode rounding
)
{
    return rounding >= BIGDECIMAL_ROUND_TOWARD_ZERO && rounding <= BIGDECIMAL_ROUND_HALF_EVEN;
}

static CalculatorStatus calculator_set_number(
    BigDecimal *value,
    const char *text
)
{
    BigDecimalStatus decimal_status;
    const char *separator = strchr(text, ',');

    if (separator == NULL)
    {
        return calculator_from_bigdecimal_status(bigdecimal_set_string(value, text));
    }

    {
        size_t length = strlen(text);
        char *normalized = numforge_malloc(length + 1U);

        if (normalized == NULL)
        {
            return CALCULATOR_OUT_OF_MEMORY;
        }

        memcpy(normalized, text, length + 1U);
        normalized[separator - text] = '.';
        decimal_status = bigdecimal_set_string(value, normalized);
        free(normalized);
    }

    return calculator_from_bigdecimal_status(decimal_status);
}

/* Domain checks use public predicates, never numeric representation fields. */

bool calculator_nonnegative_integer(
    const BigDecimal *value
)
{
    bool integer = false;
    int sign = 0;

    return bigdecimal_is_integer(&integer, value) == BIGDECIMAL_OK &&
           bigdecimal_sign(&sign, value) == BIGDECIMAL_OK && integer && sign >= 0;
}

CalculatorStatus calculator_bigdecimal_to_bigint(
    BigInt **result,
    const BigDecimal *value,
    bool signed_input
)
{
    if (!signed_input && !calculator_nonnegative_integer(value))
    {
        return CALCULATOR_INVALID_ARGUMENT;
    }

    BigInt *integer = bigint_create();

    if (integer == NULL)
    {
        return CALCULATOR_OUT_OF_MEMORY;
    }

    CalculatorStatus status = calculator_from_bigdecimal_status(bigdecimal_to_bigint(integer, value));

    if (status != CALCULATOR_OK)
    {
        bigint_destroy(integer);
    }
    else
    {
        *result = integer;
    }

    return status;
}

CalculatorStatus calculator_set_bigdecimal_from_bigint(
    BigDecimal *value,
    const BigInt *integer
)
{
    return calculator_from_bigdecimal_status(bigdecimal_from_bigint(value, integer));
}

static CalculatorStatus calculator_check_factorial_limit(
    const BigDecimal *value
)
{
    BigDecimal *limit;
    CalculatorStatus status;
    int comparison = 0;

    if (!calculator_nonnegative_integer(value))
    {
        return CALCULATOR_INVALID_ARGUMENT;
    }

    limit = bigdecimal_create();

    if (limit == NULL)
    {
        return CALCULATOR_OUT_OF_MEMORY;
    }

    status = calculator_from_bigdecimal_status(
        bigdecimal_set_string(limit, CALCULATOR_STRINGIFY(CALCULATOR_FACTORIAL_MAX_N)));

    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(bigdecimal_compare(&comparison, value, limit));
    }

    if (status == CALCULATOR_OK && comparison > 0)
    {
        status = CALCULATOR_VALUE_TOO_LARGE;
    }

    bigdecimal_destroy(limit);

    return status;
}

static CalculatorStatus calculator_bigdecimal_pow(
    BigDecimal *result,
    const BigDecimal *base,
    const BigDecimal *exponent,
    const CalculatorEvaluation *evaluation
)
{
    BigInt *integer = NULL;
    CalculatorStatus status = calculator_bigdecimal_to_bigint(&integer, exponent, true);

    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(
            bigdecimal_pow_signed(
                result,
                base,
                integer,
                evaluation->context->division_scale,
                evaluation->context->rounding));
    }

    bigint_destroy(integer);

    return status;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Postfix operations: square, cube, and factorial.
------------------------------------------------------------------------------------------------------------------------------
*/

static CalculatorStatus calculator_evaluate_postfix(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
)
{
    BigDecimal *operand;
    BigDecimal *value;
    BigInt *integer = NULL;
    BigInt *integer_result = NULL;
    CalculatorStatus status =
        calculator_evaluate_expression(&operand, expression->data.postfix.operand, evaluation, error);

    if (status != CALCULATOR_OK)
    {
        return status;
    }

    if (expression->data.postfix.operation == CALCULATOR_POSTFIX_SQUARE ||
        expression->data.postfix.operation == CALCULATOR_POSTFIX_CUBE)
    {
        value = bigdecimal_create();

        if (value == NULL)
        {
            bigdecimal_destroy(operand);
            calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);

            return CALCULATOR_OUT_OF_MEMORY;
        }

        status = calculator_from_bigdecimal_status(bigdecimal_mul(value, operand, operand));

        if (status == CALCULATOR_OK && expression->data.postfix.operation == CALCULATOR_POSTFIX_CUBE)
        {
            status = calculator_from_bigdecimal_status(bigdecimal_mul(value, value, operand));
        }

        bigdecimal_destroy(operand);

        if (status == CALCULATOR_OK && calculator_time_limit_reached(evaluation))
        {
            status = CALCULATOR_TIME_LIMIT;
        }

        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);

            return status;
        }

        *result = value;

        return CALCULATOR_OK;
    }

    if (expression->data.postfix.operation != CALCULATOR_POSTFIX_FACTORIAL)
    {
        bigdecimal_destroy(operand);
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, expression->offset);

        return CALCULATOR_INVALID_ARGUMENT;
    }

    status = calculator_check_factorial_limit(operand);

    if (status == CALCULATOR_OK)
    {
        status = calculator_bigdecimal_to_bigint(&integer, operand, false);
    }

    bigdecimal_destroy(operand);

    if (status != CALCULATOR_OK)
    {
        calculator_error_set(error, status, expression->offset);

        return status;
    }

    integer_result = bigint_create();

    if (integer_result == NULL)
    {
        bigint_destroy(integer);
        calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);

        return CALCULATOR_OUT_OF_MEMORY;
    }

    status = calculator_from_bigint_status(bigint_factorial(integer_result, integer));
    bigint_destroy(integer);

    if (status == CALCULATOR_OK && calculator_time_limit_reached(evaluation))
    {
        status = CALCULATOR_TIME_LIMIT;
    }

    if (status != CALCULATOR_OK)
    {
        bigint_destroy(integer_result);
        calculator_error_set(error, status, expression->offset);

        return status;
    }

    value = bigdecimal_create();

    if (value == NULL)
    {
        bigint_destroy(integer_result);
        calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);

        return CALCULATOR_OUT_OF_MEMORY;
    }

    status = calculator_set_bigdecimal_from_bigint(value, integer_result);
    bigint_destroy(integer_result);

    if (status != CALCULATOR_OK)
    {
        bigdecimal_destroy(value);
        calculator_error_set(error, status, expression->offset);

        return status;
    }

    *result = value;

    return CALCULATOR_OK;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Recursive expression evaluation and function dispatch.
------------------------------------------------------------------------------------------------------------------------------
*/

CalculatorStatus calculator_evaluate_expression(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
)
{
    BigDecimal *value;
    CalculatorStatus status;

    if (calculator_time_limit_reached(evaluation))
    {
        calculator_error_set(error, CALCULATOR_TIME_LIMIT, expression->offset);

        return CALCULATOR_TIME_LIMIT;
    }

    if (evaluation->context->significant_division &&
        (expression->type == CALCULATOR_EXPRESSION_BINARY ||
         expression->type == CALCULATOR_EXPRESSION_POSTFIX ||
         expression->type == CALCULATOR_EXPRESSION_CALL ||
         (expression->type == CALCULATOR_EXPRESSION_UNARY &&
          expression->data.unary.operand->type == CALCULATOR_EXPRESSION_BINARY)) &&
        calculator_exact_supported(expression, evaluation->typed_answer))
    {
        BigRational *exact = NULL;
        value = bigdecimal_create();
        status = value == NULL ? CALCULATOR_OUT_OF_MEMORY :
            calculator_evaluate_exact(&exact, expression, evaluation->typed_answer, error);
        if (status == CALCULATOR_NOT_IMPLEMENTED)
        {
            bigrational_destroy(exact);
            bigdecimal_destroy(value);
            calculator_error_clear(error);
        }
        else
        {
            if (status == CALCULATOR_OK)
            {
                BigRationalStatus rational_status = bigrational_to_bigdecimal(
                    value, exact, evaluation->context->division_scale, evaluation->context->rounding);
                status = rational_status == BIGRATIONAL_OK ? CALCULATOR_OK :
                    rational_status == BIGRATIONAL_OUT_OF_MEMORY ? CALCULATOR_OUT_OF_MEMORY :
                    rational_status == BIGRATIONAL_SCALE_OVERFLOW ? CALCULATOR_SCALE_OVERFLOW :
                    rational_status == BIGRATIONAL_VALUE_TOO_LARGE ? CALCULATOR_VALUE_TOO_LARGE :
                    CALCULATOR_INVALID_ARGUMENT;
            }
            bigrational_destroy(exact);
            if (status == CALCULATOR_OK)
            {
                *result = value;
                return CALCULATOR_OK;
            }
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);
            return status;
        }
    }

    if (expression->type == CALCULATOR_EXPRESSION_VARIABLE)
    {
        const CalculatorValue *stored = expression->data.variable.value;
        if (stored == NULL)
        {
            calculator_error_set(error, CALCULATOR_UNDEFINED_VARIABLE, expression->offset);
            return CALCULATOR_UNDEFINED_VARIABLE;
        }
        value = bigdecimal_create();
        status = value == NULL ? CALCULATOR_OUT_OF_MEMORY :
            stored->kind == CALCULATOR_VALUE_DECIMAL
                ? calculator_from_bigdecimal_status(bigdecimal_copy(value, stored->number))
                : calculator_materialize_exact(value, stored, evaluation->context);
        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);
            return status;
        }
        *result = value;
        return CALCULATOR_OK;
    }

    if (expression->type == CALCULATOR_EXPRESSION_ANSWER)
    {
        if (evaluation->answer == NULL)
        {
            calculator_error_set(error, CALCULATOR_UNDEFINED_ANSWER, expression->offset);
            return CALCULATOR_UNDEFINED_ANSWER;
        }

        value = bigdecimal_create();
        status = value == NULL ? CALCULATOR_OUT_OF_MEMORY :
            calculator_from_bigdecimal_status(bigdecimal_copy(value, evaluation->answer));
        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);
            return status;
        }

        *result = value;
        return CALCULATOR_OK;
    }

    if (expression->type == CALCULATOR_EXPRESSION_CALL)
    {
        /* Borrow children for the existing operator path; this temporary node
         * owns nothing and must never be passed to expression_destroy. */
        CalculatorExpression operation = {0};
        operation.offset = expression->offset;
        operation.depth = expression->depth;

        switch (expression->data.call.function->implementation)
        {
            case CALCULATOR_FUNCTION_CONVERT:
            {
                BigDecimal *input = NULL;
                value = bigdecimal_create();
                status = value == NULL ? CALCULATOR_OUT_OF_MEMORY :
                    calculator_evaluate_expression(&input, expression->data.call.arguments[0], evaluation, error);
                if (status == CALCULATOR_OK)
                    status = calculator_from_unit_status(numforge_unit_convert_decimal(value, input,
                        expression->data.call.from_unit, expression->data.call.to_unit,
                        evaluation->context->division_scale, evaluation->context->rounding));
                bigdecimal_destroy(input);
                if (status != CALCULATOR_OK)
                {
                    bigdecimal_destroy(value);
                    if (error == NULL || error->status != status)
                        calculator_error_set(error, status, expression->offset);
                    return status;
                }
                *result = value;
                return CALCULATOR_OK;
            }
            case CALCULATOR_FUNCTION_RANDOM:
                return calculator_evaluate_random_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_SQRT:
            case CALCULATOR_FUNCTION_CBRT:
            case CALCULATOR_FUNCTION_ROOT:
                return calculator_evaluate_root_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_EXP:
            case CALCULATOR_FUNCTION_LN:
            case CALCULATOR_FUNCTION_LOG:
                return calculator_evaluate_transcendental_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_SINH:
            case CALCULATOR_FUNCTION_COSH:
            case CALCULATOR_FUNCTION_TANH:
            case CALCULATOR_FUNCTION_ASINH:
            case CALCULATOR_FUNCTION_ACOSH:
            case CALCULATOR_FUNCTION_ATANH:
                return calculator_evaluate_hyperbolic_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_SIN:
            case CALCULATOR_FUNCTION_COS:
            case CALCULATOR_FUNCTION_TAN:
            case CALCULATOR_FUNCTION_ASIN:
            case CALCULATOR_FUNCTION_ACOS:
            case CALCULATOR_FUNCTION_ATAN:
            case CALCULATOR_FUNCTION_RADIANS:
            case CALCULATOR_FUNCTION_DEGREES:
                return calculator_evaluate_trigonometric_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_GCD:
            case CALCULATOR_FUNCTION_LCM:
            case CALCULATOR_FUNCTION_MOD:
            case CALCULATOR_FUNCTION_NPR:
            case CALCULATOR_FUNCTION_NCR:
            case CALCULATOR_FUNCTION_ISQRT:
                return calculator_evaluate_integer_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_ABS:
            case CALCULATOR_FUNCTION_SIGN:
            case CALCULATOR_FUNCTION_MIN:
            case CALCULATOR_FUNCTION_MAX:
                return calculator_evaluate_basic_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_SUM:
            case CALCULATOR_FUNCTION_PRODUCT:
            case CALCULATOR_FUNCTION_MEAN:
            case CALCULATOR_FUNCTION_MEDIAN:
            case CALCULATOR_FUNCTION_GEOMETRIC_MEAN:
            case CALCULATOR_FUNCTION_HARMONIC_MEAN:
            case CALCULATOR_FUNCTION_VARIANCE:
            case CALCULATOR_FUNCTION_STANDARD_DEVIATION_POPULATION:
            case CALCULATOR_FUNCTION_STANDARD_DEVIATION_SAMPLE:
                return calculator_evaluate_aggregate_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_FLOOR:
            case CALCULATOR_FUNCTION_CEIL:
            case CALCULATOR_FUNCTION_TRUNC:
            case CALCULATOR_FUNCTION_ROUND:
                return calculator_evaluate_rounding_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_POWER:
                operation.type = CALCULATOR_EXPRESSION_BINARY;
                operation.data.binary.operation = CALCULATOR_BINARY_POWER;
                operation.data.binary.left = expression->data.call.arguments[0];
                operation.data.binary.right = expression->data.call.arguments[1];
                break;
            case CALCULATOR_FUNCTION_FACTORIAL:
                operation.type = CALCULATOR_EXPRESSION_POSTFIX;
                operation.data.postfix.operation = CALCULATOR_POSTFIX_FACTORIAL;
                operation.data.postfix.operand = expression->data.call.arguments[0];
                break;
            default:
                calculator_error_set(error, CALCULATOR_NOT_IMPLEMENTED, expression->offset);

                return CALCULATOR_NOT_IMPLEMENTED;
        }

        return calculator_evaluate_expression(result, &operation, evaluation, error);
    }

    if (expression->type == CALCULATOR_EXPRESSION_NUMBER)
    {
        value = bigdecimal_create();

        if (value == NULL)
        {
            calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);

            return CALCULATOR_OUT_OF_MEMORY;
        }

        status = calculator_set_number(value, expression->data.number.text);

        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);

            return status;
        }

        *result = value;

        return CALCULATOR_OK;
    }

    if (expression->type == CALCULATOR_EXPRESSION_CONSTANT)
    {
        size_t constant_index = (size_t)expression->data.constant.constant;

        if (constant_index >= CALCULATOR_CONSTANT_COUNT)
        {
            calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, expression->offset);

            return CALCULATOR_INVALID_ARGUMENT;
        }

        if (evaluation->constant_cache->values[constant_index] == NULL ||
            evaluation->constant_cache->digits[constant_index] != evaluation->context->division_scale)
        {
            BigDecimal *constant = bigdecimal_create();

            if (constant == NULL)
            {
                calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);

                return CALCULATOR_OUT_OF_MEMORY;
            }

            status = calculator_constant_set_value(
                constant,
                expression->data.constant.constant,
                evaluation->context->division_scale,
                evaluation->context->rounding);

            if (status != CALCULATOR_OK)
            {
                bigdecimal_destroy(constant);
                calculator_error_set(error, status, expression->offset);

                return status;
            }

            bigdecimal_destroy(evaluation->constant_cache->values[constant_index]);
            evaluation->constant_cache->values[constant_index] = constant;
            evaluation->constant_cache->digits[constant_index] = evaluation->context->division_scale;
        }

        value = bigdecimal_create();

        if (value == NULL)
        {
            calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);

            return CALCULATOR_OUT_OF_MEMORY;
        }

        status = calculator_from_bigdecimal_status(
            bigdecimal_copy(value, evaluation->constant_cache->values[constant_index]));

        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);

            return status;
        }

        *result = value;

        return CALCULATOR_OK;
    }

    if (expression->type == CALCULATOR_EXPRESSION_UNARY)
    {
        BigDecimal *operand;

        status = calculator_evaluate_expression(&operand, expression->data.unary.operand, evaluation, error);

        if (status != CALCULATOR_OK)
        {
            return status;
        }

        if (expression->data.unary.operation == CALCULATOR_UNARY_PLUS)
        {
            *result = operand;

            return CALCULATOR_OK;
        }

        if (expression->data.unary.operation != CALCULATOR_UNARY_MINUS)
        {
            bigdecimal_destroy(operand);
            calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, expression->offset);

            return CALCULATOR_INVALID_ARGUMENT;
        }

        value = bigdecimal_create();

        if (value == NULL)
        {
            bigdecimal_destroy(operand);
            calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);

            return CALCULATOR_OUT_OF_MEMORY;
        }

        status = calculator_from_bigdecimal_status(bigdecimal_negate(value, operand));
        bigdecimal_destroy(operand);

        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);

            return status;
        }

        *result = value;

        return CALCULATOR_OK;
    }

    if (expression->type == CALCULATOR_EXPRESSION_POSTFIX)
    {
        return calculator_evaluate_postfix(result, expression, evaluation, error);
    }

    if (expression->type == CALCULATOR_EXPRESSION_BINARY)
    {
        BigDecimal *left;
        BigDecimal *right;

        status = calculator_evaluate_expression(&left, expression->data.binary.left, evaluation, error);

        if (status != CALCULATOR_OK)
        {
            return status;
        }

        status = calculator_evaluate_expression(&right, expression->data.binary.right, evaluation, error);

        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(left);

            return status;
        }

        value = bigdecimal_create();

        if (value == NULL)
        {
            bigdecimal_destroy(left);
            bigdecimal_destroy(right);
            calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);

            return CALCULATOR_OUT_OF_MEMORY;
        }

        switch (expression->data.binary.operation)
        {
            case CALCULATOR_BINARY_ADD:
                status = calculator_from_bigdecimal_status(bigdecimal_add(value, left, right));
                break;
            case CALCULATOR_BINARY_SUBTRACT:
                status = calculator_from_bigdecimal_status(bigdecimal_sub(value, left, right));
                break;
            case CALCULATOR_BINARY_MULTIPLY:
                status = calculator_from_bigdecimal_status(bigdecimal_mul(value, left, right));
                break;
            case CALCULATOR_BINARY_DIVIDE:
                status = calculator_from_bigdecimal_status(
                    evaluation->context->significant_division
                        ? bigdecimal_div_exact_or_significant(value,
                                                              left,
                                                              right,
                                                              evaluation->context->division_scale,
                                                              evaluation->context->rounding)
                        : bigdecimal_div(value,
                                         left,
                                         right,
                                         evaluation->context->division_scale,
                                         evaluation->context->rounding));
                break;
            case CALCULATOR_BINARY_POWER:
                status = calculator_bigdecimal_pow(value, left, right, evaluation);
                break;
            default:
                status = CALCULATOR_INVALID_ARGUMENT;
                break;
        }

        if (status == CALCULATOR_OK && calculator_time_limit_reached(evaluation))
        {
            status = CALCULATOR_TIME_LIMIT;
        }

        bigdecimal_destroy(left);
        bigdecimal_destroy(right);

        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);

            return status;
        }

        *result = value;

        return CALCULATOR_OK;
    }

    calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, expression->offset);

    return CALCULATOR_INVALID_ARGUMENT;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Evaluator operation functions.
------------------------------------------------------------------------------------------------------------------------------
*/

static CalculatorStatus calculator_evaluate_impl(
    BigDecimal *result,
    const CalculatorExpression *expression,
    const CalculatorContext *context,
    const BigDecimal *answer,
    const CalculatorValue *typed_answer,
    uint64_t *random_state,
    CalculatorError *error
)
{
    CalculatorEvaluation evaluation;
    CalculatorConstantCache constant_cache;
    BigDecimal *temporary;
    CalculatorStatus status;

    if (result == NULL || expression == NULL || context == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0);

        return CALCULATOR_NULL_ARGUMENT;
    }

    if (!calculator_valid_rounding(context->rounding))
    {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0);

        return CALCULATOR_INVALID_ARGUMENT;
    }

    if (context->output_scale < CALCULATOR_UNLIMITED_OUTPUT_SCALE)
    {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0);

        return CALCULATOR_INVALID_ARGUMENT;
    }

    if (context->time_limit_ms < 0)
    {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0);

        return CALCULATOR_INVALID_ARGUMENT;
    }

    if (context->output_scale > CALCULATOR_MAX_OUTPUT_SCALE ||
        context->division_scale > CALCULATOR_MAX_OUTPUT_SCALE + CALCULATOR_DIVISION_GUARD_DIGITS)
    {
        calculator_error_set(error, CALCULATOR_VALUE_TOO_LARGE, 0U);

        return CALCULATOR_VALUE_TOO_LARGE;
    }

    evaluation.context = context;
    evaluation.constant_cache = &constant_cache;
    evaluation.answer = answer;
    evaluation.typed_answer = typed_answer;
    evaluation.random_state = random_state;

    for (size_t index = 0U; index < CALCULATOR_CONSTANT_COUNT; index++)
    {
        constant_cache.values[index] = NULL;
        constant_cache.digits[index] = 0;
    }

    status = calculator_evaluate_expression(&temporary, expression, &evaluation, error);

    for (size_t index = 0U; index < CALCULATOR_CONSTANT_COUNT; index++)
    {
        bigdecimal_destroy(constant_cache.values[index]);
    }

    if (status != CALCULATOR_OK)
    {
        return status;
    }

    status = calculator_budget_status(CALCULATOR_OK);

    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(bigdecimal_copy(result, temporary));
    }

    bigdecimal_destroy(temporary);

    if (status != CALCULATOR_OK)
    {
        calculator_error_set(error, status, expression->offset);

        return status;
    }

    calculator_error_clear(error);

    return CALCULATOR_OK;
}

CalculatorStatus calculator_evaluate(
    BigDecimal *result,
    const CalculatorExpression *expression,
    const CalculatorContext *context,
    CalculatorError *error
)
{
    return calculator_evaluate_with_answer(result, expression, context, NULL, NULL, NULL, error);
}

CalculatorStatus calculator_evaluate_with_answer(
    BigDecimal *result,
    const CalculatorExpression *expression,
    const CalculatorContext *context,
    const BigDecimal *answer,
    const CalculatorValue *typed_answer,
    uint64_t *random_state,
    CalculatorError *error
)
{
    bool owner;
    CalculatorStatus status;
    uint64_t local_random_state = calculator_random_seed();
    if (calculator_expression_has_complex(expression,typed_answer)) {
        calculator_error_set(error,CALCULATOR_INVALID_ARGUMENT,expression->offset);
        return CALCULATOR_INVALID_ARGUMENT;
    }

    if (random_state == NULL)
    {
        random_state = &local_random_state;
    }

    if (context == NULL)
    {
        return calculator_evaluate_impl(result, expression, context, answer, typed_answer, random_state, error);
    }

    owner = numforge_budget_begin(context->time_limit_ms < 0 ? 0U : (uint64_t)context->time_limit_ms,
                                  CALCULATOR_ALLOCATION_BUDGET,
                                  CALCULATOR_SINGLE_ALLOCATION);
    status = calculator_evaluate_impl(result, expression, context, answer, typed_answer, random_state, error);

    if (status != CALCULATOR_OK && context->time_limit_ms >= 0)
    {
        status = calculator_budget_status(status);
    }

    if (status != CALCULATOR_OK && (error == NULL || error->status != status))
    {
        calculator_error_set(error, status, error == NULL ? 0U : error->offset);
    }

    if (owner)
    {
        numforge_budget_end();
    }

    return status;
}
