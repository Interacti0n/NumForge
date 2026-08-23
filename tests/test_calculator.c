#include <stdlib.h>

#include <unity.h>

#include "calculator_internal.h"
#include "evaluator.h"
#include "expression_internal.h"
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

/* ============================================================
   Shared calculator utilities
   ============================================================ */

void test_context_defaults_and_status_strings(void)
{
    CalculatorContext context;

    calculator_context_init(&context);

    TEST_ASSERT_EQUAL_INT64(34, context.division_scale);
    TEST_ASSERT_EQUAL(BIGDECIMAL_ROUND_HALF_EVEN, context.rounding);
    TEST_ASSERT_EQUAL_STRING("syntax error", calculator_status_to_string(CALCULATOR_SYNTAX_ERROR));
    TEST_ASSERT_EQUAL_STRING("unknown status", calculator_status_to_string((CalculatorStatus)999));
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
}

/* ============================================================
   Tokenizer
   ============================================================ */

void test_tokenizer_produces_numbers_operators_and_offsets(void)
{
    CalculatorTokenizer tokenizer;

    TEST_ASSERT_EQUAL(CALCULATOR_OK,
                      calculator_tokenizer_init(&tokenizer, " \t12.5e-2 + (.3 * 1.) / 4\r\n"));
    assert_next_token(&tokenizer, CALCULATOR_TOKEN_NUMBER, "12.5e-2", 7, 2);
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

void test_tokenizer_rejects_invalid_tokens(void)
{
    const char *input[] = { ".", "@", "1e", "1E+", "1e-" };
    const size_t expected_offset[] = { 0, 0, 1, 1, 1 };

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
    const char *input[] = { "1", "-1", "+.5", "1 + 2 * 3", "(1 + 2) / 3", "1e-2 / .5" };

    for (size_t index = 0; index < sizeof(input) / sizeof(input[0]); index++)
    {
        CalculatorExpression *expression = parse_expression(input[index]);
        calculator_expression_destroy(expression);
    }
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
}

void test_parser_reports_syntax_and_lexical_errors(void)
{
    const char *input[] = { "", "()", "(1", "1 +", "1 2", "1)", "+)", "1 @ 2" };
    const CalculatorStatus expected_status[] = {
        CALCULATOR_SYNTAX_ERROR,
        CALCULATOR_SYNTAX_ERROR,
        CALCULATOR_SYNTAX_ERROR,
        CALCULATOR_SYNTAX_ERROR,
        CALCULATOR_SYNTAX_ERROR,
        CALCULATOR_SYNTAX_ERROR,
        CALCULATOR_SYNTAX_ERROR,
        CALCULATOR_INVALID_TOKEN
    };
    const size_t expected_offset[] = { 0, 1, 2, 3, 2, 1, 1, 2 };

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
}

void test_evaluator_division_uses_context(void)
{
    CalculatorContext context;
    BigDecimal *result;

    calculator_context_init(&context);
    context.division_scale = 4;
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

/* ============================================================
   Main
   ============================================================ */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_context_defaults_and_status_strings);
    RUN_TEST(test_error_helpers);
    RUN_TEST(test_tokenizer_produces_numbers_operators_and_offsets);
    RUN_TEST(test_tokenizer_keeps_signs_as_operators);
    RUN_TEST(test_tokenizer_rejects_invalid_tokens);
    RUN_TEST(test_parser_accepts_expression_grammar);
    RUN_TEST(test_parser_builds_precedence_and_associativity);
    RUN_TEST(test_parser_reports_syntax_and_lexical_errors);
    RUN_TEST(test_evaluator_respects_precedence_and_parentheses);
    RUN_TEST(test_evaluator_division_uses_context);
    RUN_TEST(test_evaluator_reports_arithmetic_errors_without_changing_result);

    return UNITY_END();
}
