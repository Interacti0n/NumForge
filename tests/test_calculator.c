#include <stdlib.h>
#include <string.h>

#include <unity.h>

#include "calculator_internal.h"
#include "evaluator.h"
#include "expression_internal.h"
#include "formatter.h"
#include "parser.h"
#include "tokenizer.h"

void setUp(void)
{
}

void tearDown(void)
{
}

/* ============================================================
   Test helpers
   ============================================================ */

static void assert_next_token(
    CalculatorTokenizer *tokenizer,
    CalculatorTokenType expected_type,
    const char *expected_text,
    size_t expected_length,
    size_t expected_offset
)
{
    CalculatorToken token;
    CalculatorError error;

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_tokenizer_next(tokenizer, &token, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, error.status);
    TEST_ASSERT_EQUAL(expected_type, token.type);
    TEST_ASSERT_EQUAL_UINT(expected_length, token.length);
    TEST_ASSERT_EQUAL_UINT(expected_offset, token.offset);
    if (expected_length != 0)
    {
        TEST_ASSERT_EQUAL_MEMORY(expected_text, token.text, expected_length);
    }
}

static CalculatorExpression *parse_expression(const char *input)
{
    CalculatorExpression *expression = NULL;
    CalculatorError error;

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_parse(input, &expression, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, error.status);
    TEST_ASSERT_NOT_NULL(expression);
    return expression;
}

static void assert_decimal_equals(const char *expected, const BigDecimal *value)
{
    char *actual = NULL;

    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(value, &actual));
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
    free(actual);
}

static BigDecimal *evaluate_expression(const char *input, const CalculatorContext *context)
{
    CalculatorExpression *expression = parse_expression(input);
    CalculatorError error;
    BigDecimal *result = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_evaluate(result, expression, context, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, error.status);
    calculator_expression_destroy(expression);
    return result;
}

static void assert_formatted_expression(
    const char *expected,
    const char *input,
    const CalculatorContext *context
)
{
    BigDecimal *value = evaluate_expression(input, context);
    char *text = NULL;

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_format_result(value, context, &text));
    TEST_ASSERT_EQUAL_STRING(expected, text);
    free(text);
    bigdecimal_destroy(value);
}

static void assert_expression_too_deep(const char *input)
{
    CalculatorExpression *expression = NULL;
    CalculatorError error;

    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE,
                      calculator_parse(input, &expression, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, error.status);
    TEST_ASSERT_NULL(expression);
}

/* ============================================================
   Shared calculator utilities
   ============================================================ */

void test_context_defaults_and_status_strings(void)
{
    CalculatorContext context;

    calculator_context_init(&context);

    TEST_ASSERT_EQUAL_INT64(CALCULATOR_DEFAULT_DIVISION_SCALE, context.division_scale);
    TEST_ASSERT_EQUAL_INT64(CALCULATOR_DEFAULT_OUTPUT_SCALE, context.output_scale);
    TEST_ASSERT_EQUAL(CALCULATOR_ANGLE_RADIANS, context.angle_unit);
    TEST_ASSERT_EQUAL_INT64(CALCULATOR_DEFAULT_TIME_LIMIT_MS, context.time_limit_ms);
    TEST_ASSERT_EQUAL(BIGDECIMAL_ROUND_HALF_EVEN, context.rounding);
    TEST_ASSERT_EQUAL_STRING("syntax error", calculator_status_to_string(CALCULATOR_SYNTAX_ERROR));
    TEST_ASSERT_EQUAL_STRING("TLE: time limit exceeded", calculator_status_to_string(CALCULATOR_TIME_LIMIT));
    TEST_ASSERT_EQUAL_STRING("unknown status", calculator_status_to_string((CalculatorStatus)999));
}

void test_context_configures_angle_units(void)
{
    CalculatorContext context;

    calculator_context_init(&context);
    TEST_ASSERT_EQUAL(
        CALCULATOR_OK,
        calculator_context_set_angle_unit(&context, CALCULATOR_ANGLE_DEGREES));
    TEST_ASSERT_EQUAL(CALCULATOR_ANGLE_DEGREES, context.angle_unit);
    TEST_ASSERT_EQUAL(
        CALCULATOR_INVALID_ARGUMENT,
        calculator_context_set_angle_unit(&context, (CalculatorAngleUnit)99));
    TEST_ASSERT_EQUAL(CALCULATOR_ANGLE_DEGREES, context.angle_unit);
    TEST_ASSERT_EQUAL(
        CALCULATOR_NULL_ARGUMENT,
        calculator_context_set_angle_unit(NULL, CALCULATOR_ANGLE_RADIANS));
}

void test_context_configures_output_precision(void)
{
    CalculatorContext context;

    calculator_context_init(&context);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_context_set_output_scale(&context, 48));
    TEST_ASSERT_EQUAL_INT64(48, context.output_scale);
    TEST_ASSERT_EQUAL_INT64(48 + CALCULATOR_DIVISION_GUARD_DIGITS, context.division_scale);

    TEST_ASSERT_EQUAL(CALCULATOR_OK,
                      calculator_context_set_output_scale(&context, CALCULATOR_UNLIMITED_OUTPUT_SCALE));
    TEST_ASSERT_EQUAL_INT64(CALCULATOR_UNLIMITED_OUTPUT_SCALE, context.output_scale);
    TEST_ASSERT_EQUAL_INT64(CALCULATOR_DEFAULT_DIVISION_SCALE, context.division_scale);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, calculator_context_set_output_scale(&context, -2));
    TEST_ASSERT_EQUAL(CALCULATOR_SCALE_OVERFLOW, calculator_context_set_output_scale(&context, INT64_MAX));
    TEST_ASSERT_EQUAL_INT64(CALCULATOR_UNLIMITED_OUTPUT_SCALE, context.output_scale);
    TEST_ASSERT_EQUAL_INT64(CALCULATOR_DEFAULT_DIVISION_SCALE, context.division_scale);
}

void test_error_helpers(void)
{
    CalculatorError error;

    calculator_error_set(&error, CALCULATOR_INVALID_TOKEN, 7);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_TOKEN, error.status);
    TEST_ASSERT_EQUAL_UINT(7, error.offset);

    calculator_error_clear(&error);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, error.status);
    TEST_ASSERT_EQUAL_UINT(0, error.offset);

    TEST_ASSERT_EQUAL_UINT(1, calculator_error_column(NULL, 10));
    TEST_ASSERT_EQUAL_UINT(1, calculator_error_column("", 0));
    TEST_ASSERT_EQUAL_UINT(5, calculator_error_column("\xCF\x80 + ?", 5));
    TEST_ASSERT_EQUAL_UINT(1, calculator_error_column("1 +\n)", 4));
}

/* ============================================================
   Tokenizer
   ============================================================ */

