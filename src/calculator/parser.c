#include "expression_internal.h"
#include "tokenizer.h"

#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Parser implementation. The recursive-descent layers directly mirror the
    expression grammar: postfix and power operators bind most tightly, unary
    signs follow, multiplication and division bind next, and addition and
    subtraction bind least tightly.
------------------------------------------------------------------------------------------------------------------------------
*/

typedef struct CalculatorParser
{
    CalculatorTokenizer tokenizer;
    CalculatorToken current;
    CalculatorError *error;
    size_t recursion_depth;
} CalculatorParser;

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal helper functions for parser operations.
------------------------------------------------------------------------------------------------------------------------------
*/
static CalculatorStatus calculator_parser_advance(CalculatorParser *parser)
{
    return calculator_tokenizer_next(&parser->tokenizer, &parser->current, parser->error);
}

static CalculatorExpression *calculator_expression_create(CalculatorExpressionType type, size_t offset)
{
    CalculatorExpression *expression = malloc(sizeof(*expression));

    if (expression != NULL)
    {
        expression->type = type;
        expression->offset = offset;
        expression->depth = 1U;
    }

    return expression;
}

static CalculatorExpression *calculator_expression_create_number(const CalculatorToken *token)
{
    CalculatorExpression *expression = NULL;
    char *text;

    if (token->length == SIZE_MAX)
    {
        return NULL;
    }

    expression = calculator_expression_create(CALCULATOR_EXPRESSION_NUMBER, token->offset);
    text = malloc(token->length + 1U);
    if (expression == NULL || text == NULL)
    {
        free(expression);
        free(text);
        return NULL;
    }

    memcpy(text, token->text, token->length);
    text[token->length] = '\0';
    expression->data.number.text = text;
    return expression;
}

static CalculatorExpression *calculator_expression_create_constant(
    const CalculatorToken *token,
    CalculatorConstant constant
)
{
    CalculatorExpression *expression = calculator_expression_create(
        CALCULATOR_EXPRESSION_CONSTANT, token->offset);

    if (expression != NULL)
    {
        expression->data.constant.constant = constant;
    }

    return expression;
}

static CalculatorExpression *calculator_expression_create_unary(
    CalculatorUnaryOperator operation,
    size_t offset,
    CalculatorExpression *operand
)
{
    CalculatorExpression *expression = calculator_expression_create(CALCULATOR_EXPRESSION_UNARY, offset);

    if (expression != NULL)
    {
        expression->data.unary.operation = operation;
        expression->data.unary.operand = operand;
        expression->depth = operand->depth + 1U;
    }

    return expression;
}

static CalculatorExpression *calculator_expression_create_postfix(
    CalculatorPostfixOperator operation,
    size_t offset,
    CalculatorExpression *operand
)
{
    CalculatorExpression *expression = calculator_expression_create(CALCULATOR_EXPRESSION_POSTFIX, offset);

    if (expression != NULL)
    {
        expression->data.postfix.operation = operation;
        expression->data.postfix.operand = operand;
        expression->depth = operand->depth + 1U;
    }

    return expression;
}

static CalculatorExpression *calculator_expression_create_binary(
    CalculatorBinaryOperator operation,
    size_t offset,
    CalculatorExpression *left,
    CalculatorExpression *right
)
{
    CalculatorExpression *expression = calculator_expression_create(CALCULATOR_EXPRESSION_BINARY, offset);

    if (expression != NULL)
    {
        expression->data.binary.operation = operation;
        expression->data.binary.left = left;
        expression->data.binary.right = right;
        expression->depth = (left->depth > right->depth ? left->depth : right->depth) + 1U;
    }

    return expression;
}

static CalculatorStatus calculator_parser_syntax_error(CalculatorParser *parser)
{
    calculator_error_set(parser->error, CALCULATOR_SYNTAX_ERROR, parser->current.offset);
    return CALCULATOR_SYNTAX_ERROR;
}

static CalculatorStatus calculator_parser_depth_error(CalculatorParser *parser, size_t offset)
{
    calculator_error_set(parser->error, CALCULATOR_VALUE_TOO_LARGE, offset);
    return CALCULATOR_VALUE_TOO_LARGE;
}

static bool calculator_expression_can_wrap(const CalculatorExpression *expression)
{
    return expression->depth < CALCULATOR_MAX_EXPRESSION_DEPTH;
}

static bool calculator_expressions_can_combine(
    const CalculatorExpression *left,
    const CalculatorExpression *right
)
{
    return left->depth < CALCULATOR_MAX_EXPRESSION_DEPTH &&
           right->depth < CALCULATOR_MAX_EXPRESSION_DEPTH;
}

static CalculatorStatus calculator_parse_expression(
    CalculatorParser *parser,
    CalculatorExpression **result
);

