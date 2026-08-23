#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <numforge/bigdecimal.h>

#include "calculator_internal.h"
#include "evaluator.h"
#include "parser.h"

#define CALCULATOR_INPUT_CAPACITY 4096U

/*
------------------------------------------------------------------------------------------------------------------------------
    Interactive command-line calculator. Parsing and evaluation remain in the
    calculator modules; this file only owns terminal input, output, and simple
    user-facing diagnostics.
------------------------------------------------------------------------------------------------------------------------------
*/

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal helper functions for command-line calculator operations.
------------------------------------------------------------------------------------------------------------------------------
*/
static void calculator_discard_remaining_line(void)
{
    int character;

    do
    {
        character = getchar();
    } while (character != '\n' && character != EOF);
}

static void calculator_print_error(const char *input, CalculatorError error)
{
    size_t index;

    fprintf(stderr, "error at column %zu: %s\n", error.offset + 1U,
            calculator_status_to_string(error.status));
    fprintf(stderr, "  %s\n  ", input);

    for (index = 0; index < error.offset && input[index] != '\0'; index++)
    {
        fputc(input[index] == '\t' ? '\t' : ' ', stderr);
    }

    fputs("^\n", stderr);
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Main command-line calculator operation.
------------------------------------------------------------------------------------------------------------------------------
*/
int main(void)
{
    CalculatorContext context;
    char input[CALCULATOR_INPUT_CAPACITY];

    calculator_context_init(&context);
    puts("NumForge calculator");
    puts("Enter an expression using +, -, *, /, and parentheses. Type exit to quit.");

    for (;;)
    {
        size_t length;
        CalculatorExpression *expression = NULL;
        CalculatorError error;
        CalculatorStatus status;
        BigDecimal *result;
        char *text;

        fputs("> ", stdout);
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            if (ferror(stdin))
            {
                fputs("failed to read input\n", stderr);
                return 1;
            }

            break;
        }

        length = strcspn(input, "\r\n");
        if (input[length] == '\0' && !feof(stdin))
        {
            calculator_discard_remaining_line();
            fprintf(stderr, "input is too long (maximum %u characters)\n",
                    CALCULATOR_INPUT_CAPACITY - 1U);
            continue;
        }
        input[length] = '\0';

        if (input[0] == '\0')
        {
            continue;
        }
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0)
        {
            break;
        }

        status = calculator_parse(input, &expression, &error);
        if (status != CALCULATOR_OK)
        {
            calculator_print_error(input, error);
            continue;
        }

        result = bigdecimal_create();
        if (result == NULL)
        {
            calculator_expression_destroy(expression);
            fputs("error: out of memory\n", stderr);
            continue;
        }

        status = calculator_evaluate(result, expression, &context, &error);
        calculator_expression_destroy(expression);
        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(result);
            calculator_print_error(input, error);
            continue;
        }

        text = NULL;
        if (bigdecimal_to_string(result, &text) != BIGDECIMAL_OK)
        {
            bigdecimal_destroy(result);
            fputs("error: failed to format result\n", stderr);
            continue;
        }

        printf("= %s\n", text);
        free(text);
        bigdecimal_destroy(result);
    }

    return 0;
}
