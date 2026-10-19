#include "src/application/expressions/expression_domain_internal.h"
#include "src/engine/api/status.h"
#include "src/runtime/numforge_alloc.h"
#include <stdlib.h>
#include <string.h>

typedef struct Range {
    BigRational *low, *high;
    bool known, continuous;
} Range;
typedef struct DomainWork {
    const char *parameter;
    const BigRational *lower, *upper;
    BigRational *zero, *one, *negative_one;
    size_t nodes;
} DomainWork;

static void destroy(Range *range)
{
    bigrational_destroy(range->low); bigrational_destroy(range->high);
}
static CalculatorStatus create(Range *range)
{
    range->low = bigrational_create(); range->high = bigrational_create();
    return range->low && range->high ? CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
}
static CalculatorStatus copy(Range *target, const BigRational *low, const BigRational *high)
{
    CalculatorStatus status = calculator_from_rational_status(bigrational_copy(target->low, low));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_copy(target->high, high));
    if (status == CALCULATOR_OK) target->known = true;
    return status;
}
static CalculatorStatus number(BigRational *target, const char *text)
{
    BigDecimal *value = bigdecimal_create();
    CalculatorStatus status = value ? calculator_from_decimal_status(bigdecimal_set_string(value, text)) : CALCULATOR_OUT_OF_MEMORY;
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_from_bigdecimal(target, value));
    bigdecimal_destroy(value); return status;
}
static CalculatorStatus compare(int *order, const BigRational *a, const BigRational *b)
{
    return calculator_from_rational_status(bigrational_compare(order, a, b));
}
static CalculatorStatus signs(const Range *range, DomainWork *work, int *low, int *high)
{
    CalculatorStatus status = compare(low, range->low, work->zero);
    if (status == CALCULATOR_OK) status = compare(high, range->high, work->zero);
    return status;
}
static CalculatorStatus multiply(Range *target, const Range *a, const Range *b, bool divide)
{
    BigRational *term = bigrational_create();
    CalculatorStatus status = term ? CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
    const BigRational *first[] = {a->low, a->high}, *second[] = {b->low, b->high};
    for (size_t i = 0U; i < 2U && status == CALCULATOR_OK; ++i)
        for (size_t j = 0U; j < 2U && status == CALCULATOR_OK; ++j) {
            status = calculator_from_rational_status(divide ? bigrational_div(term, first[i], second[j]) : bigrational_mul(term, first[i], second[j]));
            if (status != CALCULATOR_OK) break;
            if (!i && !j) status = copy(target, term, term);
            else {
                int order = 0;
                status = compare(&order, term, target->low);
                if (status == CALCULATOR_OK && order < 0) status = calculator_from_rational_status(bigrational_copy(target->low, term));
                if (status == CALCULATOR_OK) status = compare(&order, term, target->high);
                if (status == CALCULATOR_OK && order > 0) status = calculator_from_rational_status(bigrational_copy(target->high, term));
            }
        }
    bigrational_destroy(term); return status;
}
static CalculatorStatus binary(Range *result, const Range *a, const Range *b,
    CalculatorBinaryOperator operation, DomainWork *work)
{
    if (!a->continuous || !b->continuous) return CALCULATOR_OK;
    CalculatorStatus status = CALCULATOR_OK;
    if (operation == CALCULATOR_BINARY_DIVIDE) {
        if (!b->known) return CALCULATOR_OK;
        int low = 0, high = 0; status = signs(b, work, &low, &high);
        if (status != CALCULATOR_OK || (low <= 0 && high >= 0)) return status;
    }
    result->continuous = true;
    if (!a->known || !b->known) return status;
    if (operation == CALCULATOR_BINARY_MULTIPLY || operation == CALCULATOR_BINARY_DIVIDE)
        return multiply(result, a, b, operation == CALCULATOR_BINARY_DIVIDE);
    status = calculator_from_rational_status(operation == CALCULATOR_BINARY_ADD
        ? bigrational_add(result->low, a->low, b->low) : bigrational_sub(result->low, a->low, b->high));
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(operation == CALCULATOR_BINARY_ADD
        ? bigrational_add(result->high, a->high, b->high) : bigrational_sub(result->high, a->high, b->low));
    if (status == CALCULATOR_OK) result->known = true;
    return status;
}
static CalculatorStatus integer_power(Range *result, const Range *base, int power, DomainWork *work)
{
    if (!base->continuous) return CALCULATOR_OK;
    result->continuous = true;
    if (!base->known) { if (power <= 0) result->continuous = false; return CALCULATOR_OK; }
    int low = 0, high = 0;
    CalculatorStatus status = signs(base, work, &low, &high);
    if (status != CALCULATOR_OK) return status;
    /* Exclude 0^0 too, independently of evaluator conventions. */
    if (power <= 0 && low <= 0 && high >= 0) { result->continuous = false; return status; }
    Range accumulator = {0}, next = {0};
    status = create(&accumulator);
    if (status == CALCULATOR_OK) status = create(&next);
    if (status == CALCULATOR_OK) status = copy(&accumulator, work->one, work->one);
    unsigned count = (unsigned)(power < 0 ? -power : power);
    for (unsigned i = 0U; i < count && status == CALCULATOR_OK; ++i) {
        status = multiply(&next, &accumulator, base, false);
        if (status == CALCULATOR_OK) status = copy(&accumulator, next.low, next.high);
    }
    if (status == CALCULATOR_OK && !(count % 2U) && low <= 0 && high >= 0)
        status = calculator_from_rational_status(bigrational_copy(accumulator.low, work->zero));
    if (status == CALCULATOR_OK) {
        if (power < 0) {
            Range one = {work->one, work->one, true, true};
            status = multiply(result, &one, &accumulator, true);
        } else status = copy(result, accumulator.low, accumulator.high);
    }
    destroy(&accumulator); destroy(&next); return status;
}
static CalculatorStatus power(Range *result, const Range *base, const Range *exponent, DomainWork *work)
{
    if (!base->continuous || !exponent->continuous) return CALCULATOR_OK;
    int order = 1;
    CalculatorStatus status = exponent->known ? compare(&order, exponent->low, exponent->high) : CALCULATOR_OK;
    char *text = NULL;
    if (status == CALCULATOR_OK && !order) status = calculator_from_rational_status(bigrational_to_string(exponent->low, &text));
    if (status == CALCULATOR_OK && text && !strchr(text, '/')) {
        size_t length = strlen(text);
        if (length <= 3U) {
            int value = atoi(text);
            if (value >= -16 && value <= 16) { free(text); return integer_power(result, base, value, work); }
        }
        /* All positive integral powers are continuous even if range propagation
         * would be too expensive. Negative powers must exclude zero. */
        result->continuous = text[0] != '-';
        if (!result->continuous && base->known) {
            int low = 0, high = 0; status = signs(base, work, &low, &high);
            result->continuous = low > 0 || high < 0;
        }
    } else if (status == CALCULATOR_OK && base->known) {
        status = compare(&order, base->low, work->zero);
        result->continuous = order > 0; /* a(x)^b(x), a strictly positive */
    }
    free(text); return status;
}
static CalculatorStatus function(Range *result, const Range *a,
    CalculatorFunctionImplementation implementation, DomainWork *work)
{
    if (!a->continuous) return CALCULATOR_OK;
    CalculatorStatus status = CALCULATOR_OK; int low = 0, high = 0, order = 0;
    switch (implementation) {
        case CALCULATOR_FUNCTION_SIN:
        case CALCULATOR_FUNCTION_COS:
        case CALCULATOR_FUNCTION_TANH:
            result->continuous = true; return copy(result, work->negative_one, work->one);
        case CALCULATOR_FUNCTION_EXP:
        case CALCULATOR_FUNCTION_SINH:
        case CALCULATOR_FUNCTION_COSH:
        case CALCULATOR_FUNCTION_ATAN:
        case CALCULATOR_FUNCTION_ASINH:
        case CALCULATOR_FUNCTION_CBRT:
        case CALCULATOR_FUNCTION_RADIANS:
        case CALCULATOR_FUNCTION_DEGREES:
            result->continuous = true; return status; /* valid, unknown range */
        case CALCULATOR_FUNCTION_ABS:
            result->continuous = true;
            if (!a->known) return status;
            status = signs(a, work, &low, &high);
            if (status != CALCULATOR_OK) return status;
            if (low >= 0) return copy(result, a->low, a->high);
            if (high <= 0) {
                status = calculator_from_rational_status(bigrational_negate(result->low, a->high));
                if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_negate(result->high, a->low));
            } else {
                status = calculator_from_rational_status(bigrational_negate(result->high, a->low));
                if (status == CALCULATOR_OK) status = compare(&order, result->high, a->high);
                if (status == CALCULATOR_OK && order < 0) status = calculator_from_rational_status(bigrational_copy(result->high, a->high));
                /* result->low was initialized to zero. */
            }
            if (status == CALCULATOR_OK) result->known = true;
            return status;
        case CALCULATOR_FUNCTION_SQRT:
        case CALCULATOR_FUNCTION_LN:
        case CALCULATOR_FUNCTION_LOG:
            if (!a->known) return status;
            status = signs(a, work, &low, &high);
            if (status != CALCULATOR_OK || low < 0 || (implementation != CALCULATOR_FUNCTION_SQRT && !low)) return status;
            result->continuous = true;
            if (implementation == CALCULATOR_FUNCTION_LOG) return status;
            if (implementation == CALCULATOR_FUNCTION_LN) {
                /* 1-1/t <= ln(t) <= t-1 for all t>0. */
                status = calculator_from_rational_status(bigrational_div(result->low, work->one, a->low));
                if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_sub(result->low, work->one, result->low));
                if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_sub(result->high, a->high, work->one));
            } else {
                /* min(t,1) <= sqrt(t) <= max(t,1), t>=0. No rounded
                 * transcendental endpoint is treated as an enclosure. */
                status = compare(&order, a->low, work->one);
                if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_copy(result->low, order < 0 ? a->low : work->one));
                if (status == CALCULATOR_OK) status = compare(&order, a->high, work->one);
                if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_copy(result->high, order > 0 ? a->high : work->one));
            }
            if (status == CALCULATOR_OK) result->known = true;
            return status;
        /* Step functions, poles/branches not covered by these enclosures,
         * statistics and complex/quantity operations remain unverified. */
        default: return status;
    }
}
static CalculatorStatus node_range(Range *result, const CalculatorExpression *node, DomainWork *work)
{
    if (!numforge_budget_check()) return calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
    if (++work->nodes > 2048U) return CALCULATOR_OK;
    Range a = {0}, b = {0}; CalculatorStatus status = CALCULATOR_OK;
    switch (node->type) {
        case CALCULATOR_EXPRESSION_NUMBER:
            status = number(result->low, node->data.number.text);
            if (status == CALCULATOR_OK) status = copy(result, result->low, result->low);
            result->continuous = status == CALCULATOR_OK;
            break;
        case CALCULATOR_EXPRESSION_CONSTANT: {
            const char *low = node->data.constant.constant == CALCULATOR_CONSTANT_PI ? "3" : node->data.constant.constant == CALCULATOR_CONSTANT_E ? "2" : "1";
            const char *high = node->data.constant.constant == CALCULATOR_CONSTANT_PI ? "4" : node->data.constant.constant == CALCULATOR_CONSTANT_E ? "3" : "2";
            status = number(result->low, low);
            if (status == CALCULATOR_OK) status = number(result->high, high);
            result->known = result->continuous = status == CALCULATOR_OK;
            break;
        }
        case CALCULATOR_EXPRESSION_VARIABLE:
            if (!strcmp(node->data.variable.name, work->parameter)) {
                status = copy(result, work->lower, work->upper); result->continuous = true;
            }
            break;
        case CALCULATOR_EXPRESSION_UNARY:
        case CALCULATOR_EXPRESSION_POSTFIX:
        case CALCULATOR_EXPRESSION_BINARY:
        case CALCULATOR_EXPRESSION_CALL: {
            status = create(&a);
            if (status != CALCULATOR_OK) break;
            const CalculatorExpression *first = node->type == CALCULATOR_EXPRESSION_UNARY ? node->data.unary.operand :
                node->type == CALCULATOR_EXPRESSION_POSTFIX ? node->data.postfix.operand :
                node->type == CALCULATOR_EXPRESSION_BINARY ? node->data.binary.left : node->data.call.count ? node->data.call.arguments[0] : NULL;
            if (!first) break;
            status = node_range(&a, first, work);
            if (status != CALCULATOR_OK) break;
            if (node->type == CALCULATOR_EXPRESSION_UNARY) {
                result->continuous = a.continuous;
                if (a.known) {
                    if (node->data.unary.operation == CALCULATOR_UNARY_PLUS) status = copy(result, a.low, a.high);
                    else {
                        status = calculator_from_rational_status(bigrational_negate(result->low, a.high));
                        if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_negate(result->high, a.low));
                        result->known = status == CALCULATOR_OK;
                    }
                }
            } else if (node->type == CALCULATOR_EXPRESSION_POSTFIX) {
                if (node->data.postfix.operation != CALCULATOR_POSTFIX_FACTORIAL)
                    status = integer_power(result, &a, node->data.postfix.operation == CALCULATOR_POSTFIX_SQUARE ? 2 : 3, work);
            } else if (node->type == CALCULATOR_EXPRESSION_BINARY ||
                (node->data.call.function->implementation == CALCULATOR_FUNCTION_POWER && node->data.call.count == 2U)) {
                status = create(&b);
                bool call = node->type == CALCULATOR_EXPRESSION_CALL;
                if (status == CALCULATOR_OK) status = node_range(&b, call ? node->data.call.arguments[1] : node->data.binary.right, work);
                CalculatorBinaryOperator operation = call ? CALCULATOR_BINARY_POWER : node->data.binary.operation;
                if (status == CALCULATOR_OK) status = operation == CALCULATOR_BINARY_POWER ? power(result, &a, &b, work) : binary(result, &a, &b, operation, work);
            } else if (node->data.call.count == 1U) status = function(result, &a, node->data.call.function->implementation, work);
            break;
        }
        default: break;
    }
    destroy(&a); destroy(&b); return status;
}
CalculatorStatus application_expression_domain_tree(bool *verified,
    const CalculatorExpression *tree, const char *parameter,
    const BigRational *lower, const BigRational *upper)
{
    DomainWork work = {parameter, lower, upper, NULL, NULL, NULL, 0U}; Range range = {0};
    work.zero = bigrational_create(); work.one = bigrational_create(); work.negative_one = bigrational_create();
    CalculatorStatus status = work.zero && work.one && work.negative_one ? create(&range) : CALCULATOR_OUT_OF_MEMORY;
    if (status == CALCULATOR_OK) status = number(work.one, "1");
    if (status == CALCULATOR_OK) status = number(work.negative_one, "-1");
    if (status == CALCULATOR_OK) status = node_range(&range, tree, &work);
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) *verified = range.continuous;
    destroy(&range); bigrational_destroy(work.zero); bigrational_destroy(work.one); bigrational_destroy(work.negative_one);
    return status;
}
