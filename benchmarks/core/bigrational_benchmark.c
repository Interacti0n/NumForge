#include <numforge/bigrational.h>
#include "benchmarks/common/benchmark_clock.h"
#include "src/runtime/numforge_alloc.h"
#include "src/runtime/instrumentation/decimal_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Fixture construction, full reference checks and aliases are outside timing.
 * --profile is a separate diagnostic pass; its clocks must not be compared
 * with ordinary timings. Allocation traffic is not peak/live memory. */
typedef struct Case {
    const char *name, *operation, *expected;
    BigRational *a, *b, *out;
    BigDecimal *decimal;
    int comparison;
} Case;

static int execute(Case *c, BigRational *out)
{
    if (!strncmp(c->operation, "convert", 7) && strlen(c->operation) == 8 &&
        c->operation[7] >= '0' && c->operation[7] <= '5')
        return bigrational_to_bigdecimal(c->decimal, c->a, 17, (BigDecimalRoundingMode)(c->operation[7]-'0'));
    if (!strcmp(c->operation, "add")) return bigrational_add(out, c->a, c->b);
    if (!strcmp(c->operation, "sub")) return bigrational_sub(out, c->a, c->b);
    if (!strcmp(c->operation, "mul")) return bigrational_mul(out, c->a, c->b);
    if (!strcmp(c->operation, "square")) return bigrational_mul(out, c->a, c->a);
    if (!strcmp(c->operation, "div")) return bigrational_div(out, c->a, c->b);
    if (!strcmp(c->operation, "selfdiv")) return bigrational_div(out, c->a, c->a);
    if (!strcmp(c->operation, "compare")) return bigrational_compare(&c->comparison, c->a, c->b);
    return BIGRATIONAL_INVALID_ARGUMENT;
}

static int check(Case *c, BigRational *out)
{
    if (!strcmp(c->operation, "compare"))
        return (c->comparison > 0) - (c->comparison < 0) == atoi(c->expected);
    char *text = NULL;
    int converted = !strncmp(c->operation, "convert", 7);
    int valid = (converted ? bigdecimal_to_string(c->decimal, &text) == BIGDECIMAL_OK :
        bigrational_to_string(out, &text) == BIGRATIONAL_OK) && !strcmp(text, c->expected);
    free(text);
    return valid;
}

static int set_fraction(BigRational *out, const char *top, const char *bottom)
{
    BigInt *n = bigint_create(), *d = bigint_create();
    int valid = n && d && bigint_set_string(n, top) == BIGINT_OK &&
        bigint_set_string(d, bottom) == BIGINT_OK && bigrational_set_fraction(out, n, d) == BIGRATIONAL_OK;
    bigint_destroy(n); bigint_destroy(d);
    return valid;
}

