#include <numforge/bigdecimal.h>
#include "bigdecimal_internal.h"
#include "../internal/numforge_alloc.h"

#include <stdint.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Built-in mathematical constants.

    Values are stored as 500-place decimal approximations. Precision-aware
    requests reuse and round these values through 500 significant digits;
    larger requests calculate the constant without binary floating point.
------------------------------------------------------------------------------------------------------------------------------
*/

#define BIGDECIMAL_STORED_CONSTANT_DIGITS 500
#define BIGDECIMAL_CONSTANT_GUARD_DIGITS 24
#define BIGDECIMAL_PI_MAX_ITERATIONS 128U

#define CONSTANT_TRY(operation)              \
    do                                       \
    {                                        \
        status = (operation);                \
        if (status != BIGDECIMAL_OK)         \
        {                                    \
            goto cleanup;                    \
        }                                    \
    } while (0)

static const char BIGDECIMAL_PI[] =
    "3.14159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706"
    "7982148086513282306647093844609550582231725359408128481117450284102701938521105559644622948954930381"
    "9644288109756659334461284756482337867831652712019091456485669234603486104543266482133936072602491412"
    "7372458700660631558817488152092096282925409171536436789259036001133053054882046652138414695194151160"
    "9433057270365759591953092186117381932611793105118548074462379962749567351885752724891227938183011949"
    "12";

static const char BIGDECIMAL_E[] =
    "2.71828182845904523536028747135266249775724709369995957496696762772407663035354759457138217852516642"
    "7427466391932003059921817413596629043572900334295260595630738132328627943490763233829880753195251019"
    "0115738341879307021540891499348841675092447614606680822648001684774118537423454424371075390777449920"
    "6955170276183860626133138458300075204493382656029760673711320070932870912744374704723069697720931014"
    "1692836819025515108657463772111252389784425056953696770785449969967946864454905987931636889230098793"
    "12";

static const char BIGDECIMAL_PHI[] =
    "1.61803398874989484820458683436563811772030917980576286213544862270526046281890244970720720418939113"
    "7484754088075386891752126633862223536931793180060766726354433389086595939582905638322661319928290267"
    "8806752087668925017116962070322210432162695486262963136144381497587012203408058879544547492461856953"
    "6486444924104432077134494704956584678850987433944221254487706647809158846074998871240076521705751797"
    "8834166256249407589069704000281210427621771117778053153171410117046665991466979873176135600670874807"
    "10";

static const char *bigdecimal_constant_text(BigDecimalConstant constant)
{
    switch (constant)
    {
        case BIGDECIMAL_CONSTANT_PI:
            return BIGDECIMAL_PI;
        case BIGDECIMAL_CONSTANT_E:
            return BIGDECIMAL_E;
        case BIGDECIMAL_CONSTANT_PHI:
            return BIGDECIMAL_PHI;
        default:
            return NULL;
    }
}

static BigDecimalStatus constant_binary_rounded(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    int64_t digits,
    bool subtract
)
{
    BigDecimal *temporary = bigdecimal_create();
    BigDecimalStatus status;

    if (temporary == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = subtract ? bigdecimal_sub(temporary, a, b)
                      : bigdecimal_add(temporary, a, b);

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_round_significant(
            result, temporary, digits, BIGDECIMAL_ROUND_HALF_EVEN);
    }

    bigdecimal_destroy(temporary);
    return status;
}

static BigDecimalStatus constant_multiply_rounded(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    int64_t digits
)
{
    BigDecimal *temporary = bigdecimal_create();
    BigDecimalStatus status;

    if (temporary == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_mul(temporary, a, b);

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_round_significant(
            result, temporary, digits, BIGDECIMAL_ROUND_HALF_EVEN);
    }

    bigdecimal_destroy(temporary);
    return status;
}

