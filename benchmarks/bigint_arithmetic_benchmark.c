#include "benchmark_clock.h"
#include "karatsuba_experiment.h"
#include "../src/bigint/bigint_internal.h"
#include "../src/internal/numforge_alloc.h"
#include "../src/internal/benchmark_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Private experiments only. Production algorithms and public API are unchanged.
 * Fixtures/references come from independent Node BigInt, outside timed regions.
 * Every invocation restores alias inputs outside timing. No chained products. */
typedef struct Case {
    char *name, *op, *mode;
    BigInt *a, *b, *expected, *remainder;
} Case;
static bool enforce_application_budget;

/* Large independent references can use hex: limbs to avoid quadratic decimal
 * fixture parsing. This is fixture ingestion only, never timed arithmetic. */
static BigIntStatus parse_fixture(BigInt *out, const char *text)
{
    if (strncmp(text, "hex:", 4U)) return bigint_set_string(out, text);
    text += 4U;
    bool negative = *text == '-';
    if (negative) ++text;
    size_t digits = strlen(text);
    if (!digits) return BIGINT_INVALID_ARGUMENT;
    BigInt temporary = {0};
    temporary.size = temporary.capacity = (digits + 15U) / 16U;
    temporary.limbs = numforge_calloc(temporary.size, sizeof(uint64_t));
    if (!temporary.limbs) return BIGINT_OUT_OF_MEMORY;
    for (size_t i = 0U; i < digits; ++i)
    {
        unsigned char c = (unsigned char)text[digits - 1U - i];
        unsigned nibble = c >= '0' && c <= '9' ? (unsigned)(c - '0') :
            c >= 'a' && c <= 'f' ? (unsigned)(c - 'a') + 10U :
            c >= 'A' && c <= 'F' ? (unsigned)(c - 'A') + 10U : 16U;
        if (nibble == 16U) { free(temporary.limbs); return BIGINT_INVALID_ARGUMENT; }
        temporary.limbs[i / 16U] |= (uint64_t)nibble << ((i % 16U) * 4U);
    }
    temporary.is_negative = negative;
    bigint_normalize(&temporary); bigint_commit(out, &temporary); free(temporary.limbs);
    return BIGINT_OK;
}

static bool add_word(uint64_t *words, size_t count, size_t offset, uint64_t value)
{
    while (value != 0U && offset < count)
    {
        uint64_t previous = words[offset];
        words[offset++] += value;
        value = words[offset - 1U] < previous ? 1U : 0U;
    }
    return value == 0U;
}

/* Compute diagonal products once, off-diagonal products once and add twice.
 * Carry propagation is explicit; no compiler-specific 128-bit type required. */
static BigIntStatus square(BigInt *out, const BigInt *a)
{
    BigInt temp = {0};
    if (a->size == 0U) return bigint_set_uint64(out, 0U);
    if (bigint_size_mul(a->size, 2U, &temp.size) != BIGINT_OK || temp.size > BIGINT_MAX_LIMBS)
        return BIGINT_VALUE_TOO_LARGE;
    temp.capacity = temp.size;
    temp.limbs = numforge_calloc(temp.size, sizeof(*temp.limbs));
    if (temp.limbs == NULL) return BIGINT_OUT_OF_MEMORY;
    for (size_t i = 0U; i < a->size; i++)
    {
        if (!numforge_budget_check()) { free(temp.limbs); return BIGINT_OUT_OF_MEMORY; }
        for (size_t j = i; j < a->size; j++)
        {
            uint64_t hi, lo;
            bigint_multiply_u64_u64(a->limbs[i], a->limbs[j], &hi, &lo);
            unsigned repeats = i == j ? 1U : 2U;
            for (unsigned k = 0U; k < repeats; k++)
            {
                if (!add_word(temp.limbs, temp.size, i + j, lo) ||
                    !add_word(temp.limbs, temp.size, i + j + 1U, hi))
                { free(temp.limbs); return BIGINT_VALUE_TOO_LARGE; }
            }
        }
    }
    bigint_normalize(&temp);
    bigint_commit(out, &temp);
    free(temp.limbs);
    return BIGINT_OK;
}

static BigIntStatus product_tree(BigInt *out, uint64_t low, uint64_t high)
{
    if (low > high) return bigint_set_uint64(out, 1U);
    if (low == high) return bigint_set_uint64(out, low);
    BigInt left = {0}, right = {0};
    uint64_t middle = low + (high - low) / 2U;
    BigIntStatus status = product_tree(&left, low, middle);
    if (status == BIGINT_OK) status = product_tree(&right, middle + 1U, high);
    if (status == BIGINT_OK) status = bigint_mul(out, &left, &right);
    free(left.limbs); free(right.limbs);
    return status;
}

