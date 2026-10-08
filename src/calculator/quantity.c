#include "quantity.h"
#include "evaluator.h"
#include "exact_evaluator.h"
#include "exact_functions.h"
#include "value_internal.h"
#include "unit_status.h"

#include <numforge/runtime.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DIMENSION_LIMIT 32

static const char *const canonical_units[] = {
    "m", "m2", "m3", "kg", "s", "m/s", "K", "deltaK", "bit", "rad"
};

static void unit_dimensions(CalculatorValue *value, const NumForgeUnitInfo *unit)
{
    memset(value->dimensions, 0, sizeof(value->dimensions));
    value->quantity = true;
    value->temperature_point = unit->quantity == NUMFORGE_UNIT_TEMPERATURE;
    switch (unit->quantity)
    {
        case NUMFORGE_UNIT_LENGTH: value->dimensions[0] = 1; break;
        case NUMFORGE_UNIT_AREA: value->dimensions[0] = 2; break;
        case NUMFORGE_UNIT_VOLUME: value->dimensions[0] = 3; break;
        case NUMFORGE_UNIT_MASS: value->dimensions[1] = 1; break;
        case NUMFORGE_UNIT_TIME: value->dimensions[2] = 1; break;
        case NUMFORGE_UNIT_SPEED: value->dimensions[0] = 1; value->dimensions[2] = -1; break;
        case NUMFORGE_UNIT_TEMPERATURE:
        case NUMFORGE_UNIT_TEMPERATURE_INTERVAL: value->dimensions[3] = 1; break;
        case NUMFORGE_UNIT_INFORMATION: value->dimensions[4] = 1; break;
        case NUMFORGE_UNIT_ANGLE: value->dimensions[5] = 1; break;
    }
    memcpy(value->unit, unit->id, strlen(unit->id) + 1U);
}

static bool same_dimensions(const CalculatorValue *a, const CalculatorValue *b)
{
    return memcmp(a->dimensions, b->dimensions, sizeof(a->dimensions)) == 0;
}

static void dimensions_copy(CalculatorValue *result, const CalculatorValue *source)
{
    result->quantity = source->quantity;
    result->temperature_point = source->temperature_point;
    memcpy(result->dimensions, source->dimensions, sizeof(result->dimensions));
    memcpy(result->unit, source->unit, sizeof(result->unit));
}

/* Assign catalogue IDs to common derived dimensions, otherwise keep an unnamed
 * composite in canonical m/kg/s/deltaK/bit/rad units. Cancellation yields scalar. */
static void canonical_identity(CalculatorValue *value)
{
    value->unit[0] = '\0';
    value->quantity = false;
    for (size_t i = 0; i < 6U; i++)
        if (value->dimensions[i] != 0) value->quantity = true;
    if (!value->quantity) return;
    for (size_t i = 0; i < sizeof(canonical_units)/sizeof(canonical_units[0]); i++)
    {
        CalculatorValue candidate = {0};
        unit_dimensions(&candidate, numforge_unit_find(canonical_units[i]));
        if (same_dimensions(value, &candidate) && value->temperature_point == candidate.temperature_point)
        {
            memcpy(value->unit, candidate.unit, sizeof(value->unit));
            return;
        }
    }
}

bool calculator_expression_has_quantity(const CalculatorExpression *expression, const CalculatorValue *answer)
{
    switch (expression->type)
    {
        case CALCULATOR_EXPRESSION_VARIABLE:
            return expression->data.variable.value != NULL && expression->data.variable.value->quantity;
        case CALCULATOR_EXPRESSION_ANSWER: return answer != NULL && answer->quantity;
        case CALCULATOR_EXPRESSION_UNARY:
            return calculator_expression_has_quantity(expression->data.unary.operand, answer);
        case CALCULATOR_EXPRESSION_POSTFIX:
            return calculator_expression_has_quantity(expression->data.postfix.operand, answer);
        case CALCULATOR_EXPRESSION_BINARY:
            return calculator_expression_has_quantity(expression->data.binary.left, answer) ||
                calculator_expression_has_quantity(expression->data.binary.right, answer);
        case CALCULATOR_EXPRESSION_CALL:
            if (expression->data.call.function->implementation == CALCULATOR_FUNCTION_QUANTITY) return true;
            for (size_t i=0; i<expression->data.call.count; i++)
                if (calculator_expression_has_quantity(expression->data.call.arguments[i], answer)) return true;
            return false;
        default: return false;
    }
}

