#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <numforge/bigcomplex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../benchmarks/benchmark_clock.h"

/* Output conversion is outside the timed public C API call. */
int main(void) {
    char op[16], re[1024], im[1024], bre[1024], bim[1024]; int digits, rounding;
    BigComplex *z = bigcomplex_create(), *out = bigcomplex_create(), *b = bigcomplex_create();
    BigDecimal *r = bigdecimal_create(), *i = bigdecimal_create();
    int exit_code = 0, fields;
    if (!z || !out || !b || !r || !i) { exit_code = 2; goto cleanup; }
    while ((fields = scanf("%15s %1023s %1023s %d %d %1023s %1023s", op, re, im, &digits, &rounding, bre, bim)) == 7) {
        BigComplexStatus status = bigcomplex_set_strings(z, re, im);
        double start, elapsed; char *rt = NULL, *it = NULL;
        if (status) { exit_code = 2; break; }
        status = bigcomplex_set_strings(b,bre,bim);
        if (status) { exit_code = 2; break; }
        start = benchmark_seconds();
#define OP(name) if (!strcmp(op, #name)) status = bigcomplex_##name(out,z,digits,(BigDecimalRoundingMode)rounding); else
        OP(sqrt) OP(exp) OP(ln) OP(sin) OP(cos) OP(tan)
        OP(sinh) OP(cosh) OP(tanh) OP(asin) OP(acos) OP(atan)
        OP(asinh) OP(acosh) OP(atanh)
        if (!strcmp(op,"div")) status=bigcomplex_div(out,z,b,digits,(BigDecimalRoundingMode)rounding);
        else if (!strcmp(op,"log")) status=bigcomplex_log(out,z,b,digits,(BigDecimalRoundingMode)rounding);
        else if (!strcmp(op,"pow")) status=bigcomplex_pow(out,z,b,digits,(BigDecimalRoundingMode)rounding);
        else
        { exit_code = 2; break; }
#undef OP
        elapsed = benchmark_seconds() - start;
        if (status || start < 0 || elapsed < 0 ||
            bigcomplex_get_real(r,out) || bigcomplex_get_imaginary(i,out) ||
            bigdecimal_to_string(r,&rt) || bigdecimal_to_string(i,&it)) {
            fprintf(stderr,"Failed %s(%s,%s): %d\n",op,re,im,(int)status);
            free(rt); free(it); exit_code = 1; break;
        }
        printf("%s\t%s\t%.9f\n",rt,it,elapsed);
        free(rt); free(it);
    }
    if ((fields != EOF || ferror(stdin)) && !exit_code) exit_code = 2;
cleanup:
    bigcomplex_destroy(z); bigcomplex_destroy(out); bigcomplex_destroy(b);
    bigdecimal_destroy(r); bigdecimal_destroy(i);
    return exit_code;
}
