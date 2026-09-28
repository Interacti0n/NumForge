#ifndef NUMFORGE_CALCULATOR_EVALUATOR_INTERNAL_H
#define NUMFORGE_CALCULATOR_EVALUATOR_INTERNAL_H

#include "evaluator.h"
#include "constants.h"

#include <numforge/bigint.h>

/* Shared only by the decimal evaluator and its function families. */
typedef struct CalculatorConstantCache
{
    BigDecimal *values[CALCULATOR_CONSTANT_COUNT];
    int64_t digits[CALCULATOR_CONSTANT_COUNT];
} CalculatorConstantCache;

typedef struct CalculatorEvaluation
{
    const CalculatorContext *context;
    CalculatorConstantCache *constant_cache;
    const BigDecimal *answer;
    const CalculatorValue *typed_answer;
    uint64_t *random_state;
} CalculatorEvaluation;

CalculatorStatus calculator_evaluate_expression(
    BigDecimal **result, const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation, CalculatorError *error);
CalculatorStatus calculator_from_bigdecimal_status(BigDecimalStatus status);
CalculatorStatus calculator_from_bigint_status(BigIntStatus status);
bool calculator_time_limit_reached(const CalculatorEvaluation *evaluation);
bool calculator_nonnegative_integer(const BigDecimal *value);
CalculatorStatus calculator_bigdecimal_to_bigint(BigInt **result, const BigDecimal *value, bool signed_input);
CalculatorStatus calculator_set_bigdecimal_from_bigint(BigDecimal *result, const BigInt *value);
CalculatorStatus calculator_evaluate_integer_call(
    BigDecimal **result, const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation, CalculatorError *error);
CalculatorStatus calculator_evaluate_root_call(
    BigDecimal **result, const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation, CalculatorError *error);
CalculatorStatus calculator_evaluate_transcendental_call(
    BigDecimal **result, const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation, CalculatorError *error);
CalculatorStatus calculator_evaluate_hyperbolic_call(
    BigDecimal **result, const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation, CalculatorError *error);
CalculatorStatus calculator_evaluate_trigonometric_call(
    BigDecimal **result, const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation, CalculatorError *error);
CalculatorStatus calculator_evaluate_basic_call(
    BigDecimal **result, const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation, CalculatorError *error);
CalculatorStatus calculator_evaluate_aggregate_call(
    BigDecimal **result, const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation, CalculatorError *error);
CalculatorStatus calculator_evaluate_rounding_call(
    BigDecimal **result, const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation, CalculatorError *error);

#endif
