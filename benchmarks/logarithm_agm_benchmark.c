#include "benchmark_clock.h"
#include "../src/bigdecimal/bigdecimal_internal.h"
#include "../src/internal/numforge_alloc.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* EXPERIMENT ONLY. This is not selected by any production function.
 * Fixed extra precision and a tiny first omitted theta term are feasibility
 * parameters, not a proof of correct rounding. Includes cold dynamic pi.
 * Private results are committed only on success; every scratch owner is local. */
static BigDecimalStatus experimental_ln(BigDecimal *result, const BigDecimal *value,
    int64_t digits, int64_t extra, BigDecimalRoundingMode rounding,
    unsigned *expansions, unsigned *iterations)
{
    enum { Y, ONE, TWO, FOUR, LIMIT, Q, Q2, Q4, Q8, Q9, Q16, Q25,
        T2, T3, A, B, NEXT_A, NEXT_B, PRODUCT, PI, ANSWER, COUNT };
    BigDecimal *v[COUNT] = {0};
    BigDecimalWorkspace ws = {0};
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    const BigDecimalRoundingMode nearest = BIGDECIMAL_ROUND_HALF_EVEN;
    int comparison = 0, sign = 0;
    int64_t work;
    bool negative = false;
    *expansions = *iterations = 0U;
    if (digits < 1 || digits > 10000 || extra < 40 || extra > 512 ||
        !bigdecimal_valid_rounding(rounding)) return BIGDECIMAL_INVALID_ARGUMENT;
    work = digits + extra;
#define TRY(op) do { status = (op); if (status != BIGDECIMAL_OK) goto cleanup; } while (0)
#define MUL(out, a, b) TRY(bigdecimal_mul_significant_workspace(v[out], v[a], v[b], work, nearest, &ws))
#define ADD(out, a, b) TRY(bigdecimal_add_significant_workspace(v[out], v[a], v[b], work, nearest, &ws))
#define DIV(out, a, b) TRY(bigdecimal_div_significant_workspace(v[out], v[a], v[b], work, nearest, &ws))
    TRY(bigdecimal_sign(&sign, value));
    if (sign <= 0) { status = BIGDECIMAL_INVALID_ARGUMENT; goto cleanup; }
    for (unsigned i = 0; i < COUNT; i++)
    {
        v[i] = bigdecimal_create();
        if (v[i] == NULL) { status = BIGDECIMAL_OUT_OF_MEMORY; goto cleanup; }
    }
    TRY(bigdecimal_set_string(v[ONE], "1"));
    TRY(bigdecimal_set_string(v[TWO], "2"));
    TRY(bigdecimal_set_string(v[FOUR], "4"));
    TRY(bigdecimal_compare(&comparison, value, v[ONE]));
    if (comparison == 0)
    {
        TRY(bigdecimal_set_string(v[ANSWER], "0"));
        goto publish;
    }
    /* Preserve the well-conditioned atanh path close to one. This provisional
     * eligibility band is not a measured precision crossover. */
    TRY(bigdecimal_set_string(v[LIMIT], "0.999"));
    TRY(bigdecimal_compare(&comparison, value, v[LIMIT]));
    if (comparison >= 0)
    {
        TRY(bigdecimal_set_string(v[LIMIT], "1.001"));
        TRY(bigdecimal_compare(&comparison, value, v[LIMIT]));
        if (comparison <= 0)
        {
            status = bigdecimal_ln(result, value, digits, rounding);
            goto cleanup;
        }
    }
    TRY(bigdecimal_compare(&comparison, value, v[ONE]));
    negative = comparison < 0;
    if (negative)
        TRY(bigdecimal_div_significant_workspace(v[Y], v[ONE], value, work, nearest, &ws));
    else TRY(bigdecimal_copy(v[Y], value));
    TRY(bigdecimal_set_string(v[LIMIT], "1"));
    v[LIMIT]->scale = -((digits + 40 + 35) / 36);
    for (;;)
    {
        if (!numforge_budget_check()) { status = BIGDECIMAL_OUT_OF_MEMORY; goto cleanup; }
        TRY(bigdecimal_compare(&comparison, v[Y], v[LIMIT]));
        if (comparison >= 0) break;
        if (*expansions == 256U) { status = BIGDECIMAL_VALUE_TOO_LARGE; goto cleanup; }
        MUL(Y, Y, Y);
        (*expansions)++;
    }
    DIV(Q, ONE, Y);
    MUL(Q2, Q, Q);
    MUL(Q4, Q2, Q2);
    MUL(Q8, Q4, Q4);
    MUL(Q9, Q8, Q);
    MUL(Q16, Q8, Q8);
    MUL(Q25, Q16, Q9);
    ADD(T2, Q, Q9);
    ADD(T2, T2, Q25);
    MUL(T2, T2, TWO);
    ADD(T3, Q4, Q16);
    MUL(T3, T3, TWO);
    ADD(T3, T3, ONE);
    MUL(A, T2, T2);
    MUL(B, T3, T3);
    /* Feasibility stopping criterion; deliberately not a certified enclosure. */
    for (;;)
    {
        if (!numforge_budget_check()) { status = BIGDECIMAL_OUT_OF_MEMORY; goto cleanup; }
        if (*iterations == 256U) { status = BIGDECIMAL_VALUE_TOO_LARGE; goto cleanup; }
        ADD(NEXT_A, A, B);
        DIV(NEXT_A, NEXT_A, TWO);
        MUL(PRODUCT, A, B);
        TRY(bigdecimal_sqrt(v[NEXT_B], v[PRODUCT], work, nearest));
        (*iterations)++;
        TRY(bigdecimal_compare(&comparison, v[NEXT_A], v[A]));
        { BigDecimal *swap = v[A]; v[A] = v[NEXT_A]; v[NEXT_A] = swap; }
        { BigDecimal *swap = v[B]; v[B] = v[NEXT_B]; v[NEXT_B] = swap; }
        if (comparison == 0) break;
    }
    TRY(bigdecimal_set_constant_significant(v[PI], BIGDECIMAL_CONSTANT_PI, work, nearest));
    MUL(A, A, FOUR);
    for (unsigned i = 0; i < *expansions; i++) MUL(A, A, TWO);
    DIV(ANSWER, PI, A);
    if (negative) TRY(bigdecimal_negate(v[ANSWER], v[ANSWER]));
    TRY(bigdecimal_round_significant_workspace(v[ANSWER], v[ANSWER], digits, rounding, &ws));
publish:
    bigdecimal_commit(result, v[ANSWER]);
    v[ANSWER] = NULL;
    status = BIGDECIMAL_OK;
cleanup:
    for (unsigned i = 0; i < COUNT; i++) bigdecimal_destroy(v[i]);
    bigdecimal_workspace_clear(&ws);
    return status;
#undef DIV
#undef ADD
#undef MUL
#undef TRY
}