void test_tokenizer_produces_numbers_operators_and_offsets(void)
{
    CalculatorTokenizer tokenizer;

    TEST_ASSERT_EQUAL(CALCULATOR_OK,
                      calculator_tokenizer_init(&tokenizer, " \t12.5E-2 + (.3 * 1.) / 4\r\n"));
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_NUMBER, "12.5E-2", 7, 2);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_PLUS, "+", 1, 10);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_LEFT_PAREN, "(", 1, 12);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_NUMBER, ".3", 2, 13);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_STAR, "*", 1, 16);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_NUMBER, "1.", 2, 18);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_RIGHT_PAREN, ")", 1, 20);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_SLASH, "/", 1, 22);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_NUMBER, "4", 1, 24);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_END, "", 0, 27);
}

void test_tokenizer_keeps_signs_as_operators(void)
{
    CalculatorTokenizer tokenizer;

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_tokenizer_init(&tokenizer, "-2 + +.5"));
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_MINUS, "-", 1, 0);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_NUMBER, "2", 1, 1);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_PLUS, "+", 1, 3);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_PLUS, "+", 1, 5);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_NUMBER, ".5", 2, 6);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_END, "", 0, 8);
}

void test_tokenizer_accepts_comma_decimal_separator(void)
{
    CalculatorTokenizer tokenizer;

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_tokenizer_init(&tokenizer, ".5 + 1,25E-1"));
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_NUMBER, ".5", 2, 0);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_PLUS, "+", 1, 3);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_NUMBER, "1,25E-1", 7, 5);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_END, "", 0, 12);
}

void test_tokenizer_produces_identifiers(void)
{
    CalculatorTokenizer tokenizer;

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_tokenizer_init(&tokenizer, "PI + e + phi"));
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_IDENTIFIER, "PI", 2, 0);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_PLUS, "+", 1, 3);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_IDENTIFIER, "e", 1, 5);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_PLUS, "+", 1, 7);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_IDENTIFIER, "phi", 3, 9);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_END, "", 0, 12);
}

void test_tokenizer_separates_constant_e_from_scientific_notation(void)
{
    CalculatorTokenizer tokenizer;

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_tokenizer_init(&tokenizer, "1e3 + 1E3 + \xCF\x80"));
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_NUMBER, "1", 1, 0);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_IDENTIFIER, "e", 1, 1);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_NUMBER, "3", 1, 2);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_PLUS, "+", 1, 4);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_NUMBER, "1E3", 3, 6);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_PLUS, "+", 1, 10);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_IDENTIFIER, "\xCF\x80", 2, 12);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_END, "", 0, 14);
}

void test_tokenizer_produces_postfix_operators(void)
{
    CalculatorTokenizer tokenizer;

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_tokenizer_init(&tokenizer, "2\xC2\xB2 + 3\xC2\xB3 + 5!"));
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_NUMBER, "2", 1, 0);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_SQUARE, "\xC2\xB2", 2, 1);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_PLUS, "+", 1, 4);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_NUMBER, "3", 1, 6);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_CUBE, "\xC2\xB3", 2, 7);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_PLUS, "+", 1, 10);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_NUMBER, "5", 1, 12);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_FACTORIAL, "!", 1, 13);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_END, "", 0, 14);
}

void test_tokenizer_produces_power_operator(void)
{
    CalculatorTokenizer tokenizer;

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_tokenizer_init(&tokenizer, "1.5^3"));
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_NUMBER, "1.5", 3, 0);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_CARET, "^", 1, 3);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_NUMBER, "3", 1, 4);
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_END, "", 0, 5);
}

void test_tokenizer_rejects_invalid_tokens(void)
{
    const char *input[] = { ".", "@" };
    const size_t expected_offset[] = { 0, 0 };

    for (size_t index = 0; index < sizeof(input) / sizeof(input[0]); index++)
    {
        CalculatorTokenizer tokenizer;
        CalculatorToken token;
        CalculatorError error;

        TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_tokenizer_init(&tokenizer, input[index]));
        TEST_ASSERT_EQUAL(CALCULATOR_INVALID_TOKEN,
                          calculator_tokenizer_next(&tokenizer, &token, &error));
        TEST_ASSERT_EQUAL(CALCULATOR_INVALID_TOKEN, error.status);
        TEST_ASSERT_EQUAL_UINT(expected_offset[index], error.offset);
    }

    TEST_ASSERT_EQUAL(CALCULATOR_NULL_ARGUMENT, calculator_tokenizer_init(NULL, "1"));
}

/* ============================================================
   Parser and AST
   ============================================================ */

void test_parser_accepts_expression_grammar(void)
{
    const char *input[] = { "1", "-1", "+.5", "\xCF\x80", "\xCF\x80\x65", "\xCF\x86", "1 + 2 * 3", "2(1 + 2)", "1E-2 / .5", "2\xC2\xB2", "3\xC2\xB3", "5!", "(2 + 3)!", "1.5^3", "2^3^2", "2^-3" };

    for (size_t index = 0; index < sizeof(input) / sizeof(input[0]); index++)
    {
        CalculatorExpression *expression = parse_expression(input[index]);
        calculator_expression_destroy(expression);
    }
}

void test_parser_rejects_unknown_identifier(void)
{
    CalculatorExpression *expression = NULL;
    CalculatorError error;

    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_TOKEN, calculator_parse("pi", &expression, &error));
    TEST_ASSERT_NULL(expression);
    TEST_ASSERT_EQUAL_UINT(0, error.offset);
}

void test_parser_builds_precedence_and_associativity(void)
{
    CalculatorExpression *expression = parse_expression("-1 + 2 * 3");

    TEST_ASSERT_EQUAL(CALCULATOR_EXPRESSION_BINARY, expression->type);
    TEST_ASSERT_EQUAL(CALCULATOR_BINARY_ADD, expression->data.binary.operation);
    TEST_ASSERT_EQUAL(CALCULATOR_EXPRESSION_UNARY, expression->data.binary.left->type);
    TEST_ASSERT_EQUAL(CALCULATOR_UNARY_MINUS, expression->data.binary.left->data.unary.operation);
    TEST_ASSERT_EQUAL(CALCULATOR_EXPRESSION_BINARY, expression->data.binary.right->type);
    TEST_ASSERT_EQUAL(CALCULATOR_BINARY_MULTIPLY, expression->data.binary.right->data.binary.operation);
    calculator_expression_destroy(expression);

    expression = parse_expression("1 - 2 - 3");
    TEST_ASSERT_EQUAL(CALCULATOR_EXPRESSION_BINARY, expression->type);
    TEST_ASSERT_EQUAL(CALCULATOR_BINARY_SUBTRACT, expression->data.binary.operation);
    TEST_ASSERT_EQUAL(CALCULATOR_EXPRESSION_BINARY, expression->data.binary.left->type);
    TEST_ASSERT_EQUAL(CALCULATOR_BINARY_SUBTRACT, expression->data.binary.left->data.binary.operation);
    calculator_expression_destroy(expression);

    expression = parse_expression("2^3^2");
    TEST_ASSERT_EQUAL(CALCULATOR_EXPRESSION_BINARY, expression->type);
    TEST_ASSERT_EQUAL(CALCULATOR_BINARY_POWER, expression->data.binary.operation);
    TEST_ASSERT_EQUAL(CALCULATOR_EXPRESSION_BINARY, expression->data.binary.right->type);
    TEST_ASSERT_EQUAL(CALCULATOR_BINARY_POWER, expression->data.binary.right->data.binary.operation);
    calculator_expression_destroy(expression);
}

