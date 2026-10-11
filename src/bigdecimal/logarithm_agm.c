/* Private outward-rounded enclosure for the complete theta/AGM path.
 * All interval operations below have nonnegative operands, except subtraction.
 * They evaluate both endpoints before overwriting an input interval. */
#include "bigdecimal_internal.h"
#include "../internal/numforge_alloc.h"

typedef struct LnInterval { BigDecimal *lo, *hi; } LnInterval;
static bool li_zero(const BigDecimal *v) { return bigint_is_zero(v->coefficient); }
static bool li_negative(const BigDecimal *v) { return v->coefficient->is_negative; }
typedef struct LnIntervalContext {
    int64_t work;
    BigDecimalWorkspace workspace;
    BigDecimal *lo, *hi;
} LnIntervalContext;

static BigDecimalStatus li_publish(LnInterval *out, LnIntervalContext *c)
{
    BigDecimalStatus s = bigdecimal_copy(out->lo, c->lo);
    return s == BIGDECIMAL_OK ? bigdecimal_copy(out->hi, c->hi) : s;
}
static BigDecimalStatus li_set(LnInterval *out, const char *text)
{
    BigDecimalStatus s = bigdecimal_set_string(out->lo, text);
    return s == BIGDECIMAL_OK ? bigdecimal_copy(out->hi, out->lo) : s;
}
static BigDecimalStatus li_binary(LnInterval *out, const LnInterval *a,
    const LnInterval *b, char op, LnIntervalContext *c)
{
    BigDecimalStatus s;
    const BigDecimalRoundingMode down = BIGDECIMAL_ROUND_FLOOR;
    const BigDecimalRoundingMode up = BIGDECIMAL_ROUND_CEILING;
    if (op == '+') {
        s = bigdecimal_add_significant_workspace(c->lo, a->lo, b->lo, c->work, down, &c->workspace);
        if (s == BIGDECIMAL_OK) s = bigdecimal_add_significant_workspace(c->hi, a->hi, b->hi, c->work, up, &c->workspace);
    } else if (op == '*') {
        s = bigdecimal_mul_significant_workspace(c->lo, a->lo, b->lo, c->work, down, &c->workspace);
        if (s == BIGDECIMAL_OK) s = bigdecimal_mul_significant_workspace(c->hi, a->hi, b->hi, c->work, up, &c->workspace);
    } else if (op == '/') {
        if (li_zero(b->lo) || li_negative(b->lo)) return BIGDECIMAL_INVALID_ARGUMENT;
        s = bigdecimal_div_significant_workspace(c->lo, a->lo, b->hi, c->work, down, &c->workspace);
        if (s == BIGDECIMAL_OK) s = bigdecimal_div_significant_workspace(c->hi, a->hi, b->lo, c->work, up, &c->workspace);
    } else {
        s = bigdecimal_sub(c->lo, a->lo, b->hi);
        if (s == BIGDECIMAL_OK) s = bigdecimal_round_significant_workspace(c->lo, c->lo, c->work, down, &c->workspace);
        if (s == BIGDECIMAL_OK) s = bigdecimal_sub(c->hi, a->hi, b->lo);
        if (s == BIGDECIMAL_OK) s = bigdecimal_round_significant_workspace(c->hi, c->hi, c->work, up, &c->workspace);
    }
    return s == BIGDECIMAL_OK ? li_publish(out, c) : s;
}
static BigDecimalStatus li_sqrt(LnInterval *out, const LnInterval *a, LnIntervalContext *c)
{
    BigDecimalStatus s = bigdecimal_sqrt(c->lo, a->lo, c->work, BIGDECIMAL_ROUND_FLOOR);
    if (s == BIGDECIMAL_OK) s = bigdecimal_sqrt(c->hi, a->hi, c->work, BIGDECIMAL_ROUND_CEILING);
    return s == BIGDECIMAL_OK ? li_publish(out, c) : s;
}

/* Gauss-Legendre: pi = M^2 / t_inf. With d=a_n-b_n and p=2^n,
 * d_(n+j) <= d/2^j, hence the entire remaining correction is <= p*d^2/2.
 * Thus b_n^2/t_n <= pi <= a_n^2/(t_n-p*d^2/2).
 * Rounded endpoints enclose a, b, t and the tail, rather than assuming the
 * existing approximate public pi factory has a certified error bound. */
