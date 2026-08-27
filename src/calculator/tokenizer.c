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

static bool calculator_is_identifier_start(char character)
{
    return (character >= 'A' && character <= 'Z') ||
           (character >= 'a' && character <= 'z') ||
           character == '_';
}

static bool calculator_is_identifier_continue(char character)
{
    return calculator_is_identifier_start(character) || calculator_is_digit(character);
}

static bool calculator_is_greek_constant_start(const char *text)
{
    const unsigned char *bytes = (const unsigned char *)text;

    return bytes[0] == 0xCFU && (bytes[1] == 0x80U || bytes[1] == 0x86U);
}

static CalculatorTokenType calculator_superscript_token_type(const char *text)
{
    const unsigned char *bytes = (const unsigned char *)text;

    if (bytes[0] == 0xC2U && bytes[1] == 0xB2U)
    {
        return CALCULATOR_TOKEN_SQUARE;
    }
    if (bytes[0] == 0xC2U && bytes[1] == 0xB3U)
    {
        return CALCULATOR_TOKEN_CUBE;
    }

    return CALCULATOR_TOKEN_END;
}

static bool calculator_is_decimal_separator(char character)
{
    return character == '.' || character == ',';
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

    if (calculator_is_decimal_separator(tokenizer->input[cursor]))
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

    if (tokenizer->input[cursor] == 'E')
    {
        size_t exponent_cursor = cursor + 1U;

        if (tokenizer->input[exponent_cursor] == '+' || tokenizer->input[exponent_cursor] == '-')
        {
            exponent_cursor++;
        }

        if (calculator_is_digit(tokenizer->input[exponent_cursor]))
        {
            cursor = exponent_cursor + 1U;

            while (calculator_is_digit(tokenizer->input[cursor]))
            {
                cursor++;
            }
        }
    }

    calculator_set_token(token, CALCULATOR_TOKEN_NUMBER, tokenizer->input + start,
                         cursor - start, start);
    tokenizer->offset = cursor;
    calculator_error_clear(error);
    return CALCULATOR_OK;
}

static CalculatorStatus calculator_read_identifier(
    CalculatorTokenizer *tokenizer,
    CalculatorToken *token,
    CalculatorError *error
)
{
    size_t start = tokenizer->offset;
    size_t cursor;

    if (calculator_is_greek_constant_start(tokenizer->input + start))
    {
        calculator_set_token(token, CALCULATOR_TOKEN_IDENTIFIER, tokenizer->input + start, 2U, start);
        tokenizer->offset = start + 2U;
        calculator_error_clear(error);
        return CALCULATOR_OK;
    }

    /* Keep Euler's constant separate from an adjacent number: 1e3 is 1 * e * 3. */
    if (tokenizer->input[start] == 'e')
    {
        calculator_set_token(token, CALCULATOR_TOKEN_IDENTIFIER, tokenizer->input + start, 1U, start);
        tokenizer->offset = start + 1U;
        calculator_error_clear(error);
        return CALCULATOR_OK;
    }

    cursor = start + 1U;

    while (calculator_is_identifier_continue(tokenizer->input[cursor]))
    {
        cursor++;
    }

    calculator_set_token(token, CALCULATOR_TOKEN_IDENTIFIER, tokenizer->input + start,
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
            (calculator_is_decimal_separator(character) &&
             calculator_is_digit(tokenizer->input[offset + 1U])))
        {
            return calculator_read_number(tokenizer, token, error);
        }
        CalculatorTokenType superscript_type = calculator_superscript_token_type(tokenizer->input + offset);

        if (superscript_type != CALCULATOR_TOKEN_END)
        {
            calculator_set_token(token, superscript_type, tokenizer->input + offset, 2U, offset);
            tokenizer->offset += 2U;
            calculator_error_clear(error);
            return CALCULATOR_OK;
        }
        if (calculator_is_greek_constant_start(tokenizer->input + offset) ||
            calculator_is_identifier_start(character))
        {
            return calculator_read_identifier(tokenizer, token, error);
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
            case '!':
                calculator_set_token(token, CALCULATOR_TOKEN_FACTORIAL, tokenizer->input + offset, 1, offset);
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