/* Compare the same 32-factor leaves with classical versus Karatsuba products.
 * This is independent of the unchanged production grouped factorial. */
static BigIntStatus grouped_tree(BigInt *out, uint64_t low, uint64_t high, size_t threshold)
{
    if (!numforge_budget_check()) return BIGINT_OUT_OF_MEMORY;
    if (low > high) return bigint_set_uint64(out, 1U);
    BigInt left = {0}, right = {0};
    BigIntStatus status;
    if (high - low < 32U)
    {
        status = bigint_set_uint64(&left, 1U);
        for (uint64_t factor = low; status == BIGINT_OK; ++factor)
        {
            if (!numforge_budget_check()) { status = BIGINT_OUT_OF_MEMORY; break; }
            status = bigint_multiply_by_uint64(&left, factor);
            if (factor == high) break;
        }
        if (status == BIGINT_OK) bigint_commit(out, &left);
    }
    else
    {
        uint64_t middle = low + (high - low) / 2U;
        status = grouped_tree(&left, low, middle, threshold);
        if (status == BIGINT_OK) status = grouped_tree(&right, middle + 1U, high, threshold);
        if (status == BIGINT_OK)
            status = threshold ? benchmark_karatsuba(out, &left, &right, threshold) : bigint_mul(out, &left, &right);
    }
    free(left.limbs); free(right.limbs);
    return status;
}

static BigIntStatus power_square(BigInt *out, const BigInt *a, const BigInt *b)
{
    BigInt result = {0}, base = {0};
    if (b->is_negative) return BIGINT_NEGATIVE_ARGUMENT;
    BigIntStatus status = bigint_set_uint64(&result, 1U);
    if (status == BIGINT_OK) status = bigint_copy(&base, a);
    size_t bits = bigint_bit_length(b);
    for (size_t i = 0U; status == BIGINT_OK && i < bits; i++)
    {
        if (bigint_get_bit(b, i)) status = bigint_mul(&result, &result, &base);
        if (status == BIGINT_OK && i + 1U < bits) status = square(&base, &base);
    }
    if (status == BIGINT_OK) status = bigint_copy(out, &result);
    free(result.limbs); free(base.limbs);
    return status;
}

static BigIntStatus operate(const Case *c, BigInt *out, BigInt *rem, BigInt *a, BigInt *b)
{
    if (!strcmp(c->op, "mul")) return bigint_mul(out, a, !strcmp(c->mode, "alias_all") ? a : b);
    if (!strncmp(c->op, "karatsuba", 9U))
    {
        const char *suffix = c->op + 9U;
        size_t threshold = !strcmp(suffix, "16") ? 16U : !strcmp(suffix, "32") ? 32U :
            !strcmp(suffix, "64") ? 64U : !strcmp(suffix, "128") ? 128U :
            !strcmp(suffix, "256") ? 256U : 0U;
        return benchmark_karatsuba(out, a, !strcmp(c->mode, "alias_all") ? a : b, threshold);
    }
    if (!strcmp(c->op, "square")) return square(out, a);
    if (!strcmp(c->op, "copy")) return bigint_copy(out, a);
    if (!strcmp(c->op, "pow")) return bigint_pow(out, a, b);
    if (!strcmp(c->op, "pow_square")) return power_square(out, a, b);
    if (!strcmp(c->op, "factorial")) return bigint_factorial(out, a);
    if (!strcmp(c->op, "mul_workspace"))
    {
        BigIntMultiplicationWorkspace workspace = {0};
        BigIntStatus status = bigint_mul_workspace(out, a, !strcmp(c->mode, "alias_all") ? a : b, &workspace);
        bigint_multiplication_workspace_free(&workspace);
        return status;
    }
    if (!strcmp(c->op, "tree_workspace") || !strcmp(c->op, "factorial_grouped"))
    {
        if (a->is_negative || a->size > 1U) return BIGINT_INVALID_ARGUMENT;
        uint64_t n = a->size ? a->limbs[0] : 0U;
        return !strcmp(c->op, "tree_workspace") ? bigint_factorial_tree(out, n) : bigint_factorial_grouped(out, n);
    }
    if (!strcmp(c->op, "tree")) return product_tree(out, 2U, a->size ? a->limbs[0] : 0U);
    if (!strcmp(c->op, "tree_grouped") || !strncmp(c->op, "tree_karatsuba", 14U))
    {
        if (a->is_negative || a->size > 1U) return BIGINT_INVALID_ARGUMENT;
        size_t threshold = !strcmp(c->op, "tree_grouped") ? 0U :
            !strcmp(c->op, "tree_karatsuba16") ? 16U :
            !strcmp(c->op, "tree_karatsuba32") ? 32U : !strcmp(c->op, "tree_karatsuba64") ? 64U :
            !strcmp(c->op, "tree_karatsuba128") ? 128U : 0U;
        if (!threshold && strcmp(c->op, "tree_grouped")) return BIGINT_INVALID_ARGUMENT;
        return grouped_tree(out, 2U, a->size ? a->limbs[0] : 0U, threshold);
    }
    if (!strcmp(c->op, "divmod")) return bigint_div_mod(out, rem, a, b);
    if (!strcmp(c->op, "div")) return bigint_div(out, a, b);
    if (!strcmp(c->op, "mod")) return bigint_mod(out, a, b);
    if (!strcmp(c->op, "gcd")) return bigint_gcd(out, a, b);
    return BIGINT_INVALID_ARGUMENT;
}