/* Gauss-Legendre doubles the number of correct digits per iteration. */
static BigDecimalStatus calculate_pi(
    BigDecimal *result,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *a = bigdecimal_create();
    BigDecimal *b = bigdecimal_create();
    BigDecimal *t = bigdecimal_create();
    BigDecimal *p = bigdecimal_create();
    BigDecimal *next_a = bigdecimal_create();
    BigDecimal *next_b = bigdecimal_create();
    BigDecimal *next_t = bigdecimal_create();
    BigDecimal *sum = bigdecimal_create();
    BigDecimal *difference = bigdecimal_create();
    BigDecimal *product = bigdecimal_create();
    BigDecimal *correction = bigdecimal_create();
    BigDecimal *numerator = bigdecimal_create();
    BigDecimal *denominator = bigdecimal_create();
    BigDecimal *one = bigdecimal_create();
    BigDecimal *two = bigdecimal_create();
    BigDecimal *four = bigdecimal_create();
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    int64_t work_digits;
    int comparison = 1;

    if (a == NULL || b == NULL || t == NULL || p == NULL || next_a == NULL ||
        next_b == NULL || next_t == NULL || sum == NULL || difference == NULL ||
        product == NULL || correction == NULL || numerator == NULL || denominator == NULL ||
        one == NULL || two == NULL || four == NULL)
    {
        goto cleanup;
    }

    if (!bigdecimal_i64_add(digits, BIGDECIMAL_CONSTANT_GUARD_DIGITS, &work_digits))
    {
        status = BIGDECIMAL_VALUE_TOO_LARGE;
        goto cleanup;
    }

    CONSTANT_TRY(bigdecimal_set_string(one, "1"));
    CONSTANT_TRY(bigdecimal_set_string(two, "2"));
    CONSTANT_TRY(bigdecimal_set_string(four, "4"));
    CONSTANT_TRY(bigdecimal_copy(a, one));
    CONSTANT_TRY(bigdecimal_set_string(t, "0.25"));
    CONSTANT_TRY(bigdecimal_copy(p, one));
    CONSTANT_TRY(bigdecimal_sqrt(b, two, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
    CONSTANT_TRY(bigdecimal_div_significant(
        b, one, b, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));

    for (unsigned iteration = 0U; iteration < BIGDECIMAL_PI_MAX_ITERATIONS; iteration++)
    {
        if (!numforge_budget_check())
        {
            status = BIGDECIMAL_OUT_OF_MEMORY;
            goto cleanup;
        }

        CONSTANT_TRY(constant_binary_rounded(sum, a, b, work_digits, false));
        CONSTANT_TRY(bigdecimal_div_exact_or_significant(
            next_a, sum, two, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
        CONSTANT_TRY(constant_multiply_rounded(product, a, b, work_digits));
        CONSTANT_TRY(bigdecimal_sqrt(
            next_b, product, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
        CONSTANT_TRY(constant_binary_rounded(
            difference, a, next_a, work_digits, true));
        CONSTANT_TRY(constant_multiply_rounded(
            correction, difference, difference, work_digits));
        CONSTANT_TRY(constant_multiply_rounded(
            correction, p, correction, work_digits));
        CONSTANT_TRY(constant_binary_rounded(
            next_t, t, correction, work_digits, true));
        CONSTANT_TRY(constant_binary_rounded(p, p, p, work_digits, false));
        CONSTANT_TRY(bigdecimal_compare(&comparison, next_a, a));
        CONSTANT_TRY(bigdecimal_copy(a, next_a));
        CONSTANT_TRY(bigdecimal_copy(b, next_b));
        CONSTANT_TRY(bigdecimal_copy(t, next_t));

        if (comparison == 0)
        {
            break;
        }
    }

    if (comparison != 0)
    {
        status = BIGDECIMAL_VALUE_TOO_LARGE;
        goto cleanup;
    }

    CONSTANT_TRY(constant_binary_rounded(sum, a, b, work_digits, false));
    CONSTANT_TRY(constant_multiply_rounded(
        numerator, sum, sum, work_digits));
    CONSTANT_TRY(constant_multiply_rounded(
        denominator, four, t, work_digits));
    CONSTANT_TRY(bigdecimal_div_significant(
        result, numerator, denominator, digits, rounding));

cleanup:
    bigdecimal_destroy(a);
    bigdecimal_destroy(b);
    bigdecimal_destroy(t);
    bigdecimal_destroy(p);
    bigdecimal_destroy(next_a);
    bigdecimal_destroy(next_b);
    bigdecimal_destroy(next_t);
    bigdecimal_destroy(sum);
    bigdecimal_destroy(difference);
    bigdecimal_destroy(product);
    bigdecimal_destroy(correction);
    bigdecimal_destroy(numerator);
    bigdecimal_destroy(denominator);
    bigdecimal_destroy(one);
    bigdecimal_destroy(two);
    bigdecimal_destroy(four);

    return status;
}

static BigDecimalStatus calculate_e(
    BigDecimal *result,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *one = bigdecimal_create();
    BigDecimalStatus status;

    if (one == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_set_string(one, "1");

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_exp(result, one, digits, rounding);
    }

    bigdecimal_destroy(one);
    return status;
}

static BigDecimalStatus calculate_phi(
    BigDecimal *result,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *one = bigdecimal_create();
    BigDecimal *two = bigdecimal_create();
    BigDecimal *five = bigdecimal_create();
    BigDecimal *root = bigdecimal_create();
    BigDecimal *sum = bigdecimal_create();
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    int64_t work_digits;

    if (one == NULL || two == NULL || five == NULL || root == NULL || sum == NULL)
    {
        goto cleanup;
    }

    if (!bigdecimal_i64_add(digits, BIGDECIMAL_CONSTANT_GUARD_DIGITS, &work_digits))
    {
        status = BIGDECIMAL_VALUE_TOO_LARGE;
        goto cleanup;
    }

    CONSTANT_TRY(bigdecimal_set_string(one, "1"));
    CONSTANT_TRY(bigdecimal_set_string(two, "2"));
    CONSTANT_TRY(bigdecimal_set_string(five, "5"));
    CONSTANT_TRY(bigdecimal_sqrt(
        root, five, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
    CONSTANT_TRY(constant_binary_rounded(
        sum, one, root, work_digits, false));
    CONSTANT_TRY(bigdecimal_div_significant(
        result, sum, two, digits, rounding));

cleanup:
    bigdecimal_destroy(one);
    bigdecimal_destroy(two);
    bigdecimal_destroy(five);
    bigdecimal_destroy(root);
    bigdecimal_destroy(sum);

    return status;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Public constant factory.
------------------------------------------------------------------------------------------------------------------------------
*/
BigDecimalStatus bigdecimal_set_constant(
    BigDecimal *result,
    BigDecimalConstant constant
)
{
    const char *text;

    if (result == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    text = bigdecimal_constant_text(constant);

    if (text == NULL)
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    return bigdecimal_set_string(result, text);
}

BigDecimalStatus bigdecimal_set_constant_significant(
    BigDecimal *result,
    BigDecimalConstant constant,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    const char *text;

    if (result == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    text = bigdecimal_constant_text(constant);

    if (text == NULL || digits < 1 || !bigdecimal_valid_rounding(rounding))
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    if (digits <= BIGDECIMAL_STORED_CONSTANT_DIGITS)
    {
        BigDecimal *stored = bigdecimal_create();
        BigDecimalStatus status;

        if (stored == NULL)
        {
            return BIGDECIMAL_OUT_OF_MEMORY;
        }

        status = bigdecimal_set_string(stored, text);

        if (status == BIGDECIMAL_OK)
        {
            status = bigdecimal_round_significant(result, stored, digits, rounding);
        }

        bigdecimal_destroy(stored);
        return status;
    }

    switch (constant)
    {
        case BIGDECIMAL_CONSTANT_PI:
            return calculate_pi(result, digits, rounding);
        case BIGDECIMAL_CONSTANT_E:
            return calculate_e(result, digits, rounding);
        case BIGDECIMAL_CONSTANT_PHI:
            return calculate_phi(result, digits, rounding);
        default:
            return BIGDECIMAL_INVALID_ARGUMENT;
    }
}

#undef CONSTANT_TRY