void test_parser_reports_syntax_and_lexical_errors(void)
{
    const char *input[] = { "", "()", "(1", "1 +", "1)", "+)", "1 @ 2" };
    const CalculatorStatus expected_status[] = {
        CALCULATOR_SYNTAX_ERROR,
        CALCULATOR_SYNTAX_ERROR,
        CALCULATOR_SYNTAX_ERROR,
        CALCULATOR_SYNTAX_ERROR,
        CALCULATOR_SYNTAX_ERROR,
        CALCULATOR_SYNTAX_ERROR,
        CALCULATOR_INVALID_TOKEN
    };
    const size_t expected_offset[] = { 0, 1, 2, 3, 1, 1, 2 };

    for (size_t index = 0; index < sizeof(input) / sizeof(input[0]); index++)
    {
        CalculatorExpression *expression = NULL;
        CalculatorError error;

        TEST_ASSERT_EQUAL(expected_status[index], calculator_parse(input[index], &expression, &error));
        TEST_ASSERT_NULL(expression);
        TEST_ASSERT_EQUAL(expected_status[index], error.status);
        TEST_ASSERT_EQUAL_UINT(expected_offset[index], error.offset);
    }
}

void test_parser_rejects_excessive_recursion_and_ast_depth(void)
{
    char parentheses[2U * (CALCULATOR_MAX_EXPRESSION_DEPTH + 1U) + 2U];
    char unary[CALCULATOR_MAX_EXPRESSION_DEPTH + 3U];
    char power[2U * (CALCULATOR_MAX_EXPRESSION_DEPTH + 1U) + 2U];
    char addition[2U * (CALCULATOR_MAX_EXPRESSION_DEPTH + 1U) + 2U];
    size_t count = CALCULATOR_MAX_EXPRESSION_DEPTH + 1U;
    size_t offset = 0U;
    size_t index;
    CalculatorExpression *expression;

    for (index = 0U; index < CALCULATOR_MAX_EXPRESSION_DEPTH; index++)
    {
        parentheses[offset++] = '(';
    }
    parentheses[offset++] = '1';
    for (index = 0U; index < CALCULATOR_MAX_EXPRESSION_DEPTH; index++)
    {
        parentheses[offset++] = ')';
    }
    parentheses[offset] = '\0';
    expression = parse_expression(parentheses);
    calculator_expression_destroy(expression);

    offset = 0U;
    for (index = 0U; index + 1U < CALCULATOR_MAX_EXPRESSION_DEPTH; index++)
    {
        addition[offset++] = '1';
        addition[offset++] = '+';
    }
    addition[offset++] = '1';
    addition[offset] = '\0';
    expression = parse_expression(addition);
    calculator_expression_destroy(expression);

    offset = 0U;
    for (index = 0U; index < count; index++) parentheses[offset++] = '(';
    parentheses[offset++] = '1';
    for (index = 0U; index < count; index++) parentheses[offset++] = ')';
    parentheses[offset] = '\0';
    assert_expression_too_deep(parentheses);

    for (index = 0U; index < count; index++) unary[index] = '-';
    unary[count] = '1';
    unary[count + 1U] = '\0';
    assert_expression_too_deep(unary);

    offset = 0U;
    for (index = 0U; index < count; index++)
    {
        power[offset++] = '1';
        power[offset++] = '^';
    }
    power[offset++] = '1';
    power[offset] = '\0';
    assert_expression_too_deep(power);

    offset = 0U;
    for (index = 0U; index < count; index++)
    {
        addition[offset++] = '1';
        addition[offset++] = '+';
    }
    addition[offset++] = '1';
    addition[offset] = '\0';
    assert_expression_too_deep(addition);
}

/* ============================================================
   Evaluator
   ============================================================ */

void test_evaluator_respects_precedence_and_parentheses(void)
{
    CalculatorContext context;
    BigDecimal *result;

    calculator_context_init(&context);

    result = evaluate_expression("1 + 2 * (3 - .5)", &context);
    assert_decimal_equals("6", result);
    bigdecimal_destroy(result);

    result = evaluate_expression("(1 + 2) * 3", &context);
    assert_decimal_equals("9", result);
    bigdecimal_destroy(result);

    result = evaluate_expression("-1.5 + +2", &context);
    assert_decimal_equals("0.5", result);
    bigdecimal_destroy(result);

    result = evaluate_expression("0,1 + 0,2", &context);
    assert_decimal_equals("0.3", result);
    bigdecimal_destroy(result);

    result = evaluate_expression("\xCF\x80 - \xCF\x80 + e - e + \xCF\x86 - \xCF\x86", &context);
    assert_decimal_equals("0", result);
    bigdecimal_destroy(result);

    result = evaluate_expression("2(2 + 2)", &context);
    assert_decimal_equals("8", result);
    bigdecimal_destroy(result);

    result = evaluate_expression("\xCF\x80" "e - \xCF\x80 * e + 10\xCF\x80 - 10 * \xCF\x80 + 5e - 5 * e", &context);
    assert_decimal_equals("0", result);
    bigdecimal_destroy(result);

    result = evaluate_expression("1e-2 - (e - 2)", &context);
    assert_decimal_equals("0", result);
    bigdecimal_destroy(result);

    result = evaluate_expression("1e3 - 3 * e + 1E3 - 1000", &context);
    assert_decimal_equals("0", result);
    bigdecimal_destroy(result);

    result = evaluate_expression("1.5\xC2\xB2 + 2.5\xC2\xB3", &context);
    assert_decimal_equals("17.875", result);
    bigdecimal_destroy(result);

    result = evaluate_expression("-2\xC2\xB2 + (2 + 3)!", &context);
    assert_decimal_equals("116", result);
    bigdecimal_destroy(result);

    result = evaluate_expression("1.5^3 + 2^10", &context);
    assert_decimal_equals("1027.375", result);
    bigdecimal_destroy(result);

    result = evaluate_expression("2^3^2 - 512 - 2^2 + (-2)^2 + 0^0 - 1", &context);
    assert_decimal_equals("0", result);
    bigdecimal_destroy(result);
}