static CalculatorStatus numeric_value(CalculatorValue *result, const CalculatorExpression *expression,
    const CalculatorContext *context, const CalculatorValue *answer, uint64_t *random_state, CalculatorError *error)
{
    CalculatorStatus status = CALCULATOR_OK;
    result->number = bigdecimal_create();
    if (result->number == NULL) return CALCULATOR_OUT_OF_MEMORY;
    if (context->significant_division && calculator_exact_supported(expression, answer))
    {
        status = calculator_evaluate_exact(&result->rational, expression, answer, error);
        if (status == CALCULATOR_OK)
        {
            result->kind = CALCULATOR_VALUE_RATIONAL;
            return calculator_materialize_exact(result->number, result, context);
        }
        if (status != CALCULATOR_NOT_IMPLEMENTED) return status;
        calculator_error_clear(error);
    }
    BigDecimal *projected_answer = NULL;
    if (answer != NULL && answer->kind != CALCULATOR_VALUE_DECIMAL)
    {
        projected_answer = bigdecimal_create();
        status = projected_answer == NULL ? CALCULATOR_OUT_OF_MEMORY :
            calculator_materialize_exact(projected_answer, answer, context);
    }
    if (status == CALCULATOR_OK || status == CALCULATOR_NOT_IMPLEMENTED)
        status = calculator_evaluate_with_answer(result->number, expression, context,
            projected_answer != NULL ? projected_answer : answer == NULL ? NULL : answer->number,
            answer, random_state, error);
    bigdecimal_destroy(projected_answer);
    result->kind = CALCULATOR_VALUE_DECIMAL;
    return status;
}

/* Re-express an owned numeric coordinate without changing its physical value. */
static CalculatorStatus reexpress(CalculatorValue *value, const char *to, const CalculatorContext *context)
{
    if (strcmp(value->unit, to) == 0) return CALCULATOR_OK;
    CalculatorValue converted = {0};
    CalculatorStatus status;
    converted.number = bigdecimal_create();
    if (converted.number == NULL) return CALCULATOR_OUT_OF_MEMORY;
    if (value->kind != CALCULATOR_VALUE_DECIMAL && numforge_unit_conversion_is_exact(value->unit, to))
    {
        BigRational *input = bigrational_create();
        converted.rational = bigrational_create();
        status = input == NULL || converted.rational == NULL ? CALCULATOR_OUT_OF_MEMORY :
            calculator_from_rational_status(value->integer != NULL ? bigrational_from_bigint(input, value->integer) :
                bigrational_copy(input, value->rational));
        if (status == CALCULATOR_OK)
            status = calculator_from_unit_status(numforge_unit_convert_rational(converted.rational, input, value->unit, to));
        bigrational_destroy(input);
        converted.kind = CALCULATOR_VALUE_RATIONAL;
        if (status == CALCULATOR_OK) status = calculator_materialize_exact(converted.number, &converted, context);
    }
    else
    {
        if (value->kind != CALCULATOR_VALUE_DECIMAL)
            status = calculator_materialize_exact(converted.number, value, context);
        else status = calculator_from_decimal_status(bigdecimal_copy(converted.number, value->number));
        if (status == CALCULATOR_OK)
            status = calculator_from_unit_status(numforge_unit_convert_decimal(converted.number, converted.number,
                value->unit, to, context->division_scale, context->rounding));
        converted.kind = CALCULATOR_VALUE_DECIMAL;
    }
    if (status == CALCULATOR_OK)
    {
        unit_dimensions(&converted, numforge_unit_find(to));
        calculator_value_destroy(value);
        *value = converted;
    }
    else calculator_value_destroy(&converted);
    return status;
}