static CalculatorStatus calculator_parse_primary(
    CalculatorParser *parser,
    CalculatorExpression **result
)
{
    CalculatorStatus status;
    CalculatorExpression *expression = NULL;

    if (parser->current.type == CALCULATOR_TOKEN_NUMBER)
    {
        expression = calculator_expression_create_number(&parser->current);
        if (expression == NULL)
        {
            calculator_error_set(parser->error, CALCULATOR_OUT_OF_MEMORY, parser->current.offset);
            return CALCULATOR_OUT_OF_MEMORY;
        }

        status = calculator_parser_advance(parser);
        if (status != CALCULATOR_OK)
        {
            calculator_expression_destroy(expression);
            return status;
        }

        *result = expression;
        return CALCULATOR_OK;
    }

    if (parser->current.type == CALCULATOR_TOKEN_IDENTIFIER)
    {
        CalculatorConstant constant;

        if (!calculator_constant_from_text(parser->current.text, parser->current.length, &constant))
        {
            calculator_error_set(parser->error, CALCULATOR_INVALID_TOKEN, parser->current.offset);
            return CALCULATOR_INVALID_TOKEN;
        }

        expression = calculator_expression_create_constant(&parser->current, constant);
        if (expression == NULL)
        {
            calculator_error_set(parser->error, CALCULATOR_OUT_OF_MEMORY, parser->current.offset);
            return CALCULATOR_OUT_OF_MEMORY;
        }

        status = calculator_parser_advance(parser);
        if (status != CALCULATOR_OK)
        {
            calculator_expression_destroy(expression);
            return status;
        }

        *result = expression;
        return CALCULATOR_OK;
    }

    if (parser->current.type == CALCULATOR_TOKEN_LEFT_PAREN)
    {
        size_t offset = parser->current.offset;

        if (parser->recursion_depth >= CALCULATOR_MAX_EXPRESSION_DEPTH)
        {
            return calculator_parser_depth_error(parser, offset);
        }
        status = calculator_parser_advance(parser);
        if (status != CALCULATOR_OK)
        {
            return status;
        }

        parser->recursion_depth++;
        status = calculator_parse_expression(parser, &expression);
        parser->recursion_depth--;
        if (status != CALCULATOR_OK)
        {
            return status;
        }

        if (parser->current.type != CALCULATOR_TOKEN_RIGHT_PAREN)
        {
            calculator_expression_destroy(expression);
            return calculator_parser_syntax_error(parser);
        }

        status = calculator_parser_advance(parser);
        if (status != CALCULATOR_OK)
        {
            calculator_expression_destroy(expression);
            return status;
        }

        *result = expression;
        return CALCULATOR_OK;
    }

    return calculator_parser_syntax_error(parser);
}

static CalculatorStatus calculator_parse_postfix(
    CalculatorParser *parser,
    CalculatorExpression **result
)
{
    CalculatorExpression *left = NULL;
    CalculatorStatus status = calculator_parse_primary(parser, &left);

    if (status != CALCULATOR_OK)
    {
        return status;
    }

    while (parser->current.type == CALCULATOR_TOKEN_SQUARE ||
           parser->current.type == CALCULATOR_TOKEN_CUBE ||
           parser->current.type == CALCULATOR_TOKEN_FACTORIAL)
    {
        CalculatorPostfixOperator operation;
        size_t offset = parser->current.offset;
        CalculatorExpression *combined = NULL;

        switch (parser->current.type)
        {
            case CALCULATOR_TOKEN_SQUARE: operation = CALCULATOR_POSTFIX_SQUARE; break;
            case CALCULATOR_TOKEN_CUBE: operation = CALCULATOR_POSTFIX_CUBE; break;
            case CALCULATOR_TOKEN_FACTORIAL: operation = CALCULATOR_POSTFIX_FACTORIAL; break;
            default:
                calculator_expression_destroy(left);
                calculator_error_set(parser->error, CALCULATOR_INVALID_ARGUMENT, offset);
                return CALCULATOR_INVALID_ARGUMENT;
        }

        status = calculator_parser_advance(parser);
        if (status != CALCULATOR_OK)
        {
            calculator_expression_destroy(left);
            return status;
        }

        if (!calculator_expression_can_wrap(left))
        {
            calculator_expression_destroy(left);
            return calculator_parser_depth_error(parser, offset);
        }

        combined = calculator_expression_create_postfix(operation, offset, left);
        if (combined == NULL)
        {
            calculator_expression_destroy(left);
            calculator_error_set(parser->error, CALCULATOR_OUT_OF_MEMORY, offset);
            return CALCULATOR_OUT_OF_MEMORY;
        }

        left = combined;
    }

    *result = left;
    return CALCULATOR_OK;
}

static CalculatorStatus calculator_parse_unary(
    CalculatorParser *parser,
    CalculatorExpression **result
);