void test_formatter_applies_precision_and_scientific_notation(void)
{
    CalculatorContext context;
    BigDecimal *result;
    char *text = NULL;

    calculator_context_init(&context);
    result = evaluate_expression("1 / 3", &context);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_format_result(result, &context, &text));
    TEST_ASSERT_EQUAL_STRING("0.3333333333", text);
    free(text);
    bigdecimal_destroy(result);

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_context_set_output_scale(&context, 10));
    result = evaluate_expression("1.234567890123E-12", &context);
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_format_result(result, &context, &text));
    TEST_ASSERT_EQUAL_STRING("1.2345678901E-12", text);
    free(text);
    bigdecimal_destroy(result);

    result = evaluate_expression("1234567890123", &context);
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_format_result(result, &context, &text));
    TEST_ASSERT_EQUAL_STRING("1.2345678901E+12", text);
    free(text);
    bigdecimal_destroy(result);

    result = evaluate_expression("1e3 - 3e", &context);
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_format_result(result, &context, &text));
    TEST_ASSERT_EQUAL_STRING("0", text);
    free(text);
    bigdecimal_destroy(result);
}

void test_formatter_respects_rounding_modes_in_scientific_notation(void)
{
    CalculatorContext context;

    calculator_context_init(&context);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_context_set_output_scale(&context, 0));

    context.rounding = BIGDECIMAL_ROUND_HALF_EVEN;
    assert_formatted_expression("2E+10", "2.5E10", &context);
    assert_formatted_expression("4E+10", "3.5E10", &context);
    assert_formatted_expression("-2E+10", "-2.5E10", &context);
    assert_formatted_expression("3E+10", "2.5001E10", &context);
    assert_formatted_expression("1E+11", "9.5E10", &context);

    context.rounding = BIGDECIMAL_ROUND_HALF_UP;
    assert_formatted_expression("3E+10", "2.5E10", &context);
    assert_formatted_expression("-3E+10", "-2.5E10", &context);

    context.rounding = BIGDECIMAL_ROUND_TOWARD_ZERO;
    assert_formatted_expression("2E+10", "2.9E10", &context);
    context.rounding = BIGDECIMAL_ROUND_AWAY_FROM_ZERO;
    assert_formatted_expression("3E+10", "2.1E10", &context);
    context.rounding = BIGDECIMAL_ROUND_FLOOR;
    assert_formatted_expression("-3E+10", "-2.1E10", &context);
    context.rounding = BIGDECIMAL_ROUND_CEILING;
    assert_formatted_expression("3E+10", "2.1E10", &context);
}

void test_evaluator_division_uses_context(void)
{
    CalculatorContext context;
    BigDecimal *result;

    calculator_context_init(&context);
    context.division_scale = 4;
    context.significant_division = false;
    result = evaluate_expression("1 / 3", &context);
    assert_decimal_equals("0.3333", result);
    bigdecimal_destroy(result);

    context.division_scale = 0;
    context.rounding = BIGDECIMAL_ROUND_HALF_EVEN;
    result = evaluate_expression("1 / 2", &context);
    assert_decimal_equals("0", result);
    bigdecimal_destroy(result);

    context.rounding = BIGDECIMAL_ROUND_HALF_UP;
    result = evaluate_expression("1 / 2", &context);
    assert_decimal_equals("1", result);
    bigdecimal_destroy(result);
}

void test_evaluator_reports_arithmetic_errors_without_changing_result(void)
{
    CalculatorContext context;
    CalculatorExpression *expression = parse_expression("1 / 0");
    CalculatorError error;
    BigDecimal *result = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(result, "42"));
    calculator_context_init(&context);
    TEST_ASSERT_EQUAL(CALCULATOR_DIVISION_BY_ZERO,
                      calculator_evaluate(result, expression, &context, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_DIVISION_BY_ZERO, error.status);
    TEST_ASSERT_EQUAL_UINT(2, error.offset);
    assert_decimal_equals("42", result);

    calculator_expression_destroy(expression);
    bigdecimal_destroy(result);
}

void test_evaluator_rejects_invalid_factorial_input(void)
{
    CalculatorContext context;
    CalculatorExpression *expression = parse_expression("1.5!");
    CalculatorError error;
    BigDecimal *result = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(result, "42"));
    calculator_context_init(&context);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,
                      calculator_evaluate(result, expression, &context, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, error.status);
    TEST_ASSERT_EQUAL_UINT(3, error.offset);
    assert_decimal_equals("42", result);

    calculator_expression_destroy(expression);
    bigdecimal_destroy(result);
}

void test_evaluator_rejects_invalid_power_exponent(void)
{
    const char *input[] = { "2^1.5" };

    for (size_t index = 0; index < sizeof(input) / sizeof(input[0]); index++)
    {
        CalculatorContext context;
        CalculatorExpression *expression = parse_expression(input[index]);
        CalculatorError error;
        BigDecimal *result = bigdecimal_create();

        TEST_ASSERT_NOT_NULL(result);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(result, "42"));
        calculator_context_init(&context);
        TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,
                          calculator_evaluate(result, expression, &context, &error));
        TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, error.status);
        TEST_ASSERT_EQUAL_UINT(1, error.offset);
        assert_decimal_equals("42", result);

        calculator_expression_destroy(expression);
        bigdecimal_destroy(result);
    }
}

void test_evaluator_enforces_time_and_factorial_limits(void)
{
    CalculatorContext context;
    CalculatorExpression *expression;
    CalculatorError error;
    BigDecimal *result = bigdecimal_create();

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(result, "42"));
    calculator_context_init(&context);
    context.time_limit_ms = 0;
    expression = parse_expression("1 + 1");
    TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, calculator_evaluate(result, expression, &context, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, error.status);
    assert_decimal_equals("42", result);
    calculator_expression_destroy(expression);

    calculator_context_init(&context);
    expression = parse_expression("10001!");
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, calculator_evaluate(result, expression, &context, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, error.status);
    TEST_ASSERT_EQUAL_UINT(5, error.offset);
    assert_decimal_equals("42", result);
    calculator_expression_destroy(expression);
    bigdecimal_destroy(result);
}

void test_evaluator_rejects_compact_invalid_integer_operands(void)
{
    static const struct
    {
        const char *input;
        CalculatorStatus expected;
    } cases[] = {
        { "1E4294967294!", CALCULATOR_VALUE_TOO_LARGE },
        { "1E9223372036854775807!", CALCULATOR_VALUE_TOO_LARGE },
        { "2E4!", CALCULATOR_VALUE_TOO_LARGE },
        { "1E-9223372036854775807!", CALCULATOR_INVALID_ARGUMENT },
        { "(-1E9223372036854775807)!", CALCULATOR_INVALID_ARGUMENT },
        { "2^1E-9223372036854775807", CALCULATOR_INVALID_ARGUMENT },
        { "2^(-1E9223372036854775807)", CALCULATOR_VALUE_TOO_LARGE }
    };
    CalculatorContext context;

    calculator_context_init(&context);
    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); index++)
    {
        CalculatorExpression *expression = parse_expression(cases[index].input);
        CalculatorError error;
        BigDecimal *result = bigdecimal_create();
        CalculatorStatus status;

        TEST_ASSERT_NOT_NULL(result);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(result, "42"));
        status = calculator_evaluate(result, expression, &context, &error);
        calculator_expression_destroy(expression);
        assert_decimal_equals("42", result);
        bigdecimal_destroy(result);
        TEST_ASSERT_EQUAL(cases[index].expected, status);
        TEST_ASSERT_EQUAL(status, error.status);
    }

    assert_formatted_expression("120", "5E0!", &context);
    assert_formatted_expression("1", "0!", &context);
}