static CalculatorStatus canonicalize(CalculatorValue *value, const CalculatorContext *context)
{
    if (!value->quantity || value->unit[0] == '\0') return CALCULATOR_OK;
    const NumForgeUnitInfo *unit = numforge_unit_find(value->unit);
    return reexpress(value, canonical_units[unit->quantity], context);
}

static CalculatorStatus align(CalculatorValue *value, const CalculatorValue *target, const CalculatorContext *context)
{
    if (!same_dimensions(value, target) || value->temperature_point != target->temperature_point)
        return CALCULATOR_DIMENSION_ERROR;
    if (!value->quantity) return CALCULATOR_OK;
    if (value->unit[0] != '\0' && target->unit[0] != '\0') return reexpress(value, target->unit, context);
    CalculatorStatus status = canonicalize(value, context);
    if (status == CALCULATOR_OK && target->unit[0] != '\0')
    {
        CalculatorValue identity = *target;
        canonical_identity(&identity);
        memcpy(value->unit, identity.unit, sizeof(value->unit));
        if (value->unit[0] == '\0') return CALCULATOR_DIMENSION_ERROR;
        status = reexpress(value, target->unit, context);
    }
    return status;
}

static CalculatorExpression binding(CalculatorValue *value, size_t offset)
{
    CalculatorExpression node = {0};
    node.type = CALCULATOR_EXPRESSION_VARIABLE;
    node.offset = offset;
    node.depth = 1U;
    node.data.variable.value = value;
    return node;
}

static CalculatorStatus small_integer(const CalculatorValue *value, int *number)
{
    char *text = NULL;
    BigRational *rational = bigrational_create();
    if (rational == NULL) return CALCULATOR_OUT_OF_MEMORY;
    CalculatorStatus status = calculator_from_rational_status(value->integer != NULL ?
        bigrational_from_bigint(rational, value->integer) : value->rational != NULL ?
        bigrational_copy(rational, value->rational) : bigrational_from_bigdecimal(rational, value->number));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_to_string(rational, &text));
    if (status == CALCULATOR_OK)
    {
        if (strlen(text) > 3U || strchr(text, '/') != NULL) status = CALCULATOR_DIMENSION_ERROR;
        else
        {
            *number = (int)strtol(text, NULL, 10);
            if (*number < -DIMENSION_LIMIT || *number > DIMENSION_LIMIT) status = CALCULATOR_DIMENSION_ERROR;
        }
    }
    free(text);
    bigrational_destroy(rational);
    return status;
}

static CalculatorStatus evaluate_recursive(CalculatorValue *result, const CalculatorExpression *expression,
    const CalculatorContext *context, const CalculatorValue *answer, uint64_t *random_state, CalculatorError *error);