#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
/* Exhaust every allocation failure in the private experiments, including an
 * aliased output. Checks remain active under NDEBUG. Never time this work. */
static bool check_experiment_failures(void)
{
    if (!numforge_alloc_stats_track(true)) return false;
    const char *operations[] = {"square", "tree", "pow_square", "tree_grouped", "tree_karatsuba64", "tree_karatsuba16", "tree_workspace"};
    for (size_t op = 0U; op < 7U; op++) for (unsigned alias = 0U; alias < 2U; alias++)
    {
        BigInt a = {0}, b = {0}, result = {0}, remainder = {0}, original = {0};
        Case c = {NULL, (char *)operations[op], "reuse", NULL, NULL, NULL, NULL};
        BigInt *out = alias ? &a : &result;
        const char *input = op >= 3U ? "2000" : op == 1U ? "20" : "340282366920938463463374607431768211455";
        bool good = bigint_set_string(&a, input) == BIGINT_OK && bigint_set_uint64(&b, 17U) == BIGINT_OK &&
            bigint_set_uint64(&result, 123U) == BIGINT_OK;
        numforge_test_allocator_begin(0U);
        BigIntStatus status = good ? operate(&c, out, &remainder, &a, &b) : BIGINT_OUT_OF_MEMORY;
        size_t allocations = numforge_test_allocator_call_count();
        numforge_test_allocator_end();
        if (status != BIGINT_OK || allocations == 0U) good = false;
        for (size_t i = 1U; good && i <= allocations; i++)
        {
            free(a.limbs); memset(&a, 0, sizeof(a));
            free(result.limbs); memset(&result, 0, sizeof(result));
            good = bigint_set_string(&a, input) == BIGINT_OK && bigint_set_uint64(&result, 123U) == BIGINT_OK &&
                bigint_copy(&original, out) == BIGINT_OK;
            size_t baseline = numforge_alloc_stats_live();
            numforge_test_allocator_begin(i);
            status = good ? operate(&c, out, &remainder, &a, &b) : BIGINT_OUT_OF_MEMORY;
            bool failed = numforge_test_allocator_did_fail();
            numforge_test_allocator_end();
            if (!failed || status != BIGINT_OUT_OF_MEMORY || bigint_compare(out, &original) != 0 ||
                numforge_alloc_stats_live() != baseline) good = false;
        }
        if (good && op == 6U)
        {
            bool completed = false;
            for (size_t checkpoint = 1U; good && checkpoint < 50000U; ++checkpoint)
            {
                good = bigint_set_string(&a, input) == BIGINT_OK && bigint_set_uint64(&result, 123U) == BIGINT_OK &&
                    bigint_copy(&original, out) == BIGINT_OK;
                size_t baseline = numforge_alloc_stats_live();
                bool owner = numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX);
                if (!owner) { good = false; break; }
                numforge_test_budget_expire_after(checkpoint);
                status = operate(&c, out, &remainder, &a, &b);
                NumForgeBudgetFailure reason = numforge_budget_failure();
                numforge_budget_end();
                if (status == BIGINT_OK) { completed = true; break; }
                good = status == BIGINT_OUT_OF_MEMORY && reason == NUMFORGE_BUDGET_TIME &&
                    bigint_compare(out, &original) == 0 && numforge_alloc_stats_live() == baseline;
            }
            if (!completed) good = false;
        }
        if (good)
        {
            good = bigint_set_string(&a, input) == BIGINT_OK && bigint_set_uint64(&result, 123U) == BIGINT_OK &&
                bigint_copy(&original, out) == BIGINT_OK;
            bool owner = numforge_budget_begin(UINT64_MAX, 0U, 0U);
            status = good && owner ? operate(&c, out, &remainder, &a, &b) : BIGINT_INVALID_ARGUMENT;
            if (owner) numforge_budget_end();
            if (!owner || status != BIGINT_OUT_OF_MEMORY || bigint_compare(out, &original) != 0) good = false;
        }
        free(a.limbs); free(b.limbs); free(result.limbs); free(remainder.limbs); free(original.limbs);
        if (!good) { fprintf(stderr, "Private experiment failure contract: %s alias=%u\n", operations[op], alias); return false; }
    }
    return numforge_alloc_stats_live() == 0U && numforge_alloc_stats_complete() &&
        numforge_alloc_stats_track(false);
}
#endif