void test_formatter_handles_extreme_scientific_scales_without_expansion(void)
{
    CalculatorContext context;

    calculator_context_init(&context);
    assert_formatted_expression("1E+4294967294", "1E4294967294", &context);
    assert_formatted_expression("1E-4294967293", "1E-4294967293", &context);
    assert_formatted_expression("-1E+9223372036854775807", "-1E9223372036854775807", &context);
    assert_formatted_expression("1E-9223372036854775807", "1E-9223372036854775807", &context);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_context_set_output_scale(&context, 0));
    assert_formatted_expression("1E+11", "9.99E10", &context);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,
                      calculator_context_set_output_scale(&context, CALCULATOR_UNLIMITED_OUTPUT_SCALE));
    assert_formatted_expression("1.25E-4294967293", "1.25E-4294967293", &context);
}

void test_large_factorial_evaluation_and_formatting(void)
{
    CalculatorContext context;

    calculator_context_init(&context);
    /* This is a numeric regression, not a five-second performance test.
     * CTest still bounds the entire process if calculation stops progressing. */
    context.time_limit_ms = INT64_MAX;
    assert_formatted_expression("1.8288019515E+12673", "4000!", &context);
}

/* ============================================================
   Main
   ============================================================ */

void test_significant_division_preserves_tiny_and_huge_values(void)
{
    CalculatorContext context;
    calculator_context_init(&context);
    assert_formatted_expression("1E-40", "1E-40 / 1", &context);
    assert_formatted_expression("-1E-40", "1E-40 / -1", &context);
    assert_formatted_expression("3.3333333333E-41", "1E-40 / 3", &context);
    assert_formatted_expression("3.3333333333E+39", "1E40 / 3", &context);
    assert_formatted_expression("1E-40", "(1E-40 / 3) * 3", &context);
    assert_formatted_expression("1E-9223372036854775807", "1E-9223372036854775807 / 1", &context);
    assert_formatted_expression("1", "1E-9223372036854775807 / 1E-9223372036854775807", &context);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,
        calculator_context_set_output_scale(&context, CALCULATOR_UNLIMITED_OUTPUT_SCALE));
    assert_formatted_expression("1E-40", "1E-40 / 1", &context);
    context.division_scale = 4;
    assert_formatted_expression("0.01235", "1 / 81", &context);
    assert_formatted_expression("12345", "12345 / 1", &context);
    context.rounding = BIGDECIMAL_ROUND_HALF_UP;
    assert_formatted_expression("12345", "12345 / 1", &context);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,
        calculator_context_set_output_scale(&context, CALCULATOR_MAX_OUTPUT_SCALE));
    context.time_limit_ms = INT64_MAX;
    assert_formatted_expression("0.125", "1 / 8", &context);
}

void test_complete_pipeline_limits_and_recovers(void)
{
    CalculatorContext context;
    CalculatorError error;
    char *text = NULL;
    calculator_context_init(&context);
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE,
        calculator_compute("1E100000000 + 1", &context, &text, &error));
    TEST_ASSERT_NULL(text);
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE,
        calculator_compute("2^1E100000000", &context, &text, &error));
    TEST_ASSERT_NULL(text);
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE,
        calculator_context_set_output_scale(&context, CALCULATOR_MAX_OUTPUT_SCALE + 1));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("2+2", &context, &text, &error));
    TEST_ASSERT_EQUAL_STRING("4", text);
    free(text);
    context.time_limit_ms = 0;
    TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, calculator_compute("4000!", &context, &text, &error));
    TEST_ASSERT_NULL(text);
    TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, error.status);
}

void test_significant_division_rounds_both_signs_in_all_modes(void)
{
    static const char *const positive[] = { "0.16", "0.17", "0.16", "0.17", "0.17", "0.17" };
    static const char *const negative[] = { "-0.16", "-0.17", "-0.17", "-0.16", "-0.17", "-0.17" };
    CalculatorContext context;
    calculator_context_init(&context);
    context.output_scale = CALCULATOR_UNLIMITED_OUTPUT_SCALE;
    context.division_scale = 2;
    for (int mode = 0; mode < 6; mode++)
    {
        context.rounding = (BigDecimalRoundingMode)mode;
        assert_formatted_expression(positive[mode], "1/6", &context);
        assert_formatted_expression(negative[mode], "-1/6", &context);
        assert_formatted_expression("0.125", "1/8", &context);
        assert_formatted_expression("-0.125", "-1/8", &context);
    }
}

void test_typed_exact_values_and_approximate_boundary(void)
{
    CalculatorContext context;
    CalculatorError error;
    CalculatorValue value = {0};
    char *text = NULL;

    calculator_context_init(&context);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute_value("1/3", &context, &value, &error));
    TEST_ASSERT_NOT_NULL(value.rational);
    TEST_ASSERT_NULL(value.integer);
    context.significant_division = false;
    TEST_ASSERT_FALSE(calculator_value_matches(&value, &context));
    context.significant_division = true;
    calculator_value_destroy(&value);

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute_value("(1/3)*3-1", &context, &value, &error));
    TEST_ASSERT_NOT_NULL(value.integer);
    TEST_ASSERT_NULL(value.rational);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_format_value(&value, &context, &text));
    TEST_ASSERT_EQUAL_STRING("0", text);
    free(text);
    text = NULL;
    calculator_value_destroy(&value);

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute_value("0,1+0,2", &context, &value, &error));
    TEST_ASSERT_NOT_NULL(value.rational);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_string(value.rational, &text));
    TEST_ASSERT_EQUAL_STRING("3/10", text);
    free(text);
    text = NULL;
    calculator_value_destroy(&value);

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute_value("sqrt(2)+1/3", &context, &value, &error));
    TEST_ASSERT_NULL(value.integer);
    TEST_ASSERT_NULL(value.rational);
    TEST_ASSERT_NOT_NULL(value.number);
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_DECIMAL, value.kind);
    calculator_value_destroy(&value);

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute_value("(2/3)^-2", &context, &value, &error));
    TEST_ASSERT_NOT_NULL(value.rational);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_string(value.rational, &text));
    TEST_ASSERT_EQUAL_STRING("9/4", text);
    free(text);
    text = NULL;
    calculator_value_destroy(&value);

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute_value("sqrt(4/9)+1/3", &context, &value, &error));
    TEST_ASSERT_NOT_NULL(value.integer);
    TEST_ASSERT_NULL(value.rational);
    calculator_value_destroy(&value);

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute_value("sqrt(2)", &context, &value, &error));
    TEST_ASSERT_NULL(value.integer);
    TEST_ASSERT_NULL(value.rational);
    calculator_value_destroy(&value);

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute_value("geomean(2;3)+1/3", &context, &value, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_DECIMAL, value.kind);
    calculator_value_destroy(&value);

    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute_value("stdevp(1;2;3)", &context, &value, &error));
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_DECIMAL, value.kind);
    calculator_value_destroy(&value);

    TEST_ASSERT_EQUAL(CALCULATOR_DIVISION_BY_ZERO,
        calculator_compute_value("0^-1", &context, &value, &error));
    TEST_ASSERT_NULL(value.number);
}