static int run(Case *c, int validate, int quick, int profile)
{
    if (execute(c, c->out) != BIGRATIONAL_OK || !check(c, c->out)) return 0;
    if (validate && strncmp(c->operation, "convert", 7)) {
        /* Both destination aliases, restoring the operands before each call. */
        BigRational *a = bigrational_create(), *b = bigrational_create();
        int valid = a && b && bigrational_copy(a, c->a) == BIGRATIONAL_OK &&
            bigrational_copy(b, c->b) == BIGRATIONAL_OK;
        for (int side = 0; valid && side < 2; ++side) {
            BigRational *out = side ? c->b : c->a;
            valid = execute(c, out) == BIGRATIONAL_OK && check(c, out) &&
                bigrational_copy(c->a, a) == BIGRATIONAL_OK && bigrational_copy(c->b, b) == BIGRATIONAL_OK;
        }
        bigrational_destroy(a); bigrational_destroy(b);
        if (!valid) return 0;
    }
    double samples[5] = {0};
    size_t count = validate || profile ? 1U : quick ? 3U : 5U;
    for (size_t sample = 0; sample < count; ++sample) {
        size_t iterations = 0;
        double begin = benchmark_seconds(), end;
        if (begin < 0) return 0;
        do {
            if (execute(c, c->out) != BIGRATIONAL_OK) return 0;
            ++iterations;
            end = benchmark_seconds();
            if (end < begin) return 0;
        } while (!validate && !profile && end - begin < (quick ? 0.0002 : 0.002));
        samples[sample] = (end - begin) * 1e9 / (double)iterations;
        if (!check(c, c->out)) return 0;
    }
    for (size_t i = 1; i < count; ++i) for (size_t j = i; j && samples[j] < samples[j-1]; --j) {
        double t = samples[j]; samples[j] = samples[j-1]; samples[j-1] = t;
    }
    numforge_alloc_stats_reset();
    if (profile) numforge_decimal_profile_reset();
    int status = execute(c, c->out);
    if (profile) numforge_decimal_profile_stop();
    size_t calls = numforge_alloc_stats_calls(), bytes = numforge_alloc_stats_bytes();
    if (status != BIGRATIONAL_OK || !check(c, c->out) ||
        (profile && !numforge_decimal_profile_complete())) return 0;
    printf("%s,%s,%zu,%.3f,%.3f,%.3f,%zu,%zu", c->name, c->operation, count,
        samples[0], samples[count/2], samples[count-1], calls, bytes);
    if (profile) {
        const NumForgeDecimalOperation ops[] = {NUMFORGE_DECIMAL_GCD, NUMFORGE_DECIMAL_DIV, NUMFORGE_DECIMAL_MUL};
        for (size_t i = 0; i < 3; ++i) printf(",%zu,%.3f", numforge_decimal_operation_calls(ops[i]),
            numforge_decimal_operation_seconds(ops[i]) * 1e9);
    }
    puts(",verified");
    return 1;
}

int main(int argc, char **argv)
{
    int validate = 0, quick = 0, profile = 0, memory = 0, success = 1, found = 0;
    if (argc < 2) return 2;
    for (int i = 2; i < argc; ++i) {
        if (!strcmp(argv[i], "--validate")) validate = 1;
        else if (!strcmp(argv[i], "--quick")) quick = 1;
        else if (!strcmp(argv[i], "--profile")) profile = 1;
        else if (!strcmp(argv[i], "--memory")) { memory = 1; validate = 1; }
        else return 2;
    }
    FILE *file = fopen(argv[1], "r");
    char *line = malloc(65536U);
    if (!file || !line) { if (file) fclose(file); free(line); return 2; }
    printf("case,operation,samples,min_ns,median_ns,max_ns,allocations,requested_bytes");
    if (profile) printf(",gcd_calls,gcd_ns,div_calls,div_ns,mul_calls,mul_ns");
    puts(",verification");
    while (fgets(line, 65536, file)) {
        if (line[0] == '#') continue;
        if (!strchr(line, '\n')) { success = 0; break; }
        char *fields[7], *part = strtok(line, "\t\r\n"); size_t n = 0;
        while (part && n < 7) { fields[n++] = part; part = strtok(NULL, "\t\r\n"); }
        if (n != 7 || part) { success = 0; break; }
        if (memory && !numforge_alloc_stats_track(true)) { success = 0; break; }
        Case c = {fields[0], fields[1], fields[6], bigrational_create(), bigrational_create(), bigrational_create(), bigdecimal_create(), 0};
        int valid = c.a && c.b && c.out && c.decimal && set_fraction(c.a, fields[2], fields[3]) &&
            set_fraction(c.b, fields[4], fields[5]) && run(&c, validate, quick, profile);
        if (!valid) fprintf(stderr, "Failed rational fixture: %s\n", c.name);
        bigrational_destroy(c.a); bigrational_destroy(c.b); bigrational_destroy(c.out);
        bigdecimal_destroy(c.decimal);
        if (memory && (!numforge_alloc_stats_complete() || numforge_alloc_stats_live() != 0 ||
            !numforge_alloc_stats_track(false))) valid = 0;
        if (!valid) { success = 0; break; }
        found = 1;
    }
    if (ferror(file)) success = 0;
    fclose(file); free(line);
    return success && found ? 0 : 1;
}