typedef struct Measurement {
    double seconds;
    size_t calls, bytes, baseline, live, peak, iterations;
} Measurement;

static bool measure(const Case *c, bool memory, double target, Measurement *m, bool warm)
{
    BigInt a = {0}, b = {0}, result = {0}, remainder = {0};
    BigInt *out = &result, *rem = &remainder;
    bool success = false;
    memset(m, 0, sizeof(*m));
    if (memory && !numforge_alloc_stats_track(true)) return false;
    if (bigint_copy(&a, c->a) != BIGINT_OK || bigint_copy(&b, c->b) != BIGINT_OK) goto cleanup;
    if (!strcmp(c->mode, "alias_a") || !strcmp(c->mode, "alias_all") || !strcmp(c->mode, "alias_pair")) out = &a;
    if (!strcmp(c->mode, "alias_b")) out = &b;
    if (!strcmp(c->mode, "alias_pair")) rem = &b;
    /* Warm destination outside timing, except explicit fresh-output cases. */
    if (warm && out == &result && strcmp(c->mode, "cold") &&
        operate(c, out, rem, &a, &b) != BIGINT_OK) goto cleanup;
    do
    {
        if (bigint_copy(&a, c->a) != BIGINT_OK || bigint_copy(&b, c->b) != BIGINT_OK) goto cleanup;
        if (!strcmp(c->mode, "cold")) { free(result.limbs); memset(&result, 0, sizeof(result)); }
        m->baseline = numforge_alloc_stats_live();
        numforge_alloc_stats_reset();
        bool owner = false;
        if (enforce_application_budget)
        {
            owner = numforge_budget_begin(5000U, 64U * 1024U * 1024U, 512U * 1024U);
            if (!owner) goto cleanup;
        }
        double begin = benchmark_seconds();
        BigIntStatus status = operate(c, out, rem, &a, &b);
        double end = benchmark_seconds();
        NumForgeBudgetFailure reason = numforge_budget_failure();
        if (owner) numforge_budget_end();
        if (status != BIGINT_OK && enforce_application_budget)
            fprintf(stderr, "Budget rejection: %s status=%d reason=%d elapsed_ms=%.3f\n",
                    c->name, (int)status, (int)reason, (end - begin) * 1000.0);
        m->calls = numforge_alloc_stats_calls(); m->bytes = numforge_alloc_stats_bytes();
        m->live = numforge_alloc_stats_live(); m->peak = numforge_alloc_stats_peak();
        if (status != BIGINT_OK || begin < 0.0 || end < begin ||
            bigint_compare(out, c->expected) != 0 ||
            (!strcmp(c->op, "divmod") && bigint_compare(rem, c->remainder) != 0)) goto cleanup;
        m->seconds += end - begin;
        m->iterations++;
    } while (!memory && m->seconds < target && m->iterations < 65536U);
    success = true;
cleanup:
    free(a.limbs); free(b.limbs); free(result.limbs); free(remainder.limbs);
    if (memory)
    {
        if (!numforge_alloc_stats_complete() || numforge_alloc_stats_live() != 0U ||
            !numforge_alloc_stats_track(false)) success = false;
    }
    return success;
}