static void assert_exact_calculator_value(const char *expression, const char *expected, bool integer)
{
    CalculatorContext context;
    CalculatorError error;
    CalculatorValue value = {0};
    char *text = NULL;

    calculator_context_init(&context);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute_value(expression, &context, &value, &error));
    if (integer)
    {
        TEST_ASSERT_EQUAL(CALCULATOR_VALUE_INTEGER, value.kind);
        TEST_ASSERT_NOT_NULL(value.integer);
        TEST_ASSERT_NULL(value.rational);
        text = bigint_to_string(value.integer);
    }
    else
    {
        TEST_ASSERT_EQUAL(CALCULATOR_VALUE_RATIONAL, value.kind);
        TEST_ASSERT_NULL(value.integer);
        TEST_ASSERT_NOT_NULL(value.rational);
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_string(value.rational, &text));
    }
    TEST_ASSERT_EQUAL_STRING(expected, text);
    free(text);
    calculator_value_destroy(&value);
}

void test_exact_named_functions_and_powers(void)
{
    assert_exact_calculator_value("(1/3)²", "1/9", false);
    assert_exact_calculator_value("pow(2/3;-2)", "9/4", false);
    assert_exact_calculator_value("sqrt(4/9)", "2/3", false);
    assert_exact_calculator_value("cbrt(-8/27)", "-2/3", false);
    assert_exact_calculator_value("root(16/81;4)", "2/3", false);
    assert_exact_calculator_value("root(-8/27;3)", "-2/3", false);
    assert_exact_calculator_value("abs(-1/3)", "1/3", false);
    assert_exact_calculator_value("sign(-1/3)", "-1", true);
    assert_exact_calculator_value("min(1/3;2/3)", "1/3", false);
    assert_exact_calculator_value("max(1/3;2/3)", "2/3", false);
    assert_exact_calculator_value("min(1;1+1/10^100)", "1", true);
    assert_exact_calculator_value("max(1;1-1/10^100)", "1", true);
    assert_exact_calculator_value("sum(1/3;2/3)", "1", true);
    assert_exact_calculator_value("product(1/3;2/3)", "2/9", false);
    assert_exact_calculator_value("mean(1/3;2/3)", "1/2", false);
    assert_exact_calculator_value("median(1/3;2/3)", "1/2", false);
    assert_exact_calculator_value("geomean(1/4;4)", "1", true);
    assert_exact_calculator_value("geomean(0;4)", "0", true);
    assert_exact_calculator_value("geomean(1/4)", "1/4", false);
    assert_exact_calculator_value("harmean(1;2;4)", "12/7", false);
    assert_exact_calculator_value("variance(1;2;3)", "2/3", false);
    assert_exact_calculator_value("variance(1E50;1E50+1;1E50+2)", "2/3", false);
    assert_exact_calculator_value("stdevp(1;3)", "1", true);
    assert_exact_calculator_value("stdev(1;2;3)", "1", true);
    assert_exact_calculator_value("5!", "120", true);
    assert_exact_calculator_value("factorial(5)", "120", true);
    assert_exact_calculator_value("isqrt(99)", "9", true);
    assert_exact_calculator_value("gcd(-48;18)", "6", true);
    assert_exact_calculator_value("lcm(-4;6)", "12", true);
    assert_exact_calculator_value("mod(-7;3)", "-1", true);
    assert_exact_calculator_value("npr(5;2)", "20", true);
    assert_exact_calculator_value("ncr(5;2)", "10", true);
    assert_exact_calculator_value("floor(1-1/10^100)", "0", true);
    assert_exact_calculator_value("ceil(-1+1/10^100)", "0", true);
    assert_exact_calculator_value("trunc(-1/3)", "0", true);
    assert_exact_calculator_value("round(1/2)", "0", true);
    assert_exact_calculator_value("round(3/2)", "2", true);
    assert_exact_calculator_value("round(-3/2)", "-2", true);
    assert_exact_calculator_value("round(1/3;2)", "33/100", false);
    assert_exact_calculator_value("round(125/100;1)", "6/5", false);
    assert_exact_calculator_value("round(-125/100;1)", "-6/5", false);
    assert_exact_calculator_value("round(250;-2)", "200", true);
    assert_exact_calculator_value("round(350;-2)", "400", true);
    assert_exact_calculator_value("round(-250;-2)", "-200", true);
}

void test_fraction_notation_and_auto_choice(void)
{
    CalculatorContext context;
    CalculatorError error;
    char *text = NULL;

    calculator_context_init(&context);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("1/3", &context, &text, &error));
    TEST_ASSERT_EQUAL_STRING("1/3", text);
    free(text);
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("3/10", &context, &text, &error));
    TEST_ASSERT_EQUAL_STRING("0.3", text);
    free(text);
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("3/8", &context, &text, &error));
    TEST_ASSERT_EQUAL_STRING("3/8", text);
    free(text);
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("1/9999", &context, &text, &error));
    TEST_ASSERT_EQUAL_STRING("1/9999", text);
    free(text);
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("123456789012/7", &context, &text, &error));
    TEST_ASSERT_EQUAL_STRING("123456789012/7", text);
    free(text);
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("100000000000001/3", &context, &text, &error));
    TEST_ASSERT_NULL(strchr(text, '/'));
    free(text);
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("1/10001", &context, &text, &error));
    TEST_ASSERT_NULL(strchr(text, '/'));
    free(text);
    text = NULL;

    context.notation = CALCULATOR_NOTATION_FRACTION;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("1/10001", &context, &text, &error));
    TEST_ASSERT_EQUAL_STRING("1/10001", text);
    free(text);
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("12345678901235/7", &context, &text, &error));
    TEST_ASSERT_EQUAL_STRING("12345678901235/7", text);
    free(text);
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("-1234567890123/7", &context, &text, &error));
    TEST_ASSERT_EQUAL_STRING("-1234567890123/7", text);
    free(text);
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("123456789012345/7", &context, &text, &error));
    TEST_ASSERT_NULL(strchr(text, '/'));
    free(text);
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("1/123456789012345", &context, &text, &error));
    TEST_ASSERT_NULL(strchr(text, '/'));
    free(text);
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("0.1+0.2", &context, &text, &error));
    TEST_ASSERT_EQUAL_STRING("3/10", text);
    free(text);
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("(1/3)*3", &context, &text, &error));
    TEST_ASSERT_EQUAL_STRING("1", text);
    free(text);
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("sqrt(2)", &context, &text, &error));
    TEST_ASSERT_NULL(strchr(text, '/'));
    free(text);
}

