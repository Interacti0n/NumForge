#include "tokenizer.h"

#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Tokenizer implementation. It owns no input memory: every produced token
    will refer to a contiguous slice of the caller-owned source string.
------------------------------------------------------------------------------------------------------------------------------
*/

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

    calculator_error_set(error, CALCULATOR_NOT_IMPLEMENTED, tokenizer->offset);
    return CALCULATOR_NOT_IMPLEMENTED;
}
