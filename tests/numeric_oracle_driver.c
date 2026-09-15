#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

/* Test-only line protocol for the independent JavaScript BigInt oracle. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <numforge/bigint.h>
#include <numforge/bigdecimal.h>
#include "bigdecimal_internal.h"
#include "../src/calculator/evaluator.h"
#include <numforge/bigdecimal.h>

int main(void)
{
    char op[16], a[2048], b[2048];
    int scale, rounding;
    BigInt *ia = bigint_create(), *ib = bigint_create(), *ir = bigint_create();
    BigDecimal *da = bigdecimal_create(), *db = bigdecimal_create(), *dr = bigdecimal_create();
    if (!ia || !ib || !ir || !da || !db || !dr)
    {
        bigint_destroy(ia); bigint_destroy(ib); bigint_destroy(ir);
        bigdecimal_destroy(da); bigdecimal_destroy(db); bigdecimal_destroy(dr);
        return 2;
    }
    while (scanf("%15s %2047s %2047s %d %d", op, a, b, &scale, &rounding) == 5)
    {
        int status;
        char *text = NULL;
        if (op[0] == 'c')
        {
            CalculatorExpression *expression = NULL;
            CalculatorContext context;
            CalculatorError error;
            calculator_context_init(&context);
            status = calculator_parse(a, &expression, &error);
            if (!status) status = calculator_evaluate(dr, expression, &context, &error);
            if (!status) status = bigdecimal_to_string(dr, &text);
            calculator_expression_destroy(expression);
        }
        else if (op[0] == 'i')
        {
            status = bigint_set_string(ia, a);
            if (!status) status = bigint_set_string(ib, b);
            if (!status)
            {
                if (!strcmp(op, "iadd")) status = bigint_add(ir, ia, ib);
                else if (!strcmp(op, "isub")) status = bigint_sub(ir, ia, ib);
                else if (!strcmp(op, "imul")) status = bigint_mul(ir, ia, ib);
                else if (!strcmp(op, "idiv")) status = bigint_div(ir, ia, ib);
                else if (!strcmp(op, "imod")) status = bigint_mod(ir, ia, ib);
                else if (!strcmp(op, "igcd")) status = bigint_gcd(ir, ia, ib);
                else if (!strcmp(op, "ipow")) status = bigint_pow(ir, ia, ib);
                else status = 99;
            }
            if (!status) text = bigint_to_string(ir);
        }
        else
        {
            status = bigdecimal_set_string(da, a);
            if (!status) status = bigdecimal_set_string(db, b);
            if (!status)
            {
                if (!strcmp(op, "dadd")) status = bigdecimal_add(dr, da, db);
                else if (!strcmp(op, "dsub")) status = bigdecimal_sub(dr, da, db);
                else if (!strcmp(op, "dmul")) status = bigdecimal_mul(dr, da, db);
                else if (!strcmp(op, "ddiv")) status = bigdecimal_div(dr, da, db, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "dsig")) status = bigdecimal_div_significant(dr, da, db, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "dcalc")) status = bigdecimal_div_exact_or_significant(dr, da, db, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "dscale")) status = bigdecimal_rescale(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "rroot")) status = bigdecimal_root(dr, da, (uint32_t)strtoul(b, NULL, 10), scale, (BigDecimalRoundingMode)rounding);
                else status = 99;
            }
            if (!status) status = bigdecimal_to_string(dr, &text);
        }
        if (status || !text) printf("ERROR %d\n", status);
        else puts(text);
        free(text);
    }
    bigint_destroy(ia); bigint_destroy(ib); bigint_destroy(ir);
    bigdecimal_destroy(da); bigdecimal_destroy(db); bigdecimal_destroy(dr);
    return 0;
}