void test_fraction_notation_respects_output_limit(void)
{
    CalculatorContext context;
    CalculatorValue value = {0};
    char *digits = malloc(CALCULATOR_MAX_OUTPUT_BYTES + 2U);
    char *text = NULL;

    TEST_ASSERT_NOT_NULL(digits);
    digits[0] = '1';
    memset(digits + 1U, '0', CALCULATOR_MAX_OUTPUT_BYTES);
    digits[CALCULATOR_MAX_OUTPUT_BYTES + 1U] = '\0';
    value.integer = bigint_create();
    TEST_ASSERT_NOT_NULL(value.integer);
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(value.integer, digits));
    value.kind = CALCULATOR_VALUE_INTEGER;
    calculator_context_init(&context);
    context.time_limit_ms = INT64_MAX;
    context.notation = CALCULATOR_NOTATION_FRACTION;
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_TOO_LARGE, calculator_format_value(&value, &context, &text));
    TEST_ASSERT_NULL(text);

    context.notation = CALCULATOR_NOTATION_AUTO;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_format_value(&value, &context, &text));
    TEST_ASSERT_NOT_NULL(text);
    free(text);
    calculator_value_destroy(&value);
    free(digits);
}

static void test_convert_numeric_values_and_diagnostics(void)
{
    /* IDs and numeric text are owned by the AST after the source is released. */
    char source[] = "convert(1;\"m\";\"cm\")";
    CalculatorExpression *expression = NULL;
    CalculatorError parse_error;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_parse(source, &expression, &parse_error));
    memset(source, 'x', sizeof(source) - 1U);
    TEST_ASSERT_EQUAL_STRING("m", expression->data.call.from_unit);
    TEST_ASSERT_EQUAL_STRING("cm", expression->data.call.to_unit);
    calculator_expression_destroy(expression);
    static const struct { const char *input; const char *expected; } cases[] = {
        {"convert(90;\"km/h\";\"m/s\")", "25"},
        {"convert(1/3;\"km\";\"m\")", "1000/3"},
        {"convert(100;\"degC\";\"degF\")", "212"},
        {"convert(-40;\"degC\";\"degF\")", "-40"},
        {"convert(1;\"deltaF\";\"deltaC\")", "5/9"},
        {"convert(1;\"m2\";\"cm2\")", "10000"},
        {"convert(1;\"MiB\";\"B\")", "1048576"},
        {"convert(1;\"MB\";\"B\")", "1000000"},
        {"convert(1;\"B\";\"bit\")", "8"},
        {"convert(1;\"deg\";\"arcmin\")", "60"},
        {"convert(convert(1/3;\"km\";\"m\");\"m\";\"km\")*3", "1"},
        {"sum(convert(1/3;\"m\";\"cm\");2/3)", "34"}
    };
    CalculatorContext context;
    CalculatorError error;
    calculator_context_init(&context);
    context.notation = CALCULATOR_NOTATION_FRACTION;
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++)
    {
        char *text = NULL;
        TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_OK, calculator_compute(cases[i].input, &context, &text, &error), cases[i].input);
        TEST_ASSERT_EQUAL_STRING(cases[i].expected, text);
        free(text);
    }
    static const struct { const char *input; CalculatorStatus status; size_t offset; } errors[] = {
        {"convert(1;\"bad\";\"m\")", CALCULATOR_UNKNOWN_UNIT, 10},
        {"convert(1;\"m\";\"bad\")", CALCULATOR_UNKNOWN_UNIT, 14},
        {"convert(1;\"m\";\"s\")", CALCULATOR_INCOMPATIBLE_UNITS, 14},
        {"convert(1;\"degC\";\"deltaC\")", CALCULATOR_INCOMPATIBLE_UNITS, 17},
        {"convert(1;\"mb\";\"B\")", CALCULATOR_UNKNOWN_UNIT, 10},
        {"convert(1;m;\"cm\")", CALCULATOR_SYNTAX_ERROR, 10},
        {"convert(1;\"m^2\";\"cm2\")", CALCULATOR_INVALID_TOKEN, 12},
        {"convert(1;\"m\\\";\"cm\")", CALCULATOR_INVALID_TOKEN, 12},
        {"convert(1;\"m", CALCULATOR_INVALID_TOKEN, 12},
        {"convert(1;\"\";\"m\")", CALCULATOR_UNKNOWN_UNIT, 10},
        {"convert(1;\"m\";\"cm\";2)", CALCULATOR_SYNTAX_ERROR, 18},
        {"abs(\"m\")", CALCULATOR_SYNTAX_ERROR, 4},
        {"convert(1/0;\"m\";\"cm\")", CALCULATOR_DIVISION_BY_ZERO, 9}
    };
    for (size_t i = 0; i < sizeof(errors)/sizeof(errors[0]); i++)
    {
        char *text = NULL;
        TEST_ASSERT_EQUAL_MESSAGE(errors[i].status, calculator_compute(errors[i].input, &context, &text, &error), errors[i].input);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(errors[i].offset, error.offset, errors[i].input);
        TEST_ASSERT_NULL(text);
    }
    context.notation = CALCULATOR_NOTATION_PLAIN;
    context.output_scale = 10;
    char *text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("convert(180;\"deg\";\"rad\")", &context, &text, &error));
    TEST_ASSERT_EQUAL_STRING("3.1415926536", text);
    free(text); text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("convert(sqrt(2);\"m\";\"cm\")", &context, &text, &error));
    TEST_ASSERT_EQUAL_STRING("141.4213562373", text);
    free(text);
    context.time_limit_ms = 0;
    text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT, calculator_compute("convert(1;\"m\";\"cm\")", &context, &text, &error));
    TEST_ASSERT_NULL(text);
}