static BigDecimalStatus li_pi(LnInterval *out, LnIntervalContext *c, int64_t digits)
{
    enum { A, B, T, P, ONE, TWO, NA, NB, DELTA, CORRECTION, LOW_T, NUM, COUNT };
    LnInterval v[COUNT] = {{0}};
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    int comparison = 0;
#define PTRY(op) do { status = (op); if (status != BIGDECIMAL_OK) goto done; } while (0)
#define PB(out_,a_,b_,op_) PTRY(li_binary(&v[out_], &v[a_], &v[b_], op_, c))
    for (unsigned i = 0; i < COUNT; i++) {
        v[i].lo = bigdecimal_create(); v[i].hi = bigdecimal_create();
        if (!v[i].lo || !v[i].hi) goto done;
    }
    PTRY(li_set(&v[ONE], "1")); PTRY(li_set(&v[TWO], "2"));
    PTRY(li_set(&v[A], "1")); PTRY(li_set(&v[T], "0.25")); PTRY(li_set(&v[P], "1"));
    PTRY(li_sqrt(&v[B], &v[TWO], c)); PB(B, ONE, B, '/');
    for (unsigned iteration = 0; iteration < 128; iteration++) {
        if (!numforge_budget_check()) { status = BIGDECIMAL_OUT_OF_MEMORY; goto done; }
        PB(NA, A, B, '+'); PB(NA, NA, TWO, '/');
        PB(NB, A, B, '*'); PTRY(li_sqrt(&v[NB], &v[NB], c));
        PB(DELTA, A, NA, '-');
        /* The exact a-na is nonnegative; remove interval dependency artefacts. */
        if (li_negative(v[DELTA].lo)) PTRY(bigdecimal_set_string(v[DELTA].lo, "0"));
        PB(CORRECTION, DELTA, DELTA, '*'); PB(CORRECTION, CORRECTION, P, '*');
        PB(T, T, CORRECTION, '-'); PB(P, P, TWO, '*');
        { LnInterval swap = v[A]; v[A] = v[NA]; v[NA] = swap; }
        { LnInterval swap = v[B]; v[B] = v[NB]; v[NB] = swap; }
        PB(DELTA, A, B, '-');
        PTRY(li_set(&v[NUM], "1")); v[NUM].lo->scale = digits + 12;
        PTRY(bigdecimal_compare(&comparison, v[DELTA].hi, v[NUM].lo));
        if (comparison > 0) continue;
        if (li_negative(v[DELTA].lo)) PTRY(bigdecimal_set_string(v[DELTA].lo, "0"));
        PB(CORRECTION, DELTA, DELTA, '*'); PB(CORRECTION, CORRECTION, P, '*');
        PB(CORRECTION, CORRECTION, TWO, '/'); PB(LOW_T, T, CORRECTION, '-');
        if (li_negative(v[LOW_T].lo) || li_zero(v[LOW_T].lo)) {
            status = BIGDECIMAL_VALUE_TOO_LARGE; goto done;
        }
        PTRY(bigdecimal_mul_significant_workspace(c->lo, v[B].lo, v[B].lo, c->work,
            BIGDECIMAL_ROUND_FLOOR, &c->workspace));
        PTRY(bigdecimal_div_significant_workspace(out->lo, c->lo, v[T].hi, c->work,
            BIGDECIMAL_ROUND_FLOOR, &c->workspace));
        PTRY(bigdecimal_mul_significant_workspace(c->hi, v[A].hi, v[A].hi, c->work,
            BIGDECIMAL_ROUND_CEILING, &c->workspace));
        PTRY(bigdecimal_div_significant_workspace(out->hi, c->hi, v[LOW_T].lo, c->work,
            BIGDECIMAL_ROUND_CEILING, &c->workspace));
        status = BIGDECIMAL_OK; goto done;
    }
    status = BIGDECIMAL_VALUE_TOO_LARGE;
done:
    for (unsigned i = 0; i < COUNT; i++) { bigdecimal_destroy(v[i].lo); bigdecimal_destroy(v[i].hi); }
    return status;
#undef PB
#undef PTRY
}