static CalculatorStatus binary_quantity(CalculatorValue *result, CalculatorValue *left, CalculatorValue *right,
    const CalculatorExpression *expression, const CalculatorContext *context,
    const CalculatorValue *answer, uint64_t *random_state, CalculatorError *error)
{
    CalculatorBinaryOperator op = expression->data.binary.operation;
    CalculatorValue identity = {0};
    CalculatorStatus status = CALCULATOR_OK;
    char temperature_unit[32] = {0};
    if (!left->quantity && !right->quantity)
    {
        /* Previously cancelled dimensions behave as ordinary scalar numbers. */
    }
    else if (op == CALCULATOR_BINARY_ADD || op == CALCULATOR_BINARY_SUBTRACT)
    {
        if (left->quantity != right->quantity || !same_dimensions(left, right)) return CALCULATOR_DIMENSION_ERROR;
        if (left->temperature_point || right->temperature_point)
        {
            bool points = left->temperature_point && right->temperature_point;
            if ((points && op == CALCULATOR_BINARY_ADD) ||
                (!left->temperature_point && right->temperature_point && op == CALCULATOR_BINARY_SUBTRACT))
                return CALCULATOR_DIMENSION_ERROR;
            const char *preferred = left->temperature_point ? left->unit : right->unit;
            if (points)
                preferred = strcmp(preferred, "degC") == 0 ? "deltaC" :
                    strcmp(preferred, "degF") == 0 ? "deltaF" : "deltaK";
            memcpy(temperature_unit, preferred, strlen(preferred) + 1U);
            identity = *left;
            identity.temperature_point = !points;
            status = canonicalize(left, context);
            if (status == CALCULATOR_OK) status = canonicalize(right, context);
            memcpy(identity.unit, points ? "deltaK" : "K", points ? 7U : 2U);
        }
        else
        {
            identity = *left;
            status = align(right, left, context);
        }
    }
    else if (op == CALCULATOR_BINARY_POWER)
    {
        int exponent = 0;
        if (right->quantity || left->temperature_point) return CALCULATOR_DIMENSION_ERROR;
        status = small_integer(right, &exponent);
        if (status == CALCULATOR_OK && exponent != 0) status = canonicalize(left, context);
        for (size_t i=0; i<6U; i++)
        {
            identity.dimensions[i] = left->dimensions[i] * exponent;
            if (identity.dimensions[i] < -DIMENSION_LIMIT || identity.dimensions[i] > DIMENSION_LIMIT)
                status = CALCULATOR_DIMENSION_ERROR;
        }
        canonical_identity(&identity);
    }
    else
    {
        if (left->temperature_point || right->temperature_point) return CALCULATOR_DIMENSION_ERROR;
        if (op == CALCULATOR_BINARY_DIVIDE && left->quantity && right->quantity && same_dimensions(left, right))
            status = align(right, left, context); /* Cancel compatible scales before introducing pi. */
        else if (left->quantity && !right->quantity)
            identity = *left; /* Scaling a quantity retains its selected unit. */
        else if (!left->quantity && right->quantity && op == CALCULATOR_BINARY_MULTIPLY)
            identity = *right;
        else
        {
            status = canonicalize(left, context);
            if (status == CALCULATOR_OK) status = canonicalize(right, context);
            for (size_t i=0; i<6U; i++)
            {
                identity.dimensions[i] = left->dimensions[i] +
                    (op == CALCULATOR_BINARY_DIVIDE ? -right->dimensions[i] : right->dimensions[i]);
                if (identity.dimensions[i] < -DIMENSION_LIMIT || identity.dimensions[i] > DIMENSION_LIMIT)
                    status = CALCULATOR_DIMENSION_ERROR;
            }
            canonical_identity(&identity);
        }
    }
    if (status != CALCULATOR_OK) return status;
    CalculatorExpression a = binding(left, expression->data.binary.left->offset);
    CalculatorExpression b = binding(right, expression->data.binary.right->offset);
    CalculatorExpression operation = *expression;
    operation.data.binary.left = &a;
    operation.data.binary.right = &b;
    status = numeric_value(result, &operation, context, answer, random_state, error);
    if (status == CALCULATOR_OK)
    {
        dimensions_copy(result, &identity);
        if (*temperature_unit) status = reexpress(result, temperature_unit, context);
    }
    return status;
}