#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
static int fault_checks(void)
{
    size_t checked = 0U;
    if (!numforge_alloc_stats_track(true)) return EXIT_FAILURE;
    for (unsigned alias = 0; alias < 2; alias++)
    for (unsigned deadlines = 0; deadlines < 2; deadlines++)
    {
        bool completed = false;
        for (size_t index = 1U; index <= 65536U; index++)
        {
            BigDecimal *a = bigdecimal_create(), *out = bigdecimal_create();
            BigDecimal *original = bigdecimal_create(), *reference = bigdecimal_create();
            bool good = a && out && original && reference &&
                bigdecimal_set_string(a, "2") == BIGDECIMAL_OK &&
                bigdecimal_set_string(out, "7") == BIGDECIMAL_OK &&
                bigdecimal_copy(original, alias ? a : out) == BIGDECIMAL_OK &&
                bigdecimal_ln(reference, a, 34, BIGDECIMAL_ROUND_HALF_EVEN) == BIGDECIMAL_OK;
            unsigned expansions, iterations;
            int comparison;
            BigDecimalStatus status = BIGDECIMAL_INVALID_ARGUMENT;
            if (good)
            {
                numforge_test_allocator_begin(deadlines ? 0U : index);
                if (deadlines)
                {
                    good = numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX);
                    numforge_test_budget_expire_after(index);
                }
                if (good) status = experimental_ln(alias ? a : out, a, 34, 80,
                    BIGDECIMAL_ROUND_HALF_EVEN, &expansions, &iterations);
                if (deadlines) numforge_budget_end();
                numforge_test_allocator_end();
                checked++;
                good = good && (status == BIGDECIMAL_OK || status == BIGDECIMAL_OUT_OF_MEMORY) &&
                    bigdecimal_compare(&comparison, alias ? a : out,
                        status == BIGDECIMAL_OK ? reference : original) == BIGDECIMAL_OK && comparison == 0;
                if (!alias)
                {
                    BigDecimal *two = bigdecimal_create();
                    good = good && two && bigdecimal_set_string(two, "2") == BIGDECIMAL_OK &&
                        bigdecimal_compare(&comparison, a, two) == BIGDECIMAL_OK && comparison == 0;
                    bigdecimal_destroy(two);
                }
            }
            bigdecimal_destroy(a); bigdecimal_destroy(out);
            bigdecimal_destroy(original); bigdecimal_destroy(reference);
            good = good && numforge_alloc_stats_complete() && numforge_alloc_stats_live() == 0U;
            if (!good) { fprintf(stderr, "Fault check failed: alias=%u deadline=%u index=%zu\n", alias, deadlines, index); return EXIT_FAILURE; }
            if (status == BIGDECIMAL_OK) { completed = true; break; }
        }
        if (!completed) return EXIT_FAILURE;
    }
    if (!numforge_alloc_stats_track(false)) return EXIT_FAILURE;
    printf("%zu allocation/deadline checkpoints passed with separate and aliased outputs\n", checked);
    return EXIT_SUCCESS;
}
#endif

static bool integer(const char *text, int64_t *value)
{
    char *end;
    errno = 0;
    long long parsed = strtoll(text, &end, 10);
    if (errno || !*text || *end) return false;
    *value = (int64_t)parsed;
    return true;
}

