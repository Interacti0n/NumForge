#ifndef NUMFORGE_CALCULATOR_EXPRESSION_INTERNAL_H
#define NUMFORGE_CALCULATOR_EXPRESSION_INTERNAL_H

#include "parser.h"
#include "constants.h"

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal AST representation shared by parser and evaluator.

    Every node owns its children. Number nodes also own their NUL-terminated
    decimal text, copied from the tokenizer so an AST remains valid after the
    caller releases or replaces the original input string.
------------------------------------------------------------------------------------------------------------------------------
*/
typedef enum CalculatorExpressionType
{
    CALCULATOR_EXPRESSION_NUMBER,
    CALCULATOR_EXPRESSION_CONSTANT,
    CALCULATOR_EXPRESSION_UNARY,
    CALCULATOR_EXPRESSION_POSTFIX,
    CALCULATOR_EXPRESSION_BINARY
} CalculatorExpressionType;

typedef enum CalculatorUnaryOperator
{
    CALCULATOR_UNARY_PLUS,
    CALCULATOR_UNARY_MINUS
} CalculatorUnaryOperator;

typedef enum CalculatorPostfixOperator
{
    CALCULATOR_POSTFIX_SQUARE,
    CALCULATOR_POSTFIX_CUBE,
    CALCULATOR_POSTFIX_FACTORIAL
} CalculatorPostfixOperator;

typedef enum CalculatorBinaryOperator
{
    CALCULATOR_BINARY_ADD,
    CALCULATOR_BINARY_SUBTRACT,
    CALCULATOR_BINARY_MULTIPLY,
    CALCULATOR_BINARY_DIVIDE,
    CALCULATOR_BINARY_POWER
} CalculatorBinaryOperator;

struct CalculatorExpression
{
    CalculatorExpressionType type;
    size_t offset;

    union
    {
        struct
        {
            char *text;
        } number;

        struct
        {
            CalculatorConstant constant;
        } constant;

        struct
        {
            CalculatorUnaryOperator operation;
            struct CalculatorExpression *operand;
        } unary;

        struct
        {
            CalculatorPostfixOperator operation;
            struct CalculatorExpression *operand;
        } postfix;

        struct
        {
            CalculatorBinaryOperator operation;
            struct CalculatorExpression *left;
            struct CalculatorExpression *right;
        } binary;
    } data;
};

#endif