static bool run(const Case *c, bool quick, bool validate)
{
    Measurement m;
    double samples[7];
    size_t count = validate ? 1U : quick ? 3U : 7U;
    if (!validate)
        for (size_t i = 0U; i < 3U; i++) if (!measure(c, false, 0.0, &m, true)) return false;
    for (size_t i = 0U; i < count; i++)
    {
        if (!measure(c, false, validate ? 0.0 : quick ? 0.001 : 0.005, &m, !validate)) return false;
        samples[i] = m.seconds * 1e9 / (double)m.iterations;
    }
    for (size_t i = 1U; i < count; i++)
        for (size_t j = i; j > 0U && samples[j] < samples[j - 1U]; j--)
        { double t = samples[j]; samples[j] = samples[j - 1U]; samples[j - 1U] = t; }
    if (!measure(c, true, 0.0, &m, !validate)) return false;
    printf("%s,%s,%s,%zu,%zu,%zu,%.3f,%.3f,%.3f,%zu,%zu,%zu,%zu,%zu,%llu\n",
        c->name, c->op, c->mode, bigint_bit_length(c->a), bigint_bit_length(c->b), count,
        samples[0], samples[count / 2U], samples[count - 1U], m.calls, m.bytes,
        m.baseline, m.live, m.peak, numforge_profile_process_peak());
    return true;
}

int main(int argc, char **argv)
{
#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
    if (argc == 2 && !strcmp(argv[1], "--karatsuba-check"))
        return benchmark_karatsuba_fault_checks() ? 0 : 1;
    if (argc == 2 && !strcmp(argv[1], "--multiply-check"))
        return benchmark_multiplication_fault_checks() ? 0 : 1;
#endif
    bool quick = false, validate = false, success = true, found = false;
    bool check = false;
    const char *selected = NULL;
    if (argc < 2) { fputs("Usage: bigint_arithmetic_benchmark fixtures.tsv [--quick] [--validate] [--budget] [--check] [--case name]\n", stderr); return 1; }
    for (int i = 2; i < argc; i++)
    {
        if (!strcmp(argv[i], "--quick")) quick = true;
        else if (!strcmp(argv[i], "--budget")) enforce_application_budget = true;
        else if (!strcmp(argv[i], "--validate")) validate = true;
        else if (!strcmp(argv[i], "--check")) check = true;
        else if (!strcmp(argv[i], "--case") && i + 1 < argc) selected = argv[++i];
        else return 1;
    }
    if (enforce_application_budget) validate = true;
#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
    if (check && !check_experiment_failures()) return 1;
#else
    if (check) { fputs("--check requires BUILD_TESTING\n", stderr); return 1; }
#endif
    FILE *input = fopen(argv[1], "r");
    /* 100000! has 456574 decimal digits; allow the complete exact reference.
     * Reject truncated/overlong records rather than silently measuring them. */
    size_t capacity = 1048576U;
    char *line = malloc(capacity);
    if (input == NULL || line == NULL) { if (input) fclose(input); free(line); return 1; }
    puts("case,operation,mode,a_bits,b_bits,samples,min_ns_per_op,median_ns_per_op,max_ns_per_op,alloc_calls,requested_bytes,baseline_live_bytes,live_bytes,peak_live_bytes,process_peak_bytes");
    while (fgets(line, (int)capacity, input))
    {
        char *fields[7]; size_t n = 0U;
        if (strchr(line, '\n') == NULL) { success = false; break; }
        char *token = strtok(line, "\t\r\n");
        while (token && n < 7U) { fields[n++] = token; token = strtok(NULL, "\t\r\n"); }
        if (n != 7U || token) { success = false; break; }
        if (selected && strcmp(selected, fields[0])) continue;
        found = true;
        Case c = {fields[0], fields[1], fields[2], bigint_create(), bigint_create(), bigint_create(), bigint_create()};
        bool valid = c.a && c.b && c.expected && c.remainder;
        if (valid) valid = parse_fixture(c.a, fields[3]) == BIGINT_OK &&
            parse_fixture(c.b, fields[4]) == BIGINT_OK && parse_fixture(c.expected, fields[5]) == BIGINT_OK &&
            parse_fixture(c.remainder, fields[6]) == BIGINT_OK && run(&c, quick, validate);
        if (!valid) fprintf(stderr, "Failed exact reference or measurement: %s\n", c.name);
        bigint_destroy(c.a); bigint_destroy(c.b); bigint_destroy(c.expected); bigint_destroy(c.remainder);
        if (!valid) { success = false; break; }
    }
    if (ferror(input)) success = false;
    fclose(input); free(line);
    return success && found ? 0 : 1;
}