static void test_quantity_arithmetic_dimensions_and_temperature(void)
{
    static const struct { const char *input; const char *expected; } cases[] = {
        {"qty(5;\"m\")*qty(5;\"m\")", "25 m²"},
        {"qty(5;\"cm\")*qty(5;\"cm\")", "0.0025 m²"},
        {"qty(1;\"km\")+qty(500;\"m\")", "1.5 km"},
        {"qty(1/3;\"m\")+qty(1/6;\"m\")", "0.5 m"},
        {"qty(3;\"m\")*qty(2;\"m\")*qty(4;\"m\")", "24 m³"},
        {"qty(36;\"km/h\")*qty(10;\"s\")", "100 m"},
        {"qty(1;\"km\")/qty(2;\"h\")", "5/36 m/s"},
        {"qty(1;\"m\")/qty(100;\"cm\")", "1"},
        {"2/qty(4;\"s\")", "0.5 s^-1"},
        {"qty(2;\"kg\")*qty(3;\"m\")/qty(2;\"s\")^2", "1.5 m*kg*s^-2"},
        {"qty(2;\"m\")^0", "1"},
        {"qty(2;\"m\")^-2", "0.25 m^-2"},
        {"qty(3;\"m\")²", "9 m²"},
        {"pow(qty(3;\"m\");2)", "9 m²"},
        {"sqrt(qty(9;\"m2\"))", "3 m"},
        {"cbrt(qty(8;\"m3\"))", "2 m"},
        {"root(qty(16;\"m2\");2)", "4 m"},
        {"abs(qty(-2;\"km\"))", "2 km"},
        {"min(qty(1;\"km\");qty(500;\"m\"))", "0.5 km"},
        {"sum(qty(1;\"km\");qty(500;\"m\"))", "1.5 km"},
        {"round(qty(1.234;\"m\");2)", "1.23 m"},
        {"qty(20;\"degC\")-qty(10;\"degC\")", "10 Δ°C"},
        {"qty(68;\"degF\")-qty(10;\"degC\")", "18 Δ°F"},
        {"qty(20;\"degC\")+qty(18;\"deltaF\")", "30 °C"},
        {"qty(18;\"deltaF\")+qty(20;\"degC\")", "30 °C"},
        {"qty(20;\"degC\")-qty(18;\"deltaF\")", "10 °C"},
        {"qty(1;\"MiB\")+qty(8;\"bit\")", "1.0000009537 MiB"},
        {"qty(1;\"deg\")+qty(60;\"arcmin\")", "2 °"},
        {"qty(1;\"deg\")/qty(3;\"deg\")", "1/3"},
        {"convert(qty(1;\"deg\");\"rad\";\"arcmin\")", "60"},
        {"convert(qty(1;\"km\");\"m\";\"cm\")", "100000"},
        {"ln(qty(1;\"m\")/qty(1;\"m\"))", "0"}
    };
    CalculatorContext context;
    CalculatorError error;
    calculator_context_init(&context);
    for (size_t i=0; i<sizeof(cases)/sizeof(cases[0]); i++)
    {
        char *text=NULL;
        TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_OK, calculator_compute(cases[i].input, &context, &text, &error), cases[i].input);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(cases[i].expected, text, cases[i].input);
        free(text);
    }
    const char *invalid[] = {
        "qty(1;\"m\")+qty(1;\"s\")", "qty(1;\"m\")+0", "qty(1;\"degC\")+qty(2;\"degC\")",
        "qty(1;\"deltaC\")-qty(2;\"degC\")", "qty(1;\"degC\")*2", "-qty(1;\"degC\")",
        "qty(1;\"m\")!", "sqrt(qty(1;\"m\"))", "qty(1;\"m\")^qty(2;\"s\")",
        "qty(1;\"m\")^33", "qty(qty(1;\"m\");\"s\")", "ln(qty(1;\"m\"))",
        "sin(qty(1;\"m\"))", "convert(qty(1;\"m\");\"s\";\"h\")", "sum(qty(1;\"m\");1)"
    };
    for (size_t i=0; i<sizeof(invalid)/sizeof(invalid[0]); i++)
    {
        char *text=NULL;
        TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_DIMENSION_ERROR, calculator_compute(invalid[i], &context, &text, &error), invalid[i]);
        TEST_ASSERT_NULL(text);
    }
    char *text=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_DIVISION_BY_ZERO, calculator_compute("qty(1;\"m\")/0", &context, &text, &error));
    TEST_ASSERT_NULL(text);
    context.notation=CALCULATOR_NOTATION_PLAIN;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute("sin(qty(90;\"deg\"))", &context, &text, &error));
    TEST_ASSERT_EQUAL_STRING("1", text);
    free(text);
    CalculatorValue value = {0}, copy = {0};
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute_value("qty(1/3;\"m\")", &context, &value, &error));
    TEST_ASSERT_TRUE(value.quantity);
    TEST_ASSERT_EQUAL(CALCULATOR_VALUE_RATIONAL, value.kind);
    TEST_ASSERT_EQUAL_INT(1, value.dimensions[0]);
    TEST_ASSERT_EQUAL_STRING("m", value.unit);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_value_copy(&copy, &value));
    calculator_value_destroy(&value);
    TEST_ASSERT_TRUE(copy.quantity);
    TEST_ASSERT_EQUAL_STRING("m", copy.unit);
    calculator_value_destroy(&copy);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_quantity_arithmetic_dimensions_and_temperature);
    RUN_TEST(test_convert_numeric_values_and_diagnostics);

    RUN_TEST(test_context_defaults_and_status_strings);
    RUN_TEST(test_significant_division_preserves_tiny_and_huge_values);
    RUN_TEST(test_complete_pipeline_limits_and_recovers);
    RUN_TEST(test_significant_division_rounds_both_signs_in_all_modes);
    RUN_TEST(test_typed_exact_values_and_approximate_boundary);
    RUN_TEST(test_exact_named_functions_and_powers);
    RUN_TEST(test_fraction_notation_and_auto_choice);
    RUN_TEST(test_fraction_notation_respects_output_limit);
    RUN_TEST(test_context_configures_output_precision);
    RUN_TEST(test_context_configures_angle_units);
    RUN_TEST(test_error_helpers);
    RUN_TEST(test_tokenizer_produces_numbers_operators_and_offsets);
    RUN_TEST(test_tokenizer_keeps_signs_as_operators);
    RUN_TEST(test_tokenizer_accepts_comma_decimal_separator);
    RUN_TEST(test_tokenizer_produces_identifiers);
    RUN_TEST(test_tokenizer_separates_constant_e_from_scientific_notation);
    RUN_TEST(test_tokenizer_produces_postfix_operators);
    RUN_TEST(test_tokenizer_produces_power_operator);
    RUN_TEST(test_tokenizer_rejects_invalid_tokens);
    RUN_TEST(test_parser_accepts_expression_grammar);
    RUN_TEST(test_parser_builds_precedence_and_associativity);
    RUN_TEST(test_parser_rejects_unknown_identifier);
    RUN_TEST(test_parser_reports_syntax_and_lexical_errors);
    RUN_TEST(test_parser_rejects_excessive_recursion_and_ast_depth);
    RUN_TEST(test_evaluator_respects_precedence_and_parentheses);
    RUN_TEST(test_evaluator_division_uses_context);
    RUN_TEST(test_formatter_applies_precision_and_scientific_notation);
    RUN_TEST(test_formatter_respects_rounding_modes_in_scientific_notation);
    RUN_TEST(test_evaluator_reports_arithmetic_errors_without_changing_result);
    RUN_TEST(test_evaluator_rejects_invalid_factorial_input);
    RUN_TEST(test_evaluator_rejects_invalid_power_exponent);
    RUN_TEST(test_evaluator_enforces_time_and_factorial_limits);
    RUN_TEST(test_evaluator_rejects_compact_invalid_integer_operands);
    RUN_TEST(test_formatter_handles_extreme_scientific_scales_without_expansion);
    RUN_TEST(test_large_factorial_evaluation_and_formatting);

    return UNITY_END();
}
