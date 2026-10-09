#include "../internal/benchmark_profile.h"
#include "calculator_internal.h"
#include "parser.h"
#include "evaluator.h"
#include "formatter.h"
#include "expression_internal.h"
#include "exact_evaluator.h"
#include "value_internal.h"
#include "quantity.h"
#include "complex_evaluator.h"

#include <numforge/runtime.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Shared calculator utilities. The tokenizer, parser, and evaluator use this
    module for a common status model, source-positioned errors, and explicit
    BigDecimal evaluation defaults.
------------------------------------------------------------------------------------------------------------------------------
*/

/*
------------------------------------------------------------------------------------------------------------------------------
    Shared utility functions for calculator operations.
------------------------------------------------------------------------------------------------------------------------------
*/

const char *calculator_status_to_string(
    CalculatorStatus status
)
{
    switch (status)
    {
        case CALCULATOR_OK:
            return "success";
        case CALCULATOR_NULL_ARGUMENT:
            return "null argument";
        case CALCULATOR_OUT_OF_MEMORY:
            return "out of memory";
        case CALCULATOR_INVALID_ARGUMENT:
            return "invalid argument";
        case CALCULATOR_INVALID_TOKEN:
            return "invalid token";
        case CALCULATOR_SYNTAX_ERROR:
            return "syntax error";
        case CALCULATOR_DIVISION_BY_ZERO:
            return "division by zero";
        case CALCULATOR_VALUE_TOO_LARGE:
            return "value too large";
        case CALCULATOR_SCALE_OVERFLOW:
            return "scale overflow";
        case CALCULATOR_TIME_LIMIT:
            return "TLE: time limit exceeded";
        case CALCULATOR_NOT_IMPLEMENTED:
            return "not implemented";
        case CALCULATOR_ARGUMENT_COUNT:
            return "wrong number of arguments";
        case CALCULATOR_UNDEFINED_ANSWER:
            return "ans is undefined";
        case CALCULATOR_DIMENSION_ERROR:
            return "invalid quantity operation";
        case CALCULATOR_UNKNOWN_UNIT:
            return "unknown unit";
        case CALCULATOR_INCOMPATIBLE_UNITS:
            return "incompatible units";
        case CALCULATOR_UNDEFINED_VARIABLE:
            return "variable is undefined";
        case CALCULATOR_STALE_REQUEST:
            return "stale session request";
        case CALCULATOR_SESSION_EXPIRED:
            return "session expired; reload the page";
        default:
            return "unknown status";
    }
}

void calculator_context_init(
    CalculatorContext *context
)
{
    if (context == NULL)
    {
        return;
    }

    context->division_scale = CALCULATOR_DEFAULT_DIVISION_SCALE;
    context->output_scale = CALCULATOR_DEFAULT_OUTPUT_SCALE;
    context->time_limit_ms = CALCULATOR_DEFAULT_TIME_LIMIT_MS;
    context->rounding = BIGDECIMAL_ROUND_HALF_EVEN;
    context->notation = CALCULATOR_NOTATION_AUTO;
    context->angle_unit = CALCULATOR_ANGLE_RADIANS;
    context->significant_division = true;
    context->complex_form = BIGCOMPLEX_FORM_CARTESIAN;
}

CalculatorStatus calculator_context_set_angle_unit(
    CalculatorContext *context,
    CalculatorAngleUnit angle_unit
)
{
    if (context == NULL)
    {
        return CALCULATOR_NULL_ARGUMENT;
    }

    if (angle_unit != CALCULATOR_ANGLE_RADIANS &&
        angle_unit != CALCULATOR_ANGLE_DEGREES)
    {
        return CALCULATOR_INVALID_ARGUMENT;
    }

    context->angle_unit = angle_unit;
    return CALCULATOR_OK;
}

CalculatorStatus calculator_context_set_output_scale(
    CalculatorContext *context,
    int64_t output_scale
)
{
    if (context == NULL)
    {
        return CALCULATOR_NULL_ARGUMENT;
    }

    if (output_scale < CALCULATOR_UNLIMITED_OUTPUT_SCALE)
    {
        return CALCULATOR_INVALID_ARGUMENT;
    }

    if (output_scale == CALCULATOR_UNLIMITED_OUTPUT_SCALE)
    {
        context->division_scale = CALCULATOR_DEFAULT_DIVISION_SCALE;
        context->output_scale = output_scale;

        return CALCULATOR_OK;
    }

    if (output_scale > INT64_MAX - CALCULATOR_DIVISION_GUARD_DIGITS)
    {
        return CALCULATOR_SCALE_OVERFLOW;
    }

    if (output_scale > CALCULATOR_MAX_OUTPUT_SCALE)
    {
        return CALCULATOR_VALUE_TOO_LARGE;
    }

    context->division_scale = output_scale + CALCULATOR_DIVISION_GUARD_DIGITS;

    if (context->division_scale < CALCULATOR_DEFAULT_DIVISION_SCALE)
    {
        context->division_scale = CALCULATOR_DEFAULT_DIVISION_SCALE;
    }

    context->output_scale = output_scale;

    return CALCULATOR_OK;
}