static CalculatorStatus quantity_call(CalculatorValue *result, const CalculatorExpression *expression,
    const CalculatorContext *context, const CalculatorValue *answer, uint64_t *random_state, CalculatorError *error)
{
    size_t count = expression->data.call.count;
    CalculatorValue *values = numforge_calloc(count, sizeof(*values));
    CalculatorExpression *bindings = numforge_calloc(count, sizeof(*bindings));
    CalculatorExpression **children = numforge_calloc(count, sizeof(*children));
    CalculatorStatus status = values == NULL || bindings == NULL || children == NULL ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_OK;
    CalculatorFunctionImplementation function = expression->data.call.function->implementation;
    CalculatorValue identity = {0};
    CalculatorContext settings = *context;
    for (size_t i=0; status == CALCULATOR_OK && i<count; i++)
        status = evaluate_recursive(&values[i], expression->data.call.arguments[i], context, answer, random_state, error);
    if (status != CALCULATOR_OK) goto done;
    if (function == CALCULATOR_FUNCTION_QUANTITY)
    {
        if (values[0].quantity) { status = CALCULATOR_DIMENSION_ERROR; goto done; }
        *result = values[0];
        memset(&values[0], 0, sizeof(values[0]));
        unit_dimensions(result, numforge_unit_find(expression->data.call.from_unit));
        goto done;
    }
    if (function == CALCULATOR_FUNCTION_CONVERT)
    {
        if (values[0].quantity)
        {
            unit_dimensions(&identity, numforge_unit_find(expression->data.call.from_unit));
            if (!same_dimensions(&values[0], &identity) || values[0].temperature_point != identity.temperature_point)
                status = CALCULATOR_DIMENSION_ERROR;
            else if (values[0].unit[0] == '\0') status = align(&values[0], &identity, context);
            if (status != CALCULATOR_OK) goto done;
        }
        else unit_dimensions(&values[0], numforge_unit_find(expression->data.call.from_unit));
        status = reexpress(&values[0], expression->data.call.to_unit, context);
        if (status == CALCULATOR_OK)
        {
            *result = values[0];
            memset(&values[0], 0, sizeof(values[0]));
            result->quantity = false;
            result->temperature_point = false;
            memset(result->dimensions, 0, sizeof(result->dimensions));
            result->unit[0] = '\0';
        }
        goto done;
    }
    bool dimensional = false;
    for (size_t i=0; i<count; i++)
        if (values[i].quantity) dimensional = true;
    if (!dimensional) goto numeric_call;
    if (function == CALCULATOR_FUNCTION_POWER)
    {
        CalculatorExpression operation = *expression;
        operation.type = CALCULATOR_EXPRESSION_BINARY;
        operation.data.binary.operation = CALCULATOR_BINARY_POWER;
        operation.data.binary.left = expression->data.call.arguments[0];
        operation.data.binary.right = expression->data.call.arguments[1];
        status = binary_quantity(result, &values[0], &values[1], &operation, context, answer, random_state, error);
        goto done;
    }
    identity = values[0];
    switch (function)
    {
        case CALCULATOR_FUNCTION_ABS:
        case CALCULATOR_FUNCTION_SIGN:
            if (identity.temperature_point) { status = CALCULATOR_DIMENSION_ERROR; break; }
            if (function == CALCULATOR_FUNCTION_SIGN) memset(&identity, 0, sizeof(identity));
            break;
        case CALCULATOR_FUNCTION_MIN:
        case CALCULATOR_FUNCTION_MAX:
        case CALCULATOR_FUNCTION_SUM:
        case CALCULATOR_FUNCTION_MEAN:
        case CALCULATOR_FUNCTION_MEDIAN:
            if (identity.temperature_point && function == CALCULATOR_FUNCTION_SUM)
            { status = CALCULATOR_DIMENSION_ERROR; break; }
            for (size_t i=1; status == CALCULATOR_OK && i<count; i++)
                if (values[i].quantity != identity.quantity) status = CALCULATOR_DIMENSION_ERROR;
                else status = align(&values[i], &identity, context);
            break;
        case CALCULATOR_FUNCTION_FLOOR:
        case CALCULATOR_FUNCTION_CEIL:
        case CALCULATOR_FUNCTION_TRUNC:
        case CALCULATOR_FUNCTION_ROUND:
            if (count > 1U && values[1].quantity) status = CALCULATOR_DIMENSION_ERROR;
            break;
        case CALCULATOR_FUNCTION_SQRT:
        case CALCULATOR_FUNCTION_CBRT:
        case CALCULATOR_FUNCTION_ROOT:
        {
            int degree = function == CALCULATOR_FUNCTION_SQRT ? 2 : 3;
            if (identity.temperature_point || (count > 1U && values[1].quantity))
            { status = CALCULATOR_DIMENSION_ERROR; break; }
            if (function == CALCULATOR_FUNCTION_ROOT) status = small_integer(&values[1], &degree);
            if (degree < 1) status = CALCULATOR_DIMENSION_ERROR;
            if (status != CALCULATOR_OK) break;
            status = canonicalize(&values[0], context);
            for (size_t i=0; i<6U; i++)
            {
                if (identity.dimensions[i] % degree != 0) status = CALCULATOR_DIMENSION_ERROR;
                identity.dimensions[i] /= degree;
            }
            canonical_identity(&identity);
            break;
        }
        case CALCULATOR_FUNCTION_SIN:
        case CALCULATOR_FUNCTION_COS:
        case CALCULATOR_FUNCTION_TAN:
        {
            CalculatorValue angle = {0};
            unit_dimensions(&angle, numforge_unit_find("rad"));
            if (!same_dimensions(&values[0], &angle)) { status = CALCULATOR_DIMENSION_ERROR; break; }
            status = canonicalize(&values[0], context);
            settings.angle_unit = CALCULATOR_ANGLE_RADIANS;
            memset(&identity, 0, sizeof(identity));
            break;
        }
        default: status = CALCULATOR_DIMENSION_ERROR; break;
    }
numeric_call:
    if (status == CALCULATOR_OK)
    {
        for (size_t i=0; i<count; i++)
        {
            bindings[i] = binding(&values[i], expression->data.call.arguments[i]->offset);
            children[i] = &bindings[i];
        }
        CalculatorExpression operation = *expression;
        operation.data.call.arguments = children;
        status = numeric_value(result, &operation, &settings, answer, random_state, error);
        if (status == CALCULATOR_OK) dimensions_copy(result, &identity);
    }
done:
    if (values != NULL)
        for (size_t i=0; i<count; i++) calculator_value_destroy(&values[i]);
    free(values);
    free(bindings);
    free(children);
    return status;
}