static BigDecimalStatus interval_ln_attempt(BigDecimal *result, const BigDecimal *value,
    int64_t digits, int64_t extra, BigDecimalRoundingMode rounding,
    unsigned *expansions, unsigned *iterations, bool *resolved)
{
    enum { Y, ONE, TWO, THREE, FOUR, Q, Q2, Q4, Q8, Q9, Q16, Q25, T2, T3,
        TAIL, A, B, NA, NB, PRODUCT, PI, ANSWER, LIMIT, COUNT };
    LnInterval v[COUNT] = {{0}};
    LnIntervalContext c = {0};
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    int comparison = 0;
    bool negative = false;
    *resolved = false; *expansions = *iterations = 0;
    c.work = digits + extra;
    c.lo = bigdecimal_create(); c.hi = bigdecimal_create();
    if (!c.lo || !c.hi) goto done;
#define ITRY(op) do { status = (op); if (status != BIGDECIMAL_OK) goto done; } while (0)
#define IB(out_,a_,b_,op_) ITRY(li_binary(&v[out_], &v[a_], &v[b_], op_, &c))
    for (unsigned i = 0; i < COUNT; i++) {
        v[i].lo = bigdecimal_create(); v[i].hi = bigdecimal_create();
        if (!v[i].lo || !v[i].hi) goto done;
    }
    ITRY(li_set(&v[ONE], "1")); ITRY(li_set(&v[TWO], "2"));
    ITRY(li_set(&v[THREE], "3")); ITRY(li_set(&v[FOUR], "4"));
    ITRY(bigdecimal_compare(&comparison, value, v[ONE].lo));
    negative = comparison < 0;
    ITRY(bigdecimal_copy(v[Y].lo, value)); ITRY(bigdecimal_copy(v[Y].hi, value));
    if (negative) IB(Y, ONE, Y, '/');
    ITRY(li_set(&v[LIMIT], "1")); v[LIMIT].lo->scale = -((c.work + 4 + 35) / 36);
    for (;;) {
        if (!numforge_budget_check()) { status = BIGDECIMAL_OUT_OF_MEMORY; goto done; }
        ITRY(bigdecimal_compare(&comparison, v[Y].lo, v[LIMIT].lo));
        if (comparison >= 0) break;
        if (*expansions == 256) { status = BIGDECIMAL_VALUE_TOO_LARGE; goto done; }
        IB(Y, Y, Y, '*'); (*expansions)++;
    }
    IB(Q, ONE, Y, '/'); IB(Q2, Q, Q, '*'); IB(Q4, Q2, Q2, '*');
    IB(Q8, Q4, Q4, '*'); IB(Q9, Q8, Q, '*'); IB(Q16, Q8, Q8, '*'); IB(Q25, Q16, Q9, '*');
    IB(T2, Q, Q9, '+'); IB(T2, T2, Q25, '+'); IB(T2, T2, TWO, '*');
    IB(T3, Q4, Q16, '+'); IB(T3, T3, TWO, '*'); IB(T3, T3, ONE, '+');
    /* q <= 0.1: whole omitted tails are < 3*q^49 and < 3*q^36.
     * Consecutive omitted exponent gaps are >=32 (theta2) and >=28 (theta3). */
    IB(TAIL, Q25, Q16, '*'); IB(TAIL, TAIL, Q8, '*'); IB(TAIL, TAIL, THREE, '*');
    ITRY(bigdecimal_add_significant_workspace(v[T2].hi, v[T2].hi, v[TAIL].hi,
        c.work, BIGDECIMAL_ROUND_CEILING, &c.workspace));
    IB(TAIL, Q16, Q16, '*'); IB(TAIL, TAIL, Q4, '*'); IB(TAIL, TAIL, THREE, '*');
    ITRY(bigdecimal_add_significant_workspace(v[T3].hi, v[T3].hi, v[TAIL].hi,
        c.work, BIGDECIMAL_ROUND_CEILING, &c.workspace));
    IB(A, T2, T2, '*'); IB(B, T3, T3, '*');
    for (;;) {
        if (!numforge_budget_check()) { status = BIGDECIMAL_OUT_OF_MEMORY; goto done; }
        if (*iterations == 256) { status = BIGDECIMAL_VALUE_TOO_LARGE; goto done; }
        IB(NA, A, B, '+'); IB(NA, NA, TWO, '/');
        IB(PRODUCT, A, B, '*'); ITRY(li_sqrt(&v[NB], &v[PRODUCT], &c));
        (*iterations)++;
        { LnInterval swap = v[A]; v[A] = v[NA]; v[NA] = swap; }
        { LnInterval swap = v[B]; v[B] = v[NB]; v[NB] = swap; }
        /* This only chooses when to try final certification, not success.
         * After the first step, B.lo <= true AGM <= A.hi. */
        ITRY(bigdecimal_round_significant_workspace(c.lo, v[B].lo, c.work - 8,
            BIGDECIMAL_ROUND_HALF_EVEN, &c.workspace));
        ITRY(bigdecimal_round_significant_workspace(c.hi, v[A].hi, c.work - 8,
            BIGDECIMAL_ROUND_HALF_EVEN, &c.workspace));
        ITRY(bigdecimal_compare(&comparison, c.lo, c.hi));
        if (comparison == 0) break;
        /* Rounded interval endpoints eventually stagnate. Adapt precision
         * instead of treating equality of successive approximations as proof. */
        if (*iterations >= 64) { status = BIGDECIMAL_OK; goto done; }
    }
    ITRY(li_pi(&v[PI], &c, c.work - 16));
    ITRY(bigdecimal_copy(v[PRODUCT].lo, v[B].lo));
    ITRY(bigdecimal_copy(v[PRODUCT].hi, v[A].hi));
    IB(PRODUCT, PRODUCT, FOUR, '*');
    for (unsigned i = 0; i < *expansions; i++) IB(PRODUCT, PRODUCT, TWO, '*');
    IB(ANSWER, PI, PRODUCT, '/');
    if (negative) {
        ITRY(bigdecimal_negate(c.lo, v[ANSWER].hi)); ITRY(bigdecimal_negate(c.hi, v[ANSWER].lo));
        ITRY(li_publish(&v[ANSWER], &c));
    }
    ITRY(bigdecimal_round_significant_workspace(c.lo, v[ANSWER].lo, digits, rounding, &c.workspace));
    ITRY(bigdecimal_round_significant_workspace(c.hi, v[ANSWER].hi, digits, rounding, &c.workspace));
    ITRY(bigdecimal_compare(&comparison, c.lo, c.hi));
    if (comparison == 0) {
        if (!numforge_budget_check()) { status = BIGDECIMAL_OUT_OF_MEMORY; goto done; }
        ITRY(bigdecimal_copy(result, c.lo)); *resolved = true;
    }
    status = BIGDECIMAL_OK;
done:
    for (unsigned i = 0; i < COUNT; i++) { bigdecimal_destroy(v[i].lo); bigdecimal_destroy(v[i].hi); }
    bigdecimal_destroy(c.lo); bigdecimal_destroy(c.hi); bigdecimal_workspace_clear(&c.workspace);
    return status;
#undef IB
#undef ITRY
}