CalculatorStatus calculator_budget_status(
    CalculatorStatus status
)
{
    (void)numforge_budget_check();

    if (numforge_budget_failure() == NUMFORGE_BUDGET_TIME)
    {
        return CALCULATOR_TIME_LIMIT;
    }

    if (numforge_budget_failure() == NUMFORGE_BUDGET_MEMORY)
    {
        return CALCULATOR_VALUE_TOO_LARGE;
    }

    return status;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Conservative proof of independence from working precision. Only explicitly
    exact operations qualify, and every operand must qualify too. Unknown/new
    operations default to context-dependent; no inference from numeric output.
------------------------------------------------------------------------------------------------------------------------------
*/

static bool calculator_expression_independent(const CalculatorExpression *expression)
{
    switch (expression->type)
    {
        case CALCULATOR_EXPRESSION_VARIABLE:
        case CALCULATOR_EXPRESSION_NUMBER:
            return true;
        case CALCULATOR_EXPRESSION_UNARY:
            return calculator_expression_independent(expression->data.unary.operand);
        case CALCULATOR_EXPRESSION_POSTFIX:
            return (expression->data.postfix.operation == CALCULATOR_POSTFIX_SQUARE ||
                    expression->data.postfix.operation == CALCULATOR_POSTFIX_CUBE ||
                    expression->data.postfix.operation == CALCULATOR_POSTFIX_FACTORIAL) &&
                   calculator_expression_independent(expression->data.postfix.operand);
        case CALCULATOR_EXPRESSION_BINARY:
            return (expression->data.binary.operation == CALCULATOR_BINARY_ADD ||
                    expression->data.binary.operation == CALCULATOR_BINARY_SUBTRACT ||
                    expression->data.binary.operation == CALCULATOR_BINARY_MULTIPLY) &&
                   calculator_expression_independent(expression->data.binary.left) &&
                   calculator_expression_independent(expression->data.binary.right);
        case CALCULATOR_EXPRESSION_CALL:
            switch (expression->data.call.function->implementation)
            {
                case CALCULATOR_FUNCTION_FACTORIAL:
                case CALCULATOR_FUNCTION_SIGN:
                case CALCULATOR_FUNCTION_MIN:
                case CALCULATOR_FUNCTION_MAX:
                case CALCULATOR_FUNCTION_SUM:
                case CALCULATOR_FUNCTION_PRODUCT:
                case CALCULATOR_FUNCTION_MEDIAN:
                case CALCULATOR_FUNCTION_FLOOR:
                case CALCULATOR_FUNCTION_CEIL:
                case CALCULATOR_FUNCTION_TRUNC:
                case CALCULATOR_FUNCTION_ROUND:
                case CALCULATOR_FUNCTION_GCD:
                case CALCULATOR_FUNCTION_LCM:
                case CALCULATOR_FUNCTION_MOD:
                case CALCULATOR_FUNCTION_NPR:
                case CALCULATOR_FUNCTION_NCR:
                case CALCULATOR_FUNCTION_ISQRT:
                    break;
                default:
                    return false;
            }
            for (size_t index = 0U; index < expression->data.call.count; index++)
            {
                if (!calculator_expression_independent(expression->data.call.arguments[index]))
                {
                    return false;
                }
            }
            return true;
        default:
            return false;
    }
}

static bool calculator_expression_uses_answer(const CalculatorExpression *expression)
{
    switch (expression->type)
    {
        case CALCULATOR_EXPRESSION_ANSWER:
            return true;
        case CALCULATOR_EXPRESSION_UNARY:
            return calculator_expression_uses_answer(expression->data.unary.operand);
        case CALCULATOR_EXPRESSION_POSTFIX:
            return calculator_expression_uses_answer(expression->data.postfix.operand);
        case CALCULATOR_EXPRESSION_BINARY:
            return calculator_expression_uses_answer(expression->data.binary.left) ||
                   calculator_expression_uses_answer(expression->data.binary.right);
        case CALCULATOR_EXPRESSION_CALL:
            for (size_t index = 0U; index < expression->data.call.count; index++)
            {
                if (calculator_expression_uses_answer(expression->data.call.arguments[index]))
                {
                    return true;
                }
            }
            return false;
        default:
            return false;
    }
}

static bool calculator_expression_uses_random(const CalculatorExpression *expression)
{
    switch (expression->type)
    {
        case CALCULATOR_EXPRESSION_CALL:
            if (expression->data.call.function->implementation == CALCULATOR_FUNCTION_RANDOM)
            {
                return true;
            }
            for (size_t index = 0U; index < expression->data.call.count; index++)
            {
                if (calculator_expression_uses_random(expression->data.call.arguments[index]))
                {
                    return true;
                }
            }
            return false;
        case CALCULATOR_EXPRESSION_UNARY:
            return calculator_expression_uses_random(expression->data.unary.operand);
        case CALCULATOR_EXPRESSION_POSTFIX:
            return calculator_expression_uses_random(expression->data.postfix.operand);
        case CALCULATOR_EXPRESSION_BINARY:
            return calculator_expression_uses_random(expression->data.binary.left) ||
                calculator_expression_uses_random(expression->data.binary.right);
        default:
            return false;
    }
}

CalculatorStatus calculator_compute_value(
    const char *input,
    const CalculatorContext *context,
    CalculatorValue *result,
    CalculatorError *error
)
{
    return calculator_compute_value_with_answer(input, context, NULL, NULL, result, error);
}

/* Resolve names once, before evaluation; ASTs borrow immutable stored values.
 * A failed lookup keeps its original expression byte offset. */
static CalculatorStatus calculator_bind_variables(CalculatorExpression *expression,
    const CalculatorVariable *variables, size_t count, bool *used, CalculatorError *error)
{
    CalculatorStatus status;
    switch (expression->type)
    {
        case CALCULATOR_EXPRESSION_VARIABLE:
            *used = true;
            for (size_t i=0;i<count;i++)
                if (strcmp(expression->data.variable.name, variables[i].name)==0)
                {
                    expression->data.variable.value=&variables[i].value;
                    return CALCULATOR_OK;
                }
            calculator_error_set(error,CALCULATOR_UNDEFINED_VARIABLE,expression->offset);
            return CALCULATOR_UNDEFINED_VARIABLE;
        case CALCULATOR_EXPRESSION_UNARY:
            return calculator_bind_variables(expression->data.unary.operand,variables,count,used,error);
        case CALCULATOR_EXPRESSION_POSTFIX:
            return calculator_bind_variables(expression->data.postfix.operand,variables,count,used,error);
        case CALCULATOR_EXPRESSION_BINARY:
            status=calculator_bind_variables(expression->data.binary.left,variables,count,used,error);
            return status!=CALCULATOR_OK ? status : calculator_bind_variables(expression->data.binary.right,variables,count,used,error);
        case CALCULATOR_EXPRESSION_CALL:
            for(size_t i=0;i<expression->data.call.count;i++)
            {
                status=calculator_bind_variables(expression->data.call.arguments[i],variables,count,used,error);
                if(status!=CALCULATOR_OK) return status;
            }
            break;
        default: break;
    }
    return CALCULATOR_OK;
}

static CalculatorStatus calculator_compute_value_with_answer_profile_impl(
    const char *input,
    const CalculatorContext *context,
    const CalculatorValue *answer,
    uint64_t *random_state,
    const CalculatorVariable *variables, size_t variable_count,
    CalculatorValue *result,
    CalculatorError *error
)
{
    CalculatorExpression *expression = NULL;
    BigDecimal *value = NULL;
    BigDecimal *answer_decimal = NULL;
    BigRational *exact = NULL;
    CalculatorStatus status;
    bool owner;
    bool exact_candidate = false;
    size_t length = 0U;

    if (result != NULL)
    {
        memset(result, 0, sizeof(*result));
    }

    if (input == NULL || context == NULL || result == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U);

        return CALCULATOR_NULL_ARGUMENT;
    }

    if (context->time_limit_ms < 0)
    {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0U);

        return CALCULATOR_INVALID_ARGUMENT;
    }

    if (context->rounding < BIGDECIMAL_ROUND_TOWARD_ZERO ||
        context->rounding > BIGDECIMAL_ROUND_HALF_EVEN ||
        context->output_scale < CALCULATOR_UNLIMITED_OUTPUT_SCALE)
    {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0U);
        return CALCULATOR_INVALID_ARGUMENT;
    }
    if (context->output_scale > CALCULATOR_MAX_OUTPUT_SCALE ||
        context->division_scale > CALCULATOR_MAX_OUTPUT_SCALE + CALCULATOR_DIVISION_GUARD_DIGITS)
    {
        calculator_error_set(error, CALCULATOR_VALUE_TOO_LARGE, 0U);
        return CALCULATOR_VALUE_TOO_LARGE;
    }

    owner = numforge_budget_begin(
        (uint64_t)context->time_limit_ms, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    calculator_error_clear(error);

    while (length <= CALCULATOR_MAX_INPUT_BYTES && input[length] != '\0')
    {
        length++;
    }

    status = length > CALCULATOR_MAX_INPUT_BYTES ? CALCULATOR_VALUE_TOO_LARGE : CALCULATOR_OK;

    if (status == CALCULATOR_OK)
    {
        status = variables != NULL ? calculator_parse_variables(input, &expression, error)
            : calculator_parse(input, &expression, error);
        if (status == CALCULATOR_OK && variables != NULL)
            status = calculator_bind_variables(expression, variables, variable_count, &result->uses_variables, error);
    }

    if (status == CALCULATOR_OK && calculator_expression_has_complex(expression, answer))
    {
        bool uses_variables = result->uses_variables;
        status = calculator_expression_has_quantity(expression, answer) ? CALCULATOR_DIMENSION_ERROR :
            calculator_evaluate_complex(result, expression, context, answer, random_state, error);
        result->uses_variables = uses_variables;
        value = result->number;
        result->number = NULL;
    }
    else if (status == CALCULATOR_OK && calculator_expression_has_quantity(expression, answer))
    {
        bool uses_variables = result->uses_variables;
        status = calculator_evaluate_quantity(result, expression, context, answer, random_state, error);
        result->uses_variables = uses_variables;
        value = result->number;
        result->number = NULL;
    }
    else if (status == CALCULATOR_OK)
    {
        value = bigdecimal_create();
        if (value == NULL)
        {
            status = CALCULATOR_OUT_OF_MEMORY;
        }
        else
        {
            exact_candidate = context->significant_division && calculator_exact_supported(expression, answer);
        }
        if (status == CALCULATOR_OK && exact_candidate)
        {
            BigInt *denominator = bigint_create();
            status = denominator == NULL ? CALCULATOR_OUT_OF_MEMORY :
                calculator_evaluate_exact(&exact, expression, answer, error);
            if (status == CALCULATOR_NOT_IMPLEMENTED)
            {
                exact_candidate = false;
                status = CALCULATOR_OK;
                calculator_error_clear(error);
            }
            if (status == CALCULATOR_OK)
            {
                status = !exact_candidate ? CALCULATOR_OK :
                    bigrational_get_denominator(denominator, exact) == BIGRATIONAL_OK
                    ? CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
            }
            if (status == CALCULATOR_OK && exact_candidate && bigint_is_one(denominator))
            {
                result->integer = bigint_create();
                status = result->integer != NULL &&
                    bigrational_get_numerator(result->integer, exact) == BIGRATIONAL_OK
                    ? CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
                if (status == CALCULATOR_OK)
                {
                    result->kind = CALCULATOR_VALUE_INTEGER;
                }
            }
            else if (status == CALCULATOR_OK && exact_candidate)
            {
                result->rational = exact;
                result->kind = CALCULATOR_VALUE_RATIONAL;
                exact = NULL;
            }
            bigint_destroy(denominator);
            if (status == CALCULATOR_OK && exact_candidate)
            {
                CalculatorValue temporary = {0};
                temporary.integer = result->integer;
                temporary.rational = result->rational;
                temporary.kind = result->kind;
                temporary.number = value;
                status = calculator_materialize_exact(value, &temporary, context);
            }
        }
        if (status == CALCULATOR_OK && !exact_candidate)
        {
            if (answer != NULL && (answer->kind == CALCULATOR_VALUE_INTEGER || answer->kind == CALCULATOR_VALUE_RATIONAL))
            {
                answer_decimal = bigdecimal_create();
                status = answer_decimal == NULL ? CALCULATOR_OUT_OF_MEMORY :
                    calculator_materialize_exact(answer_decimal, answer, context);
            }
            else
            {
                status = CALCULATOR_OK;
            }
            if (status == CALCULATOR_OK)
            {
                status = calculator_evaluate_with_answer(value, expression, context,
                    answer_decimal != NULL ? answer_decimal :
                    answer == NULL ? NULL : answer->number, answer, random_state, error);
            }
        }
    }

    if (status == CALCULATOR_OK)
    {
        result->independent = (result->kind != CALCULATOR_VALUE_DECIMAL && result->kind != CALCULATOR_VALUE_COMPLEX_DECIMAL) ||
            calculator_expression_independent(expression);
        result->uses_answer = calculator_expression_uses_answer(expression);
        result->uses_random = calculator_expression_uses_random(expression);
    }
    calculator_expression_destroy(expression);
    bigdecimal_destroy(answer_decimal);
    status = calculator_budget_status(status);

    if (status != CALCULATOR_OK)
    {
        bigdecimal_destroy(value);
        bigrational_destroy(exact);
        calculator_value_destroy(result);

        if (error == NULL || error->status != status)
        {
            calculator_error_set(error, status, error == NULL ? 0U : error->offset);
        }
    }
    else
    {
        bigrational_destroy(exact);
        result->number = value;
        result->context = *context;
        calculator_error_clear(error);
    }

    if (owner)
    {
        numforge_budget_end();
    }

    return status;
}