static CalculatorStatus evaluate_recursive(CalculatorValue *result, const CalculatorExpression *expression,
    const CalculatorContext *context, const CalculatorValue *answer, uint64_t *random_state, CalculatorError *error)
{
    if (!numforge_budget_check()) return calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
    if (!calculator_expression_has_quantity(expression, answer))
        return numeric_value(result, expression, context, answer, random_state, error);
    CalculatorValue left = {0}, right = {0};
    CalculatorStatus status = CALCULATOR_DIMENSION_ERROR;
    switch (expression->type)
    {
        case CALCULATOR_EXPRESSION_VARIABLE: return calculator_value_copy(result, expression->data.variable.value);
        case CALCULATOR_EXPRESSION_ANSWER: return calculator_value_copy(result, answer);
        case CALCULATOR_EXPRESSION_CALL:
            status = quantity_call(result, expression, context, answer, random_state, error);
            break;
        case CALCULATOR_EXPRESSION_BINARY:
            status = evaluate_recursive(&left, expression->data.binary.left, context, answer, random_state, error);
            if (status == CALCULATOR_OK)
                status = evaluate_recursive(&right, expression->data.binary.right, context, answer, random_state, error);
            if (status == CALCULATOR_OK)
                status = binary_quantity(result, &left, &right, expression, context, answer, random_state, error);
            break;
        case CALCULATOR_EXPRESSION_UNARY:
        case CALCULATOR_EXPRESSION_POSTFIX:
        {
            const CalculatorExpression *operand = expression->type == CALCULATOR_EXPRESSION_UNARY ?
                expression->data.unary.operand : expression->data.postfix.operand;
            status = evaluate_recursive(&left, operand, context, answer, random_state, error);
            if (status != CALCULATOR_OK) break;
            CalculatorExpression child = binding(&left, operand->offset);
            CalculatorExpression operation = *expression;
            if (expression->type == CALCULATOR_EXPRESSION_UNARY)
            {
                if (left.temperature_point && expression->data.unary.operation == CALCULATOR_UNARY_MINUS)
                { status = CALCULATOR_DIMENSION_ERROR; break; }
                operation.data.unary.operand = &child;
                status = numeric_value(result, &operation, context, answer, random_state, error);
                if (status == CALCULATOR_OK) dimensions_copy(result, &left);
            }
            else
            {
                if (!left.quantity)
                {
                    operation.data.postfix.operand = &child;
                    status = numeric_value(result, &operation, context, answer, random_state, error);
                    break;
                }
                if (left.temperature_point || expression->data.postfix.operation == CALCULATOR_POSTFIX_FACTORIAL)
                { status = CALCULATOR_DIMENSION_ERROR; break; }
                status = canonicalize(&left, context);
                if (status != CALCULATOR_OK) break;
                operation.data.postfix.operand = &child;
                status = numeric_value(result, &operation, context, answer, random_state, error);
                if (status == CALCULATOR_OK)
                {
                    int power = expression->data.postfix.operation == CALCULATOR_POSTFIX_SQUARE ? 2 : 3;
                    for (size_t i=0; i<6U; i++)
                    {
                        result->dimensions[i] = left.dimensions[i] * power;
                        if (result->dimensions[i] < -DIMENSION_LIMIT || result->dimensions[i] > DIMENSION_LIMIT)
                            status = CALCULATOR_DIMENSION_ERROR;
                    }
                    canonical_identity(result);
                }
            }
            break;
        }
        default: break;
    }
    calculator_value_destroy(&left);
    calculator_value_destroy(&right);
    if (status != CALCULATOR_OK && (error == NULL || error->status != status))
        calculator_error_set(error, status, expression->offset);
    return status;
}

