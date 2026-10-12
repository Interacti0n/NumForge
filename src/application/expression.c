#include "expression.h"
#include "expression_internal.h"
#include "evaluator.h"
#include <numforge/runtime.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(APPLICATION_EXPRESSION_MAX_SAMPLES <=
    CALCULATOR_SINGLE_ALLOCATION / sizeof(ApplicationExpressionSample),
    "Sample array must fit the application single-allocation budget");

struct ApplicationExpression
{
    CalculatorExpression *tree;
    CalculatorContext context;
    char parameter_name[CALCULATOR_VARIABLE_NAME_BYTES + 1U];
    CalculatorValue parameter; /* borrowed for one sample only */
    CalculatorValue answer;
    CalculatorVariable variables[CALCULATOR_VARIABLE_CAPACITY];
    size_t variable_count;
    bool uses_variables;
};

void application_expression_destroy(ApplicationExpression *expression)
{
    if (expression == NULL) return;
    calculator_expression_destroy(expression->tree);
    calculator_value_destroy(&expression->answer);
    for (size_t i = 0; i < expression->variable_count; ++i)
        calculator_value_destroy(&expression->variables[i].value);
    free(expression);
}

static bool valid_parameter(const char *name)
{
    size_t length = 0U;
    while (length <= CALCULATOR_VARIABLE_NAME_BYTES && name[length] != '\0') ++length;
    if (length == 0U || length > CALCULATOR_VARIABLE_NAME_BYTES) return false;
    for (size_t i = 0; i < length; ++i)
        if (!((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= 'A' && name[i] <= 'Z')))
            return false;
    CalculatorConstant constant;
    return strcmp(name, "i") != 0 && strcmp(name, "ans") != 0 &&
        calculator_function_find(name, length) == NULL &&
        !calculator_constant_from_text(name, length, &constant);
}

static CalculatorStatus bind_snapshot(ApplicationExpression *prepared,
    CalculatorExpression *node, const CalculatorSession *session, CalculatorError *error)
{
    CalculatorStatus status = CALCULATOR_OK;
    if (!numforge_budget_check()) status = calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
    else switch (node->type)
    {
        case CALCULATOR_EXPRESSION_VARIABLE:
        {
            prepared->uses_variables = true;
            const char *name = node->data.variable.name;
            if (strcmp(name, prepared->parameter_name) == 0)
            {
                node->data.variable.value = &prepared->parameter;
                break;
            }
            for (size_t i = 0; i < prepared->variable_count; ++i)
                if (strcmp(name, prepared->variables[i].name) == 0)
                {
                    node->data.variable.value = &prepared->variables[i].value;
                    return CALCULATOR_OK;
                }
            status = CALCULATOR_UNDEFINED_VARIABLE;
            if (session != NULL)
                for (size_t i = 0; i < session->variable_count; ++i)
                    if (strcmp(name, session->variables[i].name) == 0)
                    {
                        CalculatorVariable *snapshot = &prepared->variables[prepared->variable_count];
                        status = calculator_value_copy(&snapshot->value, &session->variables[i].value);
                        if (status == CALCULATOR_OK)
                        {
                            memcpy(snapshot->name, name, strlen(name) + 1U);
                            node->data.variable.value = &snapshot->value;
                            ++prepared->variable_count;
                        }
                        break;
                    }
            break;
        }
        case CALCULATOR_EXPRESSION_ANSWER:
            if (prepared->answer.number == NULL)
            {
                const CalculatorValue *answer = calculator_session_answer(session);
                status = answer == NULL ? CALCULATOR_UNDEFINED_ANSWER :
                    calculator_value_copy(&prepared->answer, answer);
            }
            break;
        case CALCULATOR_EXPRESSION_UNARY:
            return bind_snapshot(prepared, node->data.unary.operand, session, error);
        case CALCULATOR_EXPRESSION_POSTFIX:
            return bind_snapshot(prepared, node->data.postfix.operand, session, error);
        case CALCULATOR_EXPRESSION_BINARY:
            status = bind_snapshot(prepared, node->data.binary.left, session, error);
            if (status != CALCULATOR_OK) return status;
            return bind_snapshot(prepared, node->data.binary.right, session, error);
        case CALCULATOR_EXPRESSION_CALL:
            if (node->data.call.function->implementation == CALCULATOR_FUNCTION_RANDOM)
                status = CALCULATOR_INVALID_ARGUMENT;
            else
                for (size_t i = 0; i < node->data.call.count; ++i)
                {
                    status = bind_snapshot(prepared, node->data.call.arguments[i], session, error);
                    if (status != CALCULATOR_OK) return status;
                }
            break;
        default: break;
    }
    if (status != CALCULATOR_OK) calculator_error_set(error, status, node->offset);
    return status;
}