CalculatorStatus calculator_compute_value_with_answer(
    const char *input,
    const CalculatorContext *context,
    const CalculatorValue *answer,
    uint64_t *random_state,
    CalculatorValue *result,
    CalculatorError *error
)
{
    NumForgeProfilePhase previous = numforge_profile_enter(NUMFORGE_PHASE_EVALUATE);
    CalculatorStatus status = calculator_compute_value_with_answer_profile_impl(input, context, answer, random_state, NULL, 0U, result, error);
    numforge_profile_leave(previous);
    return status;
}

CalculatorStatus calculator_compute_value_with_variables(const char *input,
    const CalculatorContext *context, const CalculatorValue *answer,
    uint64_t *random_state, const CalculatorVariable *variables, size_t count,
    CalculatorValue *result, CalculatorError *error)
{
    NumForgeProfilePhase previous = numforge_profile_enter(NUMFORGE_PHASE_EVALUATE);
    CalculatorStatus status = calculator_compute_value_with_answer_profile_impl(
        input, context, answer, random_state, variables, count, result, error);
    numforge_profile_leave(previous);
    return status;
}

/* Keep the original one-shot API and its shared compute/format time budget. */
CalculatorStatus calculator_compute(
    const char *input,
    const CalculatorContext *context,
    char **result,
    CalculatorError *error
)
{
    CalculatorValue value = {0};
    CalculatorStatus status;
    bool owner;

    if (result != NULL)
    {
        *result = NULL;
    }
    if (input == NULL || context == NULL || result == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U);
        return CALCULATOR_NULL_ARGUMENT;
    }
    if (context->time_limit_ms < 0)
    {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0U);
        return CALCULATOR_INVALID_ARGUMENT;
    }
    owner = numforge_budget_begin(
        (uint64_t)context->time_limit_ms, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    status = calculator_compute_value(input, context, &value, error);
    if (status == CALCULATOR_OK)
    {
        status = calculator_format_value(&value, context, result);
        status = calculator_budget_status(status);
        if (status != CALCULATOR_OK)
        {
            free(*result);
            *result = NULL;
        }
        calculator_error_set(error, status, 0U);
    }
    calculator_value_destroy(&value);
    if (owner)
    {
        numforge_budget_end();
    }
    return status;
}

void calculator_error_clear(
    CalculatorError *error
)
{
    calculator_error_set(error, CALCULATOR_OK, 0);
}

void calculator_error_set(
    CalculatorError *error,
    CalculatorStatus status,
    size_t offset
)
{
    if (error != NULL)
    {
        error->status = status;
        error->offset = offset;
    }
}

size_t calculator_error_column(
    const char *input,
    size_t byte_offset
)
{
    size_t column = 1U;
    size_t index;

    if (input == NULL)
    {
        return column;
    }

    for (index = 0U; index < byte_offset && input[index] != '\0'; index++)
    {
        unsigned char byte = (unsigned char)input[index];

        if (input[index] == '\n' || input[index] == '\r')
        {
            column = 1U;
        }
        else if ((byte & 0xC0U) != 0x80U)
        {
            column++;
        }
    }

    return column;
}