BigDecimalStatus bigdecimal_ln_agm_internal(BigDecimal *result, const BigDecimal *value,
    int64_t digits, int64_t extra, BigDecimalRoundingMode rounding,
    unsigned *expansions, unsigned *iterations, unsigned *attempts)
{
    BigDecimal *one = NULL;
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    int comparison, sign;
    *attempts = *expansions = *iterations = 0;
    if (result == NULL || value == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    if (digits < 1 || digits > 10000 || extra < 40 || extra > 512 || !bigdecimal_valid_rounding(rounding)) {
        status = BIGDECIMAL_INVALID_ARGUMENT; goto done;
    }
    status = bigdecimal_sign(&sign, value);
    if (status != BIGDECIMAL_OK) goto done;
    if (sign <= 0) { status = BIGDECIMAL_INVALID_ARGUMENT; goto done; }
    one = bigdecimal_create();
    if (!one) { status = BIGDECIMAL_OUT_OF_MEMORY; goto done; }
    status = bigdecimal_set_string(one, "1"); if (status != BIGDECIMAL_OK) goto done;
    status = bigdecimal_compare(&comparison, value, one); if (status != BIGDECIMAL_OK) goto done;
    if (comparison == 0) { status = bigdecimal_set_string(result, "0"); goto done; }
    /* Preserve the production series in its well-conditioned near-one band.
     * These fallback results are not claimed to have interval certification. */
    status = bigdecimal_set_string(one, "0.999"); if (status != BIGDECIMAL_OK) goto done;
    status = bigdecimal_compare(&comparison, value, one); if (status != BIGDECIMAL_OK) goto done;
    if (comparison >= 0) {
        status = bigdecimal_set_string(one, "1.001"); if (status != BIGDECIMAL_OK) goto done;
        status = bigdecimal_compare(&comparison, value, one); if (status != BIGDECIMAL_OK) goto done;
        if (comparison <= 0) { status = bigdecimal_ln_series_internal(result, value, digits, rounding); goto done; }
    }
    for (;;) {
        bool resolved;
        (*attempts)++;
        status = interval_ln_attempt(result, value, digits, extra, rounding, expansions, iterations, &resolved);
        if (status != BIGDECIMAL_OK || resolved) break;
        if (extra == 512) { status = BIGDECIMAL_VALUE_TOO_LARGE; break; }
        extra = extra > 256 ? 512 : extra * 2;
    }
done:
    bigdecimal_destroy(one); return status;
}
