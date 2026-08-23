#ifndef NUMFORGE_CALCULATOR_TOKENIZER_H
#define NUMFORGE_CALCULATOR_TOKENIZER_H

#include "calculator_internal.h"

/*
------------------------------------------------------------------------------------------------------------------------------
    Token types produced from calculator source text.

    The tokenizer deliberately leaves unary plus/minus to the parser. This
    makes -2^2 and 2*-3 unambiguous once exponentiation is introduced.
------------------------------------------------------------------------------------------------------------------------------
*/
typedef enum CalculatorTokenType
{
    CALCULATOR_TOKEN_NUMBER,
    CALCULATOR_TOKEN_PLUS,
    CALCULATOR_TOKEN_MINUS,
    CALCULATOR_TOKEN_STAR,
    CALCULATOR_TOKEN_SLASH,
    CALCULATOR_TOKEN_LEFT_PAREN,
    CALCULATOR_TOKEN_RIGHT_PAREN,
    CALCULATOR_TOKEN_END
} CalculatorTokenType;

/*
------------------------------------------------------------------------------------------------------------------------------
    text points into the original input; it is never separately allocated.
------------------------------------------------------------------------------------------------------------------------------
*/
typedef struct CalculatorToken
{
    CalculatorTokenType type;
    const char *text;
    size_t length;
    size_t offset;
} CalculatorToken;

typedef struct CalculatorTokenizer
{
    const char *input;
    size_t length;
    size_t offset;
} CalculatorTokenizer;

/*
------------------------------------------------------------------------------------------------------------------------------
    Tokenizer operation functions.
------------------------------------------------------------------------------------------------------------------------------
*/
CalculatorStatus calculator_tokenizer_init( /*Initialize a tokenizer over a NUL-terminated input string*/
    CalculatorTokenizer *tokenizer,
    const char *input
);
CalculatorStatus calculator_tokenizer_next(
    CalculatorTokenizer *tokenizer,
    CalculatorToken *token,
    CalculatorError *error
);

#endif