CalculatorStatus application_expression_prepare(ApplicationExpression **result,
    const char *input, const char *parameter, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error)
{
    CalculatorStatus status;
    ApplicationExpression *prepared = NULL;
    calculator_error_clear(error);
    if (result == NULL || input == NULL || parameter == NULL || context == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U);
        return CALCULATOR_NULL_ARGUMENT;
    }
    status = calculator_context_validate(context, error);
    if (status != CALCULATOR_OK) return status;
    if (!valid_parameter(parameter) || (session != NULL && session->variable_count > CALCULATOR_VARIABLE_CAPACITY))
        status = CALCULATOR_INVALID_ARGUMENT;
    else if (session != NULL && session->released) status = CALCULATOR_SESSION_EXPIRED;
    else status = CALCULATOR_OK;
    if (status != CALCULATOR_OK)
    {
        calculator_error_set(error, status, 0U);
        return status;
    }
    size_t length = 0U;
    while (length <= CALCULATOR_MAX_INPUT_BYTES && input[length] != '\0') ++length;
    if (length > CALCULATOR_MAX_INPUT_BYTES)
    {
        calculator_error_set(error, CALCULATOR_VALUE_TOO_LARGE, length);
        return CALCULATOR_VALUE_TOO_LARGE;
    }
    bool owner = numforge_budget_begin((uint64_t)context->time_limit_ms,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    prepared = numforge_calloc(1U, sizeof(*prepared));
    status = prepared == NULL ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_OK;
    if (status == CALCULATOR_OK)
    {
        prepared->context = *context;
        memcpy(prepared->parameter_name, parameter, strlen(parameter) + 1U);
        status = calculator_parse_variables(input, &prepared->tree, error);
        if (status == CALCULATOR_OK) status = bind_snapshot(prepared, prepared->tree, session, error);
    }
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) *result = prepared;
    else
    {
        application_expression_destroy(prepared);
        calculator_error_set(error, status, error == NULL ? 0U : error->offset);
    }
    if (owner) numforge_budget_end();
    return status;
}

void application_expression_samples_destroy(ApplicationExpressionSample *samples, size_t count)
{
    if (samples == NULL) return;
    for (size_t i = 0; i < count; ++i) calculator_value_destroy(&samples[i].value);
    free(samples);
}

CalculatorStatus application_expression_sample(ApplicationExpression *expression,
    const CalculatorValue *arguments, size_t count,
    ApplicationExpressionSample **result, CalculatorError *error)
{
    calculator_error_clear(error);
    CalculatorStatus status = CALCULATOR_OK;
    if (expression == NULL || arguments == NULL || result == NULL) status = CALCULATOR_NULL_ARGUMENT;
    else if (count == 0U || count > APPLICATION_EXPRESSION_MAX_SAMPLES) status = CALCULATOR_INVALID_ARGUMENT;
    if (status != CALCULATOR_OK)
    {
        calculator_error_set(error, status, 0U);
        return status;
    }
    bool owner = numforge_budget_begin((uint64_t)expression->context.time_limit_ms,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    ApplicationExpressionSample *samples = numforge_calloc(count, sizeof(*samples));
    status = samples == NULL ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_OK;
    for (size_t i = 0; status == CALCULATOR_OK && i < count; ++i)
    {
        if (!numforge_budget_check()) { status = CALCULATOR_OUT_OF_MEMORY; break; }
        if (arguments[i].number == NULL) { status = CALCULATOR_INVALID_ARGUMENT; break; }
        expression->parameter = arguments[i];
        samples[i].value.uses_variables = expression->uses_variables;
        status = calculator_evaluate_parsed_value(expression->tree, &expression->context,
            expression->answer.number == NULL ? NULL : &expression->answer, NULL,
            &samples[i].value, &samples[i].error);
        memset(&expression->parameter, 0, sizeof(expression->parameter));
        if (status == CALCULATOR_DIVISION_BY_ZERO || status == CALCULATOR_INVALID_ARGUMENT ||
            status == CALCULATOR_DIMENSION_ERROR || status == CALCULATOR_NOT_IMPLEMENTED)
            status = CALCULATOR_OK;
        else if (status != CALCULATOR_OK && error != NULL) *error = samples[i].error;
    }
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) *result = samples;
    else
    {
        application_expression_samples_destroy(samples, count);
        calculator_error_set(error, status, error == NULL ? 0U : error->offset);
    }
    if (owner) numforge_budget_end();
    return status;
}