static CalculatorStatus calculator_parse_power(
    CalculatorParser *parser,
    CalculatorExpression **result
)
{
    CalculatorExpression *base = NULL;
    CalculatorExpression *exponent = NULL;
    CalculatorExpression *expression = NULL;
    CalculatorStatus status = calculator_parse_postfix(parser, &base);

    if (status != CALCULATOR_OK)
    {
        return status;
    }
    if (parser->current.type != CALCULATOR_TOKEN_CARET)
    {
        *result = base;
        return CALCULATOR_OK;
    }

    {
        size_t offset = parser->current.offset;

        status = calculator_parser_advance(parser);
        if (status != CALCULATOR_OK)
        {
            calculator_expression_destroy(base);
            return status;
        }

        if (parser->recursion_depth >= CALCULATOR_MAX_EXPRESSION_DEPTH)
        {
            calculator_expression_destroy(base);
            return calculator_parser_depth_error(parser, offset);
        }
        parser->recursion_depth++;
        status = calculator_parse_unary(parser, &exponent);
        parser->recursion_depth--;
        if (status != CALCULATOR_OK)
        {
            calculator_expression_destroy(base);
            return status;
        }

        if (!calculator_expressions_can_combine(base, exponent))
        {
            calculator_expression_destroy(base);
            calculator_expression_destroy(exponent);
            return calculator_parser_depth_error(parser, offset);
        }

        expression = calculator_expression_create_binary(
            CALCULATOR_BINARY_POWER, offset, base, exponent);
        if (expression == NULL)
        {
            calculator_expression_destroy(base);
            calculator_expression_destroy(exponent);
            calculator_error_set(parser->error, CALCULATOR_OUT_OF_MEMORY, offset);
            return CALCULATOR_OUT_OF_MEMORY;
        }

        *result = expression;
        return CALCULATOR_OK;
    }
}

static CalculatorStatus calculator_parse_unary(
    CalculatorParser *parser,
    CalculatorExpression **result
)
{
    CalculatorTokenType type = parser->current.type;

    if (type == CALCULATOR_TOKEN_PLUS || type == CALCULATOR_TOKEN_MINUS)
    {
        CalculatorUnaryOperator operation = type == CALCULATOR_TOKEN_PLUS
            ? CALCULATOR_UNARY_PLUS
            : CALCULATOR_UNARY_MINUS;
        size_t offset = parser->current.offset;
        CalculatorExpression *operand = NULL;
        CalculatorExpression *expression = NULL;
        CalculatorStatus status = calculator_parser_advance(parser);

        if (status != CALCULATOR_OK)
        {
            return status;
        }

        if (parser->recursion_depth >= CALCULATOR_MAX_EXPRESSION_DEPTH)
        {
            return calculator_parser_depth_error(parser, offset);
        }
        parser->recursion_depth++;
        status = calculator_parse_unary(parser, &operand);
        parser->recursion_depth--;
        if (status != CALCULATOR_OK)
        {
            return status;
        }

        if (!calculator_expression_can_wrap(operand))
        {
            calculator_expression_destroy(operand);
            return calculator_parser_depth_error(parser, offset);
        }

        expression = calculator_expression_create_unary(operation, offset, operand);
        if (expression == NULL)
        {
            calculator_expression_destroy(operand);
            calculator_error_set(parser->error, CALCULATOR_OUT_OF_MEMORY, offset);
            return CALCULATOR_OUT_OF_MEMORY;
        }

        *result = expression;
        return CALCULATOR_OK;
    }

    return calculator_parse_power(parser, result);
}

static bool calculator_token_starts_primary(CalculatorTokenType type)
{
    return type == CALCULATOR_TOKEN_NUMBER ||
           type == CALCULATOR_TOKEN_IDENTIFIER ||
           type == CALCULATOR_TOKEN_LEFT_PAREN;
}

