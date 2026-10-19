#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

/* Test-only line protocol for the independent JavaScript BigInt oracle. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <numforge/bigint.h>
#include <numforge/bigdecimal.h>
#include "src/core/bigdecimal/bigdecimal_internal.h"
#include "src/core/bigint/bigint_internal.h"
#include "src/engine/evaluator/evaluator.h"
#include "src/runtime/numforge_alloc.h"

int main(void)
{
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    if (!numforge_alloc_stats_track(true)) return 3;
#endif
    BigDecimalWorkspace workspace = {0};
    char op[16], a[20000], b[20000];
    int scale, rounding;
    BigInt *ia = bigint_create(), *ib = bigint_create(), *ir = bigint_create(), *im = bigint_create();
    BigDecimal *da = bigdecimal_create(), *db = bigdecimal_create(), *dr = bigdecimal_create();
    if (!ia || !ib || !ir || !im || !da || !db || !dr)
    {
        bigdecimal_workspace_clear(&workspace);
        bigint_destroy(ia); bigint_destroy(ib); bigint_destroy(ir); bigint_destroy(im);
        bigdecimal_destroy(da); bigdecimal_destroy(db); bigdecimal_destroy(dr);
        return 2;
    }
    while (scanf("%15s %19999s %19999s %d %d", op, a, b, &scale, &rounding) == 5)
    {
        int status;
        char *text = NULL;
        if (!strcmp(op, "ibase") || !strcmp(op, "iparse"))
        {
            status = !strcmp(op, "iparse") ? bigint_set_string_base(ia, a, (unsigned)scale) : bigint_set_string(ia, a);
            if (!status)
            {
                if (!strcmp(op, "ibase")) status = bigint_to_string_base(ia, (unsigned)scale, rounding != 0, &text);
                else text = bigint_to_string(ia);
            }
        }
        else if (!strcmp(op, "idigits"))
        {
            size_t count = 0U;
            status = bigint_set_string(ia, a);
            if (!status) status = bigint_decimal_digits(ia, &count);
            if (!status)
            {
                text = malloc(32U);
                if (text) (void)snprintf(text, 32U, "%zu", count);
                else status = BIGINT_OUT_OF_MEMORY;
            }
        }
        else if (!strcmp(op, "calc"))
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
                if (!strcmp(op, "idivpair"))
                {
                    BigInt *q = scale == 1 || scale == 5 ? ia : scale == 2 || scale == 6 ? ib : ir;
                    BigInt *r = scale == 3 || scale == 6 ? ia : scale == 4 || scale == 5 ? ib : im;
                    status = scale < 0 || scale > 6 ? BIGINT_INVALID_ARGUMENT : bigint_div_mod(q, r, ia, ib);
                    if (!status)
                    {
                        char *qt = bigint_to_string(q), *rt = bigint_to_string(r);
                        if (qt && rt)
                        {
                            size_t size = strlen(qt) + strlen(rt) + 2U;
                            text = malloc(size);
                            if (text) (void)snprintf(text, size, "%s;%s", qt, rt);
                        }
                        free(qt); free(rt);
                        if (!text) status = BIGINT_OUT_OF_MEMORY;
                    }
                }
                else if (!strcmp(op, "iadd")) status = bigint_add(ir, ia, ib);
                else if (!strcmp(op, "isub")) status = bigint_sub(ir, ia, ib);
                else if (!strcmp(op, "imul")) status = bigint_mul(ir, ia, ib);
                else if (!strcmp(op, "idiv")) status = bigint_div(ir, ia, ib);
                else if (!strcmp(op, "imod")) status = bigint_mod(ir, ia, ib);
                else if (!strcmp(op, "igcd")) status = bigint_gcd(ir, ia, ib);
                else if (!strcmp(op, "ipow")) status = bigint_pow(ir, ia, ib);
                else status = 99;
            }
            if (!status && strcmp(op, "idivpair")) text = bigint_to_string(ir);
        }
        else
        {
            status = bigdecimal_set_string(da, a);
            if (!status) status = bigdecimal_set_string(db, b);
            if (!status)
            {
                if (!strcmp(op, "dfmt")) status = bigdecimal_format(da, scale, (BigDecimalRoundingMode)rounding, &text);
                else if (!strcmp(op, "dadd")) status = bigdecimal_add(dr, da, db);
                else if (!strcmp(op, "dsub")) status = bigdecimal_sub(dr, da, db);
                else if (!strcmp(op, "dmul")) status = bigdecimal_mul(dr, da, db);
                else if (!strcmp(op, "ddiv")) status = bigdecimal_div(dr, da, db, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "dsig")) status = bigdecimal_div_significant(dr, da, db, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "droundsig")) status = bigdecimal_round_significant(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "dwdiv")) status = bigdecimal_div_significant_workspace(dr, da, db, scale, (BigDecimalRoundingMode)rounding, &workspace);
                else if (!strcmp(op, "dwround")) status = bigdecimal_round_significant_workspace(dr, da, scale, (BigDecimalRoundingMode)rounding, &workspace);
                else if (!strcmp(op, "dwadd")) status = bigdecimal_add_significant_workspace(dr, da, db, scale, (BigDecimalRoundingMode)rounding, &workspace);
                else if (!strcmp(op, "dwmul")) status = bigdecimal_mul_significant_workspace(dr, da, db, scale, (BigDecimalRoundingMode)rounding, &workspace);
                else if (!strcmp(op, "dcalc")) status = bigdecimal_div_exact_or_significant(dr, da, db, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "dscale")) status = bigdecimal_rescale(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "rroot")) status = bigdecimal_root(dr, da, (uint32_t)strtoul(b, NULL, 10), scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "exp")) status = bigdecimal_exp(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "ln")) status = bigdecimal_ln(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "log10")) status = bigdecimal_log10(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "log")) status = bigdecimal_log(dr, da, db, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "sin")) status = bigdecimal_sin(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "cos")) status = bigdecimal_cos(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "tan")) status = bigdecimal_tan(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "asin")) status = bigdecimal_asin(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "acos")) status = bigdecimal_acos(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "atan")) status = bigdecimal_atan(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "sinh")) status = bigdecimal_sinh(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "cosh")) status = bigdecimal_cosh(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "tanh")) status = bigdecimal_tanh(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "asinh")) status = bigdecimal_asinh(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "acosh")) status = bigdecimal_acosh(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else if (!strcmp(op, "atanh")) status = bigdecimal_atanh(dr, da, scale, (BigDecimalRoundingMode)rounding);
                else status = 99;
            }
            if (!status && strcmp(op, "dfmt")) status = bigdecimal_to_string(dr, &text);
        }
        if (status || !text) printf("ERROR %d\n", status);
        else puts(text);
        free(text);
    }
    bigdecimal_workspace_clear(&workspace);
    bigint_destroy(ia); bigint_destroy(ib); bigint_destroy(ir); bigint_destroy(im);
    bigdecimal_destroy(da); bigdecimal_destroy(db); bigdecimal_destroy(dr);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    if (!numforge_alloc_stats_complete() || numforge_alloc_stats_live() != 0U)
    {
        fprintf(stderr, "Numeric oracle retained tracked allocations after cleanup\n");
        return 3;
    }
    if (!numforge_alloc_stats_track(false)) return 3;
#endif
    return 0;
}
