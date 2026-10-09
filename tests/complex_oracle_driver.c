#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <numforge/bigcomplex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../benchmarks/benchmark_clock.h"
#include "../src/internal/numforge_alloc.h"

/* Output conversion is outside the timed public C API call. */
int main(int argc,char **argv) {
    bool memory=argc==2 && !strcmp(argv[1],"--memory");
    if(argc>1 && !memory) return 2;
#ifndef NUMFORGE_ENABLE_ALLOC_STATS
    if(memory) {fprintf(stderr,"Memory mode requires NUMFORGE_BUILD_BENCHMARKS\n");return 2;}
#endif
    char op[16], re[1024], im[1024], bre[1024], bim[1024]; int digits, rounding;
    BigComplex *z = NULL, *out = NULL, *b = NULL;
    BigDecimal *r = NULL, *i = NULL;
    int exit_code = 0, fields;
    while ((fields = scanf("%15s %1023s %1023s %d %d %1023s %1023s", op, re, im, &digits, &rounding, bre, bim)) == 7) {
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
        if(memory && !numforge_alloc_stats_track(true)) {exit_code=2;break;}
#endif
        if(!z) {
            z=bigcomplex_create();out=bigcomplex_create();b=bigcomplex_create();
            r=bigdecimal_create();i=bigdecimal_create();
        }
        if (!z || !out || !b || !r || !i) {exit_code=2;break;}
        BigComplexStatus status = bigcomplex_set_strings(z, re, im);
        double start, elapsed; char *rt = NULL, *it = NULL;
        if (status) { exit_code = 2; break; }
        status = bigcomplex_set_strings(b,bre,bim);
        if (status) { exit_code = 2; break; }
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
        size_t baseline=0,calls=0,bytes=0,live=0,peak=0;
        if(memory) {baseline=numforge_alloc_stats_live();numforge_alloc_stats_reset();}
#endif
        start = benchmark_seconds();
#define OP(name) if (!strcmp(op, #name)) status = bigcomplex_##name(out,z,digits,(BigDecimalRoundingMode)rounding); else
        OP(sqrt) OP(exp) OP(ln) OP(sin) OP(cos) OP(tan)
        OP(sinh) OP(cosh) OP(tanh) OP(asin) OP(acos) OP(atan)
        OP(asinh) OP(acosh) OP(atanh)
        if (!strcmp(op,"abs") || !strcmp(op,"arg")) {
            status=!strcmp(op,"abs") ? bigcomplex_abs(r,z,digits,(BigDecimalRoundingMode)rounding) :
                bigcomplex_arg(r,z,digits,(BigDecimalRoundingMode)rounding);
        }
        else
        if (!strcmp(op,"div")) status=bigcomplex_div(out,z,b,digits,(BigDecimalRoundingMode)rounding);
        else if (!strcmp(op,"log")) status=bigcomplex_log(out,z,b,digits,(BigDecimalRoundingMode)rounding);
        else if (!strcmp(op,"pow")) status=bigcomplex_pow(out,z,b,digits,(BigDecimalRoundingMode)rounding);
        else
        { exit_code = 2; break; }
#undef OP
        elapsed = benchmark_seconds() - start;
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
        if(memory) {
            calls=numforge_alloc_stats_calls();bytes=numforge_alloc_stats_bytes();
            live=numforge_alloc_stats_live();peak=numforge_alloc_stats_peak();
        }
#endif
        if(!status && (!strcmp(op,"abs") || !strcmp(op,"arg"))) status=bigcomplex_from_bigdecimal(out,r);
        if (status || start < 0 || elapsed < 0 ||
            bigcomplex_get_real(r,out) || bigcomplex_get_imaginary(i,out) ||
            bigdecimal_format_mode(r,-1,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_SCIENTIFIC,65536,&rt) ||
            bigdecimal_format_mode(i,-1,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_SCIENTIFIC,65536,&it)) {
            fprintf(stderr,"Failed %s(%s,%s): %d\n",op,re,im,(int)status);
            free(rt); free(it); exit_code = 1; break;
        }
        printf("%s\t%s\t%.9f",rt,it,elapsed);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
        if(memory) printf("\t%zu\t%zu\t%zu\t%zu\t%zu",calls,bytes,baseline,live,peak);
#endif
        printf("\n");
        free(rt); free(it);
        if(memory) {
            bigcomplex_destroy(z);bigcomplex_destroy(out);bigcomplex_destroy(b);
            bigdecimal_destroy(r);bigdecimal_destroy(i);z=NULL;out=NULL;b=NULL;r=NULL;i=NULL;
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
            if(!numforge_alloc_stats_complete() || numforge_alloc_stats_live()!=0 ||
                !numforge_alloc_stats_track(false)) {exit_code=1;break;}
#endif
        }
    }
    if ((fields != EOF || ferror(stdin)) && !exit_code) exit_code = 2;
    bigcomplex_destroy(z); bigcomplex_destroy(out); bigcomplex_destroy(b);
    bigdecimal_destroy(r); bigdecimal_destroy(i);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    if(memory && (!numforge_alloc_stats_complete() || numforge_alloc_stats_live()!=0 ||
        !numforge_alloc_stats_track(false))) exit_code=1;
#endif
    return exit_code;
}