static CalculatorStatus calculator_parse_multiplicative(
    CalculatorParser *parser,
    CalculatorExpression **result
)
{
    CalculatorExpression *left = NULL;
    CalculatorStatus status = calculator_parse_unary(parser, &left);

    if (status != CALCULATOR_OK)
    {
        return status;
    }

    while (parser->current.type == CALCULATOR_TOKEN_STAR ||
           parser->current.type == CALCULATOR_TOKEN_SLASH ||
           calculator_token_starts_primary(parser->current.type))
    {
        bool explicit_operator = parser->current.type == CALCULATOR_TOKEN_STAR ||
                                 parser->current.type == CALCULATOR_TOKEN_SLASH;
        CalculatorBinaryOperator operation = parser->current.type == CALCULATOR_TOKEN_SLASH
            ? CALCULATOR_BINARY_DIVIDE
            : CALCULATOR_BINARY_MULTIPLY;
        size_t offset = parser->current.offset;
        CalculatorExpression *right = NULL;
        CalculatorExpression *combined = NULL;

        if (explicit_operator)
        {
            status = calculator_parser_advance(parser);
            if (status != CALCULATOR_OK)
            {
                calculator_expression_destroy(left);
                return status;
            }
        }

        status = calculator_parse_unary(parser, &right);
        if (status != CALCULATOR_OK)
        {
            calculator_expression_destroy(left);
            return status;
        }

        if (!calculator_expressions_can_combine(left, right))
        {
            calculator_expression_destroy(left);
            calculator_expression_destroy(right);
            return calculator_parser_depth_error(parser, offset);
        }

        combined = calculator_expression_create_binary(operation, offset, left, right);
        if (combined == NULL)
        {
            calculator_expression_destroy(left);
            calculator_expression_destroy(right);
            calculator_error_set(parser->error, CALCULATOR_OUT_OF_MEMORY, offset);
            return CALCULATOR_OUT_OF_MEMORY;
        }

        left = combined;
    }

    *result = left;
    return CALCULATOR_OK;
}

static CalculatorStatus calculator_parse_expression(
    CalculatorParser *parser,
    CalculatorExpression **result
)
{
    CalculatorExpression *left = NULL;
    CalculatorStatus status = calculator_parse_multiplicative(parser, &left);

    if (status != CALCULATOR_OK)
    {
        return status;
    }

    while (parser->current.type == CALCULATOR_TOKEN_PLUS ||
           parser->current.type == CALCULATOR_TOKEN_MINUS)
    {
        CalculatorBinaryOperator operation = parser->current.type == CALCULATOR_TOKEN_PLUS
            ? CALCULATOR_BINARY_ADD
            : CALCULATOR_BINARY_SUBTRACT;
        size_t offset = parser->current.offset;
        CalculatorExpression *right = NULL;
        CalculatorExpression *combined = NULL;

        status = calculator_parser_advance(parser);
        if (status != CALCULATOR_OK)
        {
            calculator_expression_destroy(left);
            return status;
        }

        status = calculator_parse_multiplicative(parser, &right);
        if (status != CALCULATOR_OK)
        {
            calculator_expression_destroy(left);
            return status;
        }

        if (!calculator_expressions_can_combine(left, right))
        {
            calculator_expression_destroy(left);
            calculator_expression_destroy(right);
            return calculator_parser_depth_error(parser, offset);
        }

        combined = calculator_expression_create_binary(operation, offset, left, right);
        if (combined == NULL)
        {
            calculator_expression_destroy(left);
            calculator_expression_destroy(right);
            calculator_error_set(parser->error, CALCULATOR_OUT_OF_MEMORY, offset);
            return CALCULATOR_OUT_OF_MEMORY;
        }

        left = combined;
    }

    *result = left;
    return CALCULATOR_OK;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Parser operation functions.
------------------------------------------------------------------------------------------------------------------------------
*/
CalculatorStatus calculator_parse(
    const char *input,
    CalculatorExpression **result,
    CalculatorError *error
)
{
    CalculatorParser parser;
    CalculatorExpression *expression = NULL;
    CalculatorStatus status;

    if (input == NULL || result == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0);
        return CALCULATOR_NULL_ARGUMENT;
    }

    status = calculator_tokenizer_init(&parser.tokenizer, input);
    if (status != CALCULATOR_OK)
    {
        calculator_error_set(error, status, 0);
        return status;
    }

    parser.error = error;
    parser.recursion_depth = 0U;
    status = calculator_parser_advance(&parser);
    if (status != CALCULATOR_OK)
    {
        return status;
    }

    status = calculator_parse_expression(&parser, &expression);
    if (status != CALCULATOR_OK)
    {
        return status;
    }

    if (parser.current.type != CALCULATOR_TOKEN_END)
    {
        calculator_expression_destroy(expression);
        return calculator_parser_syntax_error(&parser);
    }

    *result = expression;
    calculator_error_clear(error);
    return CALCULATOR_OK;
}

void calculator_expression_destroy(CalculatorExpression *expression)
{
    if (expression == NULL)
    {
        return;
    }

    switch (expression->type)
    {
        case CALCULATOR_EXPRESSION_NUMBER:
            free(expression->data.number.text);
            break;
        case CALCULATOR_EXPRESSION_CONSTANT:
            break;
        case CALCULATOR_EXPRESSION_UNARY:
            calculator_expression_destroy(expression->data.unary.operand);
            break;
        case CALCULATOR_EXPRESSION_POSTFIX:
            calculator_expression_destroy(expression->data.postfix.operand);
            break;
        case CALCULATOR_EXPRESSION_BINARY:
            calculator_expression_destroy(expression->data.binary.left);
            calculator_expression_destroy(expression->data.binary.right);
            break;
        default:
            break;
    }

    free(expression);
}