static bool expected_file(BigDecimal *expected, const char *path)
{
    char text[24000];
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    size_t size = fread(text, 1U, sizeof(text)-1U, file);
    bool good = !ferror(file) && feof(file);
    fclose(file);
    text[size] = '\0';
    return good && bigdecimal_set_string(expected, text) == BIGDECIMAL_OK;
}

int main(int argc, char **argv)
{
#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
    if (argc == 2 && !strcmp(argv[1], "--fault-checks")) return fault_checks();
#endif
    int64_t digits, extra, rounding, deadline;
    bool memory = argc == 9 && !strcmp(argv[8], "--memory");
    bool alias = argc == 9 && !strcmp(argv[8], "--alias");
    if ((argc != 8 && !memory && !alias) ||
        (strcmp(argv[1], "agm") && strcmp(argv[1], "series")) ||
        !integer(argv[3], &digits) || !integer(argv[4], &extra) ||
        !integer(argv[5], &rounding) || !integer(argv[6], &deadline) ||
        digits < 1 || digits > 10000 || extra < 40 || extra > 512 ||
        rounding < BIGDECIMAL_ROUND_TOWARD_ZERO || rounding > BIGDECIMAL_ROUND_HALF_EVEN || deadline < 0)
    {
        fputs("Usage: logarithm_agm_benchmark agm|series input digits extra rounding deadline-ms reference-file|- [--memory|--alias]\n", stderr);
        return EXIT_FAILURE;
    }
    if (memory && !numforge_alloc_stats_track(true)) return EXIT_FAILURE;
    BigDecimal *a = bigdecimal_create(), *out = bigdecimal_create();
    BigDecimal *expected = bigdecimal_create(), *original = bigdecimal_create();
    int exit_code = EXIT_FAILURE, comparison = 1;
    char *formatted = NULL;
    if (!a || !out || !expected || !original ||
        bigdecimal_set_string(a, argv[2]) != BIGDECIMAL_OK ||
        bigdecimal_set_string(out, "7") != BIGDECIMAL_OK ||
        bigdecimal_copy(original, alias ? a : out) != BIGDECIMAL_OK) goto cleanup;
    if (strcmp(argv[7], "-") && !expected_file(expected, argv[7])) goto cleanup;
    numforge_alloc_stats_reset();
    if (!numforge_budget_begin((uint64_t)deadline, 64U*1024U*1024U, 512U*1024U)) goto cleanup;
    unsigned expansions = 0U, iterations = 0U;
    double begin = benchmark_seconds();
    BigDecimal *destination = alias ? a : out;
    BigDecimalStatus status = !strcmp(argv[1], "agm") ?
        experimental_ln(destination, a, digits, extra, (BigDecimalRoundingMode)rounding, &expansions, &iterations) :
        bigdecimal_ln(destination, a, digits, (BigDecimalRoundingMode)rounding);
    double end = benchmark_seconds();
    size_t calls = numforge_alloc_stats_calls(), bytes = numforge_alloc_stats_bytes();
    size_t peak = numforge_alloc_stats_peak();
    NumForgeBudgetFailure failure = numforge_budget_failure();
    numforge_budget_end();
    bool checked = false;
    if (status == BIGDECIMAL_OK)
    {
        if (strcmp(argv[7], "-"))
            checked = bigdecimal_compare(&comparison, destination, expected) == BIGDECIMAL_OK && comparison == 0;
        if (bigdecimal_to_string(destination, &formatted) != BIGDECIMAL_OK) goto cleanup;
    }
    else
    {
        if (bigdecimal_compare(&comparison, destination, original) != BIGDECIMAL_OK || comparison != 0) goto cleanup;
    }
    if (begin < 0 || end < begin) goto cleanup;
    printf("algorithm,digits,extra,rounding,status,elapsed_ms,alloc_calls,requested_bytes,peak_live,expansions,agm_iterations,budget_reason,reference_match,output_bytes\n");
    printf("%s,%" PRId64 ",%" PRId64 ",%" PRId64 ",%d,%.6f,%zu,%zu,%zu,%u,%u,%s,%d,%zu\n",
        argv[1], digits, extra, rounding, (int)status, (end-begin)*1000.0,
        calls, bytes, memory ? peak : 0U, expansions, iterations,
        failure == NUMFORGE_BUDGET_TIME ? "time" : failure == NUMFORGE_BUDGET_MEMORY ? "allocation" : "none",
        checked ? 1 : 0, formatted ? strlen(formatted) : 0U);
    exit_code = !strcmp(argv[7], "-") || checked ? EXIT_SUCCESS : EXIT_FAILURE;
cleanup:
    free(formatted);
    bigdecimal_destroy(a); bigdecimal_destroy(out);
    bigdecimal_destroy(expected); bigdecimal_destroy(original);
    if (memory && (!numforge_alloc_stats_complete() || numforge_alloc_stats_live() != 0U ||
        !numforge_alloc_stats_track(false))) exit_code = EXIT_FAILURE;
    return exit_code;
}
