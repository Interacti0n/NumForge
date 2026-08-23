#include "tokenizer.h"

#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Tokenizer implementation. It owns no input memory: every produced token
    refers to a contiguous slice of the caller-owned source string. A NUMBER
    token validates the lexical decimal grammar, while BigDecimal later owns
    numeric conversion and arbitrary-precision storage.
------------------------------------------------------------------------------------------------------------------------------
*/

static bool calculator_is_whitespace(char character)
{
    return character == ' ' || character == '\t' ||
           character == '\n' || character == '\r' ||
           character == '\f' || character == '\v';
}

static bool calculator_is_digit(char character)
{
    return character >= '0' && character <= '9';
}

static void calculator_set_token(
    CalculatorToken *token,
    CalculatorTokenType type,
    const char *text,
    size_t length,
    size_t offset
)
{
    token->type = type;
    token->text = text;
    token->length = length;
    token->offset = offset;
}

static CalculatorStatus calculator_read_number(
    CalculatorTokenizer *tokenizer,
    CalculatorToken *token,
    CalculatorError *error
)
{
    size_t start = tokenizer->offset;
    size_t cursor = start;
    bool digits_before_point = false;
    bool digits_after_point = false;

    while (calculator_is_digit(tokenizer->input[cursor]))
    {
        digits_before_point = true;
        cursor++;
    }

    if (tokenizer->input[cursor] == '.')
    {
        cursor++;
        while (calculator_is_digit(tokenizer->input[cursor]))
        {
            digits_after_point = true;
            cursor++;
        }
    }

    if (!digits_before_point && !digits_after_point)
    {
        calculator_error_set(error, CALCULATOR_INVALID_TOKEN, start);
        return CALCULATOR_INVALID_TOKEN;
    }

    if (tokenizer->input[cursor] == 'e' || tokenizer->input[cursor] == 'E')
    {
        size_t exponent_offset = cursor;

        cursor++;
        if (tokenizer->input[cursor] == '+' || tokenizer->input[cursor] == '-')
        {
            cursor++;
        }

        if (!calculator_is_digit(tokenizer->input[cursor]))
        {
            calculator_error_set(error, CALCULATOR_INVALID_TOKEN, exponent_offset);
            return CALCULATOR_INVALID_TOKEN;
        }

        do
        {
            cursor++;
        } while (calculator_is_digit(tokenizer->input[cursor]));
    }

    calculator_set_token(token, CALCULATOR_TOKEN_NUMBER, tokenizer->input + start,
                         cursor - start, start);
    tokenizer->offset = cursor;
    calculator_error_clear(error);
    return CALCULATOR_OK;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Tokenizer operation functions.
------------------------------------------------------------------------------------------------------------------------------
*/
CalculatorStatus calculator_tokenizer_init(CalculatorTokenizer *tokenizer, const char *input)
{
    if (tokenizer == NULL || input == NULL)
    {
        return CALCULATOR_NULL_ARGUMENT;
    }

    tokenizer->input = input;
    tokenizer->length = strlen(input);
    tokenizer->offset = 0;
    return CALCULATOR_OK;
}

CalculatorStatus calculator_tokenizer_next(
    CalculatorTokenizer *tokenizer,
    CalculatorToken *token,
    CalculatorError *error
)
{
    if (tokenizer == NULL || token == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0);
        return CALCULATOR_NULL_ARGUMENT;
    }

    while (calculator_is_whitespace(tokenizer->input[tokenizer->offset]))
    {
        tokenizer->offset++;
    }

    {
        size_t offset = tokenizer->offset;
        char character = tokenizer->input[offset];

        if (calculator_is_digit(character) ||
            (character == '.' && calculator_is_digit(tokenizer->input[offset + 1U])))
        {
            return calculator_read_number(tokenizer, token, error);
        }

        switch (character)
        {
            case '+':
                calculator_set_token(token, CALCULATOR_TOKEN_PLUS, tokenizer->input + offset, 1, offset);
                break;
            case '-':
                calculator_set_token(token, CALCULATOR_TOKEN_MINUS, tokenizer->input + offset, 1, offset);
                break;
            case '*':
                calculator_set_token(token, CALCULATOR_TOKEN_STAR, tokenizer->input + offset, 1, offset);
                break;
            case '/':
                calculator_set_token(token, CALCULATOR_TOKEN_SLASH, tokenizer->input + offset, 1, offset);
                break;
            case '(':
                calculator_set_token(token, CALCULATOR_TOKEN_LEFT_PAREN, tokenizer->input + offset, 1, offset);
                break;
            case ')':
                calculator_set_token(token, CALCULATOR_TOKEN_RIGHT_PAREN, tokenizer->input + offset, 1, offset);
                break;
            case '\0':
                calculator_set_token(token, CALCULATOR_TOKEN_END, tokenizer->input + offset, 0, offset);
                calculator_error_clear(error);
                return CALCULATOR_OK;
            default:
                calculator_error_set(error, CALCULATOR_INVALID_TOKEN, offset);
                return CALCULATOR_INVALID_TOKEN;
        }

        tokenizer->offset++;
        calculator_error_clear(error);
        return CALCULATOR_OK;
    }
}