CalculatorStatus calculator_evaluate_quantity(CalculatorValue *result, const CalculatorExpression *expression,
    const CalculatorContext *context, const CalculatorValue *answer, uint64_t *random_state, CalculatorError *error)
{
    CalculatorStatus status = evaluate_recursive(result, expression, context, answer, random_state, error);
    if (status == CALCULATOR_OK) result->context = *context;
    return status;
}

CalculatorStatus calculator_quantity_symbol(const CalculatorValue *value, char *text, size_t capacity)
{
    if (value->unit[0] != '\0')
    {
        const NumForgeUnitInfo *unit = numforge_unit_find(value->unit);
        if (unit == NULL || strlen(unit->symbol) >= capacity) return CALCULATOR_VALUE_TOO_LARGE;
        memcpy(text, unit->symbol, strlen(unit->symbol)+1U);
        return CALCULATOR_OK;
    }
    static const char *const symbols[] = {"m", "kg", "s", "deltaK", "bit", "rad"};
    size_t used = 0;
    for (size_t i=0; i<6U; i++)
    {
        int power = value->dimensions[i];
        if (power == 0) continue;
        int length = power == 1 ? snprintf(text + used, capacity - used, "%s%s", used ? "*" : "", symbols[i]) :
            snprintf(text + used, capacity - used, "%s%s^%d", used ? "*" : "", symbols[i], power);
        if (length < 0 || (size_t)length >= capacity-used) return CALCULATOR_VALUE_TOO_LARGE;
        used += (size_t)length;
    }
    return CALCULATOR_OK;
}
