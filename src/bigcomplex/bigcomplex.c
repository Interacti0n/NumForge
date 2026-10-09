#include <numforge/bigcomplex.h>
#include <numforge/runtime.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../bigdecimal/bigdecimal_internal.h"
#include "../bigint/bigint_internal.h"


struct BigComplex { BigDecimal *real, *imaginary; };

static BigComplexStatus mapped(BigDecimalStatus status)
{
    switch (status) {
        case BIGDECIMAL_OK: return BIGCOMPLEX_OK;
        case BIGDECIMAL_NULL_ARGUMENT: return BIGCOMPLEX_NULL_ARGUMENT;
        case BIGDECIMAL_OUT_OF_MEMORY: return BIGCOMPLEX_OUT_OF_MEMORY;
        case BIGDECIMAL_INVALID_ARGUMENT: return BIGCOMPLEX_INVALID_ARGUMENT;
        case BIGDECIMAL_DIVISION_BY_ZERO: return BIGCOMPLEX_DIVISION_BY_ZERO;
        case BIGDECIMAL_VALUE_TOO_LARGE: return BIGCOMPLEX_VALUE_TOO_LARGE;
        case BIGDECIMAL_SCALE_OVERFLOW: return BIGCOMPLEX_SCALE_OVERFLOW;
        default: return BIGCOMPLEX_INVALID_ARGUMENT;
    }
}
const char *bigcomplex_status_to_string(BigComplexStatus status)
{
    switch (status) {
        case BIGCOMPLEX_OK: return "success";
        case BIGCOMPLEX_NULL_ARGUMENT: return "null argument";
        case BIGCOMPLEX_OUT_OF_MEMORY: return "out of memory";
        case BIGCOMPLEX_INVALID_ARGUMENT: return "invalid argument";
        case BIGCOMPLEX_DIVISION_BY_ZERO: return "division by zero";
        case BIGCOMPLEX_VALUE_TOO_LARGE: return "value too large";
        case BIGCOMPLEX_SCALE_OVERFLOW: return "scale overflow";
        default: return "unknown status";
    }
}
BigComplex *bigcomplex_create(void)
{
    BigComplex *value = numforge_calloc(1, sizeof(*value));
    if (value == NULL) return NULL;
    value->real = bigdecimal_create(); value->imaginary = bigdecimal_create();
    if (value->real == NULL || value->imaginary == NULL) { bigcomplex_destroy(value); return NULL; }
    return value;
}
void bigcomplex_destroy(BigComplex *value)
{
    if (value == NULL) return;
    bigdecimal_destroy(value->real); bigdecimal_destroy(value->imaginary); free(value);
}
static void commit(BigComplex *result, BigComplex *temporary)
{
    BigComplex old = *result; *result = *temporary; *temporary = old;
}
#define TRY(call) do { status = (call); if (status != BIGDECIMAL_OK) goto done; } while (0)

BigComplexStatus bigcomplex_set_parts(BigComplex *result, const BigDecimal *real, const BigDecimal *imaginary)
{
    if (result == NULL || real == NULL || imaginary == NULL) return BIGCOMPLEX_NULL_ARGUMENT;
    BigComplex *temporary = bigcomplex_create();
    if (temporary == NULL) return BIGCOMPLEX_OUT_OF_MEMORY;
    BigDecimalStatus status;
    TRY(bigdecimal_copy(temporary->real, real)); TRY(bigdecimal_copy(temporary->imaginary, imaginary));
    commit(result, temporary);
done:
    bigcomplex_destroy(temporary); return mapped(status);
}
BigComplexStatus bigcomplex_set_strings(BigComplex *result, const char *real, const char *imaginary)
{
    if (result == NULL || real == NULL || imaginary == NULL) return BIGCOMPLEX_NULL_ARGUMENT;
    BigComplex *temporary = bigcomplex_create();
    if (temporary == NULL) return BIGCOMPLEX_OUT_OF_MEMORY;
    BigDecimalStatus status;
    TRY(bigdecimal_set_string(temporary->real, real)); TRY(bigdecimal_set_string(temporary->imaginary, imaginary));
    commit(result, temporary);
done:
    bigcomplex_destroy(temporary); return mapped(status);
}
BigComplexStatus bigcomplex_copy(BigComplex *result, const BigComplex *value)
{
    if (value == NULL) return BIGCOMPLEX_NULL_ARGUMENT;
    return bigcomplex_set_parts(result, value->real, value->imaginary);
}
BigComplexStatus bigcomplex_from_bigdecimal(BigComplex *result, const BigDecimal *value)
{
    if (result == NULL || value == NULL) return BIGCOMPLEX_NULL_ARGUMENT;
    BigComplex *temporary = bigcomplex_create();
    if (temporary == NULL) return BIGCOMPLEX_OUT_OF_MEMORY;
    BigDecimalStatus status = bigdecimal_copy(temporary->real, value);
    if (status == BIGDECIMAL_OK) commit(result, temporary);
    bigcomplex_destroy(temporary); return mapped(status);
}
BigComplexStatus bigcomplex_get_real(BigDecimal *result, const BigComplex *value)
{ return value == NULL ? BIGCOMPLEX_NULL_ARGUMENT : mapped(bigdecimal_copy(result, value->real)); }
BigComplexStatus bigcomplex_get_imaginary(BigDecimal *result, const BigComplex *value)
{ return value == NULL ? BIGCOMPLEX_NULL_ARGUMENT : mapped(bigdecimal_copy(result, value->imaginary)); }
BigComplexStatus bigcomplex_is_zero(bool *result, const BigComplex *value)
{
    if (result == NULL || value == NULL) return BIGCOMPLEX_NULL_ARGUMENT;
    bool real = false, imaginary = false;
    BigDecimalStatus status = bigdecimal_is_zero(&real, value->real);
    if (status == BIGDECIMAL_OK) status = bigdecimal_is_zero(&imaginary, value->imaginary);
    if (status == BIGDECIMAL_OK) *result = real && imaginary;
    return mapped(status);
}
BigComplexStatus bigcomplex_equal(bool *result, const BigComplex *a, const BigComplex *b)
{
    if (result == NULL || a == NULL || b == NULL) return BIGCOMPLEX_NULL_ARGUMENT;
    int real = 0, imaginary = 0;
    BigDecimalStatus status = bigdecimal_compare(&real, a->real, b->real);
    if (status == BIGDECIMAL_OK) status = bigdecimal_compare(&imaginary, a->imaginary, b->imaginary);
    if (status == BIGDECIMAL_OK) *result = real == 0 && imaginary == 0;
    return mapped(status);
}
static BigComplexStatus unary(BigComplex *result, const BigComplex *value, bool conjugate)
{
    if (result == NULL || value == NULL) return BIGCOMPLEX_NULL_ARGUMENT;
    BigComplex *temporary = bigcomplex_create();
    if (temporary == NULL) return BIGCOMPLEX_OUT_OF_MEMORY;
    BigDecimalStatus status;
    TRY(conjugate ? bigdecimal_copy(temporary->real, value->real) : bigdecimal_negate(temporary->real, value->real));
    TRY(bigdecimal_negate(temporary->imaginary, value->imaginary));
    commit(result, temporary);
done:
    bigcomplex_destroy(temporary); return mapped(status);
}
BigComplexStatus bigcomplex_negate(BigComplex *result, const BigComplex *value) { return unary(result, value, false); }
BigComplexStatus bigcomplex_conjugate(BigComplex *result, const BigComplex *value) { return unary(result, value, true); }

/* Factor a common decimal power without rounding coefficients. Ignoring zero
 * components prevents their canonical scale zero from selecting the factor. */
static int64_t common_scale(const BigComplex *value)
{
    bool re_zero=bigint_is_zero(value->real->coefficient),im_zero=bigint_is_zero(value->imaginary->coefficient);
    if(re_zero) return im_zero ? 0 : value->imaginary->scale;
    if(im_zero) return value->real->scale;
    return value->real->scale<value->imaginary->scale ? value->real->scale : value->imaginary->scale;
}
static BigComplexStatus copy_scaled(BigComplex *result,const BigComplex *value,int64_t scale)
{
    BigComplexStatus status=bigcomplex_copy(result,value);
    if(status!=BIGCOMPLEX_OK) return status;
    BigDecimal *parts[]={result->real,result->imaginary};
    for(size_t i=0;i<2;i++) if(!bigint_is_zero(parts[i]->coefficient)) {
        int64_t adjusted;
        if(!bigdecimal_i64_sub(parts[i]->scale,scale,&adjusted)) return BIGCOMPLEX_SCALE_OVERFLOW;
        parts[i]->scale=adjusted;
    }
    return BIGCOMPLEX_OK;
}

static BigComplexStatus binary(BigComplex *result, const BigComplex *a, const BigComplex *b,
    int operation, int64_t digits, BigDecimalRoundingMode rounding)
{
    if (result == NULL || a == NULL || b == NULL) return BIGCOMPLEX_NULL_ARGUMENT;
    if (operation == 3 && (digits < 1 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO || rounding > BIGDECIMAL_ROUND_HALF_EVEN))
        return BIGCOMPLEX_INVALID_ARGUMENT;
    if (operation == 3) {
        bool zero; BigComplexStatus check = bigcomplex_is_zero(&zero, b);
        if (check != BIGCOMPLEX_OK) return check;
        if (zero) return BIGCOMPLEX_DIVISION_BY_ZERO;
        int64_t scale=common_scale(b);
        if(scale!=0) {
            BigComplex *scaled_a=bigcomplex_create(),*scaled_b=bigcomplex_create();
            check=BIGCOMPLEX_OUT_OF_MEMORY;
            if(scaled_a && scaled_b) {
                check=copy_scaled(scaled_a,a,scale);
                if(check==BIGCOMPLEX_OK) check=copy_scaled(scaled_b,b,scale);
                if(check==BIGCOMPLEX_OK) check=binary(result,scaled_a,scaled_b,operation,digits,rounding);
            }
            bigcomplex_destroy(scaled_a);bigcomplex_destroy(scaled_b);return check;
        }
    }
    BigComplex *temporary = bigcomplex_create();
    BigDecimal *x = NULL, *y = NULL, *denominator = NULL;
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    if (temporary == NULL) goto done;
    if (operation < 2) {
        TRY(operation == 0 ? bigdecimal_add(temporary->real, a->real, b->real) : bigdecimal_sub(temporary->real, a->real, b->real));
        TRY(operation == 0 ? bigdecimal_add(temporary->imaginary, a->imaginary, b->imaginary) : bigdecimal_sub(temporary->imaginary, a->imaginary, b->imaginary));
    } else {
        x = bigdecimal_create(); y = bigdecimal_create();
        if (x == NULL || y == NULL) { status = BIGDECIMAL_OUT_OF_MEMORY; goto done; }
        TRY(bigdecimal_mul(x, a->real, b->real)); TRY(bigdecimal_mul(y, a->imaginary, b->imaginary));
        TRY(operation == 2 ? bigdecimal_sub(temporary->real, x, y) : bigdecimal_add(temporary->real, x, y));
        TRY(bigdecimal_mul(x, a->imaginary, b->real)); TRY(bigdecimal_mul(y, a->real, b->imaginary));
        TRY(operation == 2 ? bigdecimal_add(temporary->imaginary, x, y) : bigdecimal_sub(temporary->imaginary, x, y));
        if (operation == 3) {
            denominator = bigdecimal_create();
            if (denominator == NULL) { status = BIGDECIMAL_OUT_OF_MEMORY; goto done; }
            TRY(bigdecimal_mul(x, b->real, b->real)); TRY(bigdecimal_mul(y, b->imaginary, b->imaginary));
            TRY(bigdecimal_add(denominator, x, y));
            TRY(bigdecimal_div_exact_or_significant(temporary->real, temporary->real, denominator, digits, rounding));
            TRY(bigdecimal_div_exact_or_significant(temporary->imaginary, temporary->imaginary, denominator, digits, rounding));
        }
    }
    commit(result, temporary);
done:
    bigdecimal_destroy(x); bigdecimal_destroy(y); bigdecimal_destroy(denominator);
    bigcomplex_destroy(temporary); return mapped(status);
}
BigComplexStatus bigcomplex_add(BigComplex *result, const BigComplex *a, const BigComplex *b) { return binary(result,a,b,0,0,BIGDECIMAL_ROUND_HALF_EVEN); }
BigComplexStatus bigcomplex_sub(BigComplex *result, const BigComplex *a, const BigComplex *b) { return binary(result,a,b,1,0,BIGDECIMAL_ROUND_HALF_EVEN); }
BigComplexStatus bigcomplex_mul(BigComplex *result, const BigComplex *a, const BigComplex *b) { return binary(result,a,b,2,0,BIGDECIMAL_ROUND_HALF_EVEN); }
BigComplexStatus bigcomplex_div(BigComplex *result, const BigComplex *a, const BigComplex *b, int64_t digits, BigDecimalRoundingMode rounding) { return binary(result,a,b,3,digits,rounding); }
BigComplexStatus bigcomplex_abs_squared(BigDecimal *result, const BigComplex *value)
{
    if (result == NULL || value == NULL) return BIGCOMPLEX_NULL_ARGUMENT;
    BigDecimal *x = bigdecimal_create(), *y = bigdecimal_create();
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    if (x == NULL || y == NULL) goto done;
    TRY(bigdecimal_mul(x, value->real, value->real)); TRY(bigdecimal_mul(y, value->imaginary, value->imaginary));
    TRY(bigdecimal_add(result, x, y));
done:
    bigdecimal_destroy(x); bigdecimal_destroy(y); return mapped(status);
}
static unsigned coefficient_modulo(const BigInt *value,unsigned prime)
{
    unsigned result=0,base=(unsigned)((UINT64_MAX%prime+1)%prime);
    for(size_t i=value->size;i>0;i--) result=(unsigned)(((uint64_t)result*base+value->limbs[i-1]%prime)%prime);
    return result;
}
static unsigned power10_modulo(uint64_t exponent,unsigned prime)
{
    unsigned result=1,factor=10%prime;
    while(exponent) {
        if(exponent&1) result=result*factor%prime;
        exponent>>=1;if(exponent) factor=factor*factor%prime;
    }
    return result;
}
/* A modular nonsquare witness proves that the modulus cannot be a finite
 * decimal. Otherwise retain the exact-square/root path and its guarantees. */
static bool modulus_is_nonsquare(const BigComplex *value)
{
    const unsigned primes[]={3,7,11,13,17,19};
    int64_t scale=value->real->scale>value->imaginary->scale ? value->real->scale : value->imaginary->scale;
    uint64_t re_shift=0,im_shift=0;
    if(bigdecimal_scale_difference(scale,value->real->scale,&re_shift)!=BIGDECIMAL_OK ||
        bigdecimal_scale_difference(scale,value->imaginary->scale,&im_shift)!=BIGDECIMAL_OK) return false;
    for(size_t i=0;i<sizeof(primes)/sizeof(primes[0]);i++) {
        unsigned p=primes[i],a=coefficient_modulo(value->real->coefficient,p)*power10_modulo(re_shift,p)%p;
        unsigned b=coefficient_modulo(value->imaginary->coefficient,p)*power10_modulo(im_shift,p)%p;
        unsigned sum=(a*a+b*b)%p;bool square=false;
        for(unsigned r=0;r<p;r++) if(r*r%p==sum) {square=true;break;}
        if(!square) return true;
    }
    return false;
}
static BigComplexStatus try_small_hypot(BigDecimal *result,const BigComplex *value,
    int64_t digits,BigDecimalRoundingMode rounding,bool *handled)
{
    *handled=false;
    if(bigint_is_zero(value->real->coefficient) || bigint_is_zero(value->imaginary->coefficient)) return BIGCOMPLEX_OK;
    int64_t high=value->real->scale>value->imaginary->scale ? value->real->scale : value->imaginary->scale;
    int64_t low=value->real->scale<value->imaginary->scale ? value->real->scale : value->imaginary->scale;
    uint64_t spread=0;
    if(bigdecimal_scale_difference(high,low,&spread)!=BIGDECIMAL_OK || spread<64) return BIGCOMPLEX_OK;
    BigDecimal *a=bigdecimal_create(),*b=bigdecimal_create(),*epsilon=bigdecimal_create(),*perturbed=bigdecimal_create();
    char *a_text=NULL,*b_text=NULL;BigDecimalStatus status=BIGDECIMAL_OUT_OF_MEMORY;int comparison=0;
    if(!a || !b || !epsilon || !perturbed) goto done;
    TRY(bigdecimal_abs(a,value->real));TRY(bigdecimal_abs(b,value->imaginary));
    TRY(bigdecimal_compare(&comparison,a,b));
    if(comparison<0) {BigDecimal *swap=a;a=b;b=swap;}
    a_text=bigint_to_string(a->coefficient);b_text=bigint_to_string(b->coefficient);
    if(!a_text || !b_text) {status=BIGDECIMAL_OUT_OF_MEMORY;goto done;}
    size_t a_digits=strlen(a_text),b_digits=strlen(b_text);
    uint64_t gap=0,difference=0;
    if(b->scale>=a->scale) {
        TRY(bigdecimal_scale_difference(b->scale,a->scale,&difference));
        if(a_digits>=b_digits) gap=difference>UINT64_MAX-(a_digits-b_digits) ? UINT64_MAX : difference+(a_digits-b_digits);
        else gap=difference>=b_digits-a_digits ? difference-(b_digits-a_digits) : 0;
    } else {
        TRY(bigdecimal_scale_difference(a->scale,b->scale,&difference));
        gap=a_digits>=b_digits && a_digits-b_digits>=difference ? a_digits-b_digits-difference : 0;
    }
    if(digits>INT64_MAX-4) {status=BIGDECIMAL_VALUE_TOO_LARGE;goto done;}
#if SIZE_MAX > INT64_MAX
    if(a_digits>(uint64_t)(INT64_MAX-4)) {status=BIGDECIMAL_VALUE_TOO_LARGE;goto done;}
#endif
    int64_t work=(int64_t)a_digits>digits ? (int64_t)a_digits+2 : digits+2;
    status=BIGDECIMAL_OK;
    if(gap<(uint64_t)work/2+2 || !modulus_is_nonsquare(value)) goto done;
    *handled=true;
    /* 0<hypot(a,b)-a<b^2/(2a)<epsilon. epsilon is two decimal positions
     * below both a's final stored digit and the requested rounding grid.
     * No rounding boundary lies between the two positive perturbations. */
    TRY(bigdecimal_set_string(epsilon,"1"));
    if(!bigdecimal_i64_add(a->scale,work-(int64_t)a_digits,&epsilon->scale)) {status=BIGDECIMAL_SCALE_OVERFLOW;goto done;}
    TRY(bigdecimal_add(perturbed,a,epsilon));
    TRY(bigdecimal_round_significant(result,perturbed,digits,rounding));
done:
    free(a_text);free(b_text);bigdecimal_destroy(a);bigdecimal_destroy(b);
    bigdecimal_destroy(epsilon);bigdecimal_destroy(perturbed);return mapped(status);
}
BigComplexStatus bigcomplex_abs(BigDecimal *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding)
{
    if (result == NULL || value == NULL) return BIGCOMPLEX_NULL_ARGUMENT;
    if (digits < 1 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO || rounding > BIGDECIMAL_ROUND_HALF_EVEN)
        return BIGCOMPLEX_INVALID_ARGUMENT;
    bool handled=false;BigComplexStatus check=try_small_hypot(result,value,digits,rounding,&handled);
    if(check!=BIGCOMPLEX_OK || handled) return check;
    int64_t scale=common_scale(value);
    if(scale!=0) {
        BigComplex *normalized=bigcomplex_create();BigDecimal *magnitude=bigdecimal_create();
        BigComplexStatus status=BIGCOMPLEX_OUT_OF_MEMORY;
        if(normalized && magnitude) {
            status=copy_scaled(normalized,value,scale);
            if(status==BIGCOMPLEX_OK) status=bigcomplex_abs(magnitude,normalized,digits,rounding);
            if(status==BIGCOMPLEX_OK && !bigint_is_zero(magnitude->coefficient) &&
                !bigdecimal_i64_add(magnitude->scale,scale,&magnitude->scale)) status=BIGCOMPLEX_SCALE_OVERFLOW;
            if(status==BIGCOMPLEX_OK) {bigdecimal_commit(result,magnitude);magnitude=NULL;}
        }
        bigcomplex_destroy(normalized);bigdecimal_destroy(magnitude);return status;
    }
    BigDecimal *squared = bigdecimal_create();
    if (squared == NULL) return BIGCOMPLEX_OUT_OF_MEMORY;
    BigComplexStatus status = bigcomplex_abs_squared(squared, value);
    if (status == BIGCOMPLEX_OK) status = mapped(bigdecimal_sqrt(result, squared, digits, rounding));
    bigdecimal_destroy(squared);
    return status;
}
BigComplexStatus bigcomplex_sqrt(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding)
{
    if (!result || !value) return BIGCOMPLEX_NULL_ARGUMENT;
    if (digits < 1 || digits > INT64_MAX-12 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO ||
        rounding > BIGDECIMAL_ROUND_HALF_EVEN) return BIGCOMPLEX_INVALID_ARGUMENT;
    BigComplex *temporary=bigcomplex_create(), *square=bigcomplex_create();
    BigDecimal *radius=bigdecimal_create(),*magnitude=bigdecimal_create(),*two=bigdecimal_create();
    BigDecimalStatus status=BIGDECIMAL_OUT_OF_MEMORY;BigComplexStatus complex_status=BIGCOMPLEX_OK;
    bool imaginary_zero=false,real_negative=false,imaginary_negative=false,equal=false;
    if (!temporary || !square || !radius || !magnitude || !two) goto done;
    TRY(bigdecimal_is_zero(&imaginary_zero,value->imaginary));
    TRY(bigdecimal_is_negative(&real_negative,value->real));
    TRY(bigdecimal_is_negative(&imaginary_negative,value->imaginary));
    TRY(bigdecimal_abs(magnitude,value->real));
    TRY(bigdecimal_set_string(two,"2"));
    if (imaginary_zero) {
        TRY(bigdecimal_sqrt(real_negative ? temporary->imaginary : temporary->real,
            magnitude,digits+12,rounding));
    } else {
        complex_status=bigcomplex_abs(radius,value,digits+12,rounding);
        if (complex_status != BIGCOMPLEX_OK) goto done;
        TRY(bigdecimal_add(magnitude,radius,magnitude));
        TRY(bigdecimal_div_exact_or_significant(magnitude,magnitude,two,digits+12,rounding));
        TRY(bigdecimal_sqrt(magnitude,magnitude,digits+12,rounding));
        TRY(bigdecimal_mul(radius,two,magnitude));
        if (!real_negative) {
            TRY(bigdecimal_copy(temporary->real,magnitude));
            TRY(bigdecimal_div_exact_or_significant(temporary->imaginary,value->imaginary,radius,digits+12,rounding));
        } else {
            TRY(bigdecimal_abs(temporary->real,value->imaginary));
            TRY(bigdecimal_div_exact_or_significant(temporary->real,temporary->real,radius,digits+12,rounding));
            TRY(imaginary_negative ? bigdecimal_negate(temporary->imaginary,magnitude) :
                bigdecimal_copy(temporary->imaginary,magnitude));
        }
    }
    /* Prove exactness on stored finite decimals before applying output precision. */
    int64_t proof_scale=common_scale(temporary);
    if(proof_scale!=0) {
        BigComplex *proof_root=bigcomplex_create(),*proof_value=bigcomplex_create();
        complex_status=BIGCOMPLEX_OUT_OF_MEMORY;
        if(proof_root && proof_value) {
            complex_status=copy_scaled(proof_root,temporary,proof_scale);
            if(complex_status==BIGCOMPLEX_OK) complex_status=copy_scaled(proof_value,value,proof_scale);
            if(complex_status==BIGCOMPLEX_OK) complex_status=copy_scaled(proof_value,proof_value,proof_scale);
            if(complex_status==BIGCOMPLEX_OK) complex_status=bigcomplex_mul(square,proof_root,proof_root);
            if(complex_status==BIGCOMPLEX_OK) complex_status=bigcomplex_equal(&equal,square,proof_value);
        }
        bigcomplex_destroy(proof_root);bigcomplex_destroy(proof_value);
    } else {
        complex_status=bigcomplex_mul(square,temporary,temporary);
        if (complex_status == BIGCOMPLEX_OK) complex_status=bigcomplex_equal(&equal,square,value);
    }
    if (complex_status != BIGCOMPLEX_OK) goto done;
    if (!equal) {
        TRY(bigdecimal_set_string(two,"1"));
        TRY(bigdecimal_div_significant(temporary->real,temporary->real,two,digits,rounding));
        TRY(bigdecimal_div_significant(temporary->imaginary,temporary->imaginary,two,digits,rounding));
    }
    commit(result,temporary);
done:
    bigcomplex_destroy(temporary);bigcomplex_destroy(square);
    bigdecimal_destroy(radius);bigdecimal_destroy(magnitude);bigdecimal_destroy(two);
    return complex_status != BIGCOMPLEX_OK ? complex_status : mapped(status);
}
BigComplexStatus bigcomplex_pow_int(BigComplex *result, const BigComplex *value,
    int64_t exponent, int64_t digits, BigDecimalRoundingMode rounding)
{
    if (result == NULL || value == NULL) return BIGCOMPLEX_NULL_ARGUMENT;
    if (digits < 1 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO || rounding > BIGDECIMAL_ROUND_HALF_EVEN)
        return BIGCOMPLEX_INVALID_ARGUMENT;
    if (exponent < 0) {
        bool zero = false;
        BigComplexStatus check = bigcomplex_is_zero(&zero, value);
        if (check != BIGCOMPLEX_OK) return check;
        if (zero) return BIGCOMPLEX_DIVISION_BY_ZERO;
    }
    BigComplex *power = bigcomplex_create(), *base = bigcomplex_create(), *one = NULL;
    BigComplexStatus status = BIGCOMPLEX_OUT_OF_MEMORY;
    if (power == NULL || base == NULL) goto done;
    status = bigcomplex_set_strings(power, "1", "0");
    if (status != BIGCOMPLEX_OK) goto done;
    status = bigcomplex_copy(base, value);
    if (status != BIGCOMPLEX_OK) goto done;
    /* Unsigned subtraction avoids signed overflow for INT64_MIN. */
    uint64_t remaining = exponent < 0 ? UINT64_C(0) - (uint64_t)exponent : (uint64_t)exponent;
    while (remaining != 0) {
        if ((remaining & UINT64_C(1)) != 0) {
            status = bigcomplex_mul(power, power, base);
            if (status != BIGCOMPLEX_OK) goto done;
        }
        remaining >>= 1;
        if (remaining != 0) {
            status = bigcomplex_mul(base, base, base);
            if (status != BIGCOMPLEX_OK) goto done;
        }
    }
    if (exponent < 0) {
        one = bigcomplex_create();
        if (one == NULL) { status = BIGCOMPLEX_OUT_OF_MEMORY; goto done; }
        status = bigcomplex_set_strings(one, "1", "0");
        if (status != BIGCOMPLEX_OK) goto done;
        status = bigcomplex_div(power, one, power, digits, rounding);
        if (status != BIGCOMPLEX_OK) goto done;
    }
    commit(result, power);
done:
    bigcomplex_destroy(power); bigcomplex_destroy(base); bigcomplex_destroy(one);
    return status;
}
static BigComplexStatus compose(const char *real, const char *imaginary,
    bool mathematical, size_t limit, char **result)
{
    const char *magnitude = imaginary[0] == '-' ? imaginary + 1 : imaginary;
    const char *coefficient = strcmp(magnitude,"1") == 0 ? "" : magnitude;
    bool real_zero = strcmp(real,"0") == 0, imaginary_zero = strcmp(imaginary,"0") == 0;
    bool parentheses = mathematical && *coefficient != '\0' && strchr(coefficient,' ') != NULL;
    size_t a = strlen(real), b = strlen(coefficient), length;
    if (a > SIZE_MAX - 10 || b > SIZE_MAX - 10 - a) return BIGCOMPLEX_VALUE_TOO_LARGE;
    if (imaginary_zero) length = a;
    else {
        size_t term = b + (*coefficient ? 2U : 1U) + (parentheses ? 2U : 0U);
        length = real_zero ? term + (imaginary[0]=='-' ? 1U : 0U) : a + 3U + term;
    }
    if (length > limit) return BIGCOMPLEX_VALUE_TOO_LARGE;
    size_t capacity = length + 1U;
    char *text = numforge_malloc(capacity);
    if (text == NULL) return BIGCOMPLEX_OUT_OF_MEMORY;
    const char *open = parentheses ? "(" : "", *close = parentheses ? ")" : "";
    if (imaginary_zero) (void)snprintf(text,capacity,"%s",real);
    else if (real_zero) (void)snprintf(text,capacity,"%s%s%s%s%si",imaginary[0]=='-'?"-":"",open,coefficient,close,*coefficient?"*":"");
    else (void)snprintf(text,capacity,"%s %s %s%s%s%si",real,imaginary[0]=='-'?"-":"+",open,coefficient,close,*coefficient?"*":"");
    *result = text;
    return BIGCOMPLEX_OK;
}
BigComplexStatus bigcomplex_to_string(const BigComplex *value, char **result)
{
    if (value == NULL || result == NULL) return BIGCOMPLEX_NULL_ARGUMENT;
    char *real = NULL, *imaginary = NULL;
    BigDecimalStatus status;
    TRY(bigdecimal_to_string(value->real, &real)); TRY(bigdecimal_to_string(value->imaginary, &imaginary));
    {
        BigComplexStatus combined = compose(real, imaginary, false, SIZE_MAX, result);
        free(real); free(imaginary); return combined;
    }
done:
    free(real); free(imaginary); return mapped(status);
}
BigComplexStatus bigcomplex_format(const BigComplex *value, int64_t places,
    BigDecimalRoundingMode rounding, BigDecimalFormatMode mode, size_t max_output_bytes, char **result)
{
    if (value == NULL || result == NULL) return BIGCOMPLEX_NULL_ARGUMENT;
    if (places < -1 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO || rounding > BIGDECIMAL_ROUND_HALF_EVEN ||
        mode < BIGDECIMAL_FORMAT_AUTO || mode > BIGDECIMAL_FORMAT_MATHEMATICAL)
        return BIGCOMPLEX_INVALID_ARGUMENT;
    char *real = NULL, *imaginary = NULL;
    BigDecimalStatus status;
    /* Round the signed value first: abs-before-rounding would reverse floor/ceil. */
    TRY(bigdecimal_format_mode(value->real, places, rounding, mode, max_output_bytes, &real));
    TRY(bigdecimal_format_mode(value->imaginary, places, rounding, mode, max_output_bytes, &imaginary));
    {
        BigComplexStatus combined = compose(real, imaginary, mode == BIGDECIMAL_FORMAT_MATHEMATICAL, max_output_bytes, result);
        free(real); free(imaginary); return combined;
    }
done:
    free(real); free(imaginary); return mapped(status);
}
BigComplexStatus bigcomplex_arg(BigDecimal *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding)
{
    if (!result || !value) return BIGCOMPLEX_NULL_ARGUMENT;
    if (digits < 1 || digits > INT64_MAX-12 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO || rounding > BIGDECIMAL_ROUND_HALF_EVEN)
        return BIGCOMPLEX_INVALID_ARGUMENT;
    bool rz=false, iz=false, rn=false, in=false;
    BigDecimalStatus status;
    BigDecimal *angle=NULL,*pi=NULL,*one=NULL,*ratio=NULL;
    TRY(bigdecimal_is_zero(&rz,value->real));TRY(bigdecimal_is_zero(&iz,value->imaginary));
    if(rz && iz) return BIGCOMPLEX_INVALID_ARGUMENT;
    TRY(bigdecimal_is_negative(&rn,value->real));TRY(bigdecimal_is_negative(&in,value->imaginary));
    angle=bigdecimal_create();pi=bigdecimal_create();one=bigdecimal_create();ratio=bigdecimal_create();
    if(!angle || !pi || !one || !ratio){status=BIGDECIMAL_OUT_OF_MEMORY;goto done;}
    TRY(bigdecimal_set_string(one,"1"));
    if(rz || rn){TRY(bigdecimal_set_constant_significant(pi,BIGDECIMAL_CONSTANT_PI,digits+12,BIGDECIMAL_ROUND_HALF_EVEN));}
    if(rz){
        TRY(bigdecimal_set_string(ratio,"2"));TRY(bigdecimal_div_exact_or_significant(angle,pi,ratio,digits+12,BIGDECIMAL_ROUND_HALF_EVEN));
        if(in){TRY(bigdecimal_negate(angle,angle));}
    }else{
        TRY(bigdecimal_div_exact_or_significant(ratio,value->imaginary,value->real,digits+12,BIGDECIMAL_ROUND_HALF_EVEN));
        TRY(bigdecimal_atan(angle,ratio,digits+12,BIGDECIMAL_ROUND_HALF_EVEN));
        if(rn){TRY(in?bigdecimal_sub(angle,angle,pi):bigdecimal_add(angle,angle,pi));}
    }
    TRY(bigdecimal_div_significant(result,angle,one,digits,rounding));
done:bigdecimal_destroy(angle);bigdecimal_destroy(pi);bigdecimal_destroy(one);bigdecimal_destroy(ratio);return mapped(status);
}
BigComplexStatus bigcomplex_format_form(const BigComplex *value, BigComplexForm form,
    int64_t digits, int64_t places, BigDecimalRoundingMode rounding,
    BigDecimalFormatMode mode, size_t limit, char **result)
{
    if(!value || !result)return BIGCOMPLEX_NULL_ARGUMENT;
    if(form<BIGCOMPLEX_FORM_CARTESIAN || form>BIGCOMPLEX_FORM_EXPONENTIAL || digits<1 || digits>INT64_MAX-12 ||
        places < -1 || rounding<BIGDECIMAL_ROUND_TOWARD_ZERO || rounding>BIGDECIMAL_ROUND_HALF_EVEN ||
        mode<BIGDECIMAL_FORMAT_AUTO || mode>BIGDECIMAL_FORMAT_MATHEMATICAL)return BIGCOMPLEX_INVALID_ARGUMENT;
    if(form==BIGCOMPLEX_FORM_CARTESIAN)return bigcomplex_format(value,places,rounding,mode,limit,result);
    bool zero=false;BigComplexStatus status=bigcomplex_is_zero(&zero,value);
    if(status!=BIGCOMPLEX_OK)return status;
    if(zero)return bigcomplex_format(value,places,rounding,mode,limit,result);
    BigDecimal *radius=bigdecimal_create(),*angle=bigdecimal_create();char *r=NULL,*a=NULL,*text=NULL;
    if(!radius || !angle){status=BIGCOMPLEX_OUT_OF_MEMORY;goto done;}
    status=bigcomplex_abs(radius,value,digits,rounding);if(status!=BIGCOMPLEX_OK)goto done;
    status=bigcomplex_arg(angle,value,digits,rounding);if(status!=BIGCOMPLEX_OK)goto done;
    status=mapped(bigdecimal_format_mode(radius,places,rounding,mode,limit,&r));if(status!=BIGCOMPLEX_OK)goto done;
    bool rz=false, iz=false, rn=false, in=false;
    BigDecimalStatus inspection=bigdecimal_is_zero(&rz,value->real);
    if(inspection==BIGDECIMAL_OK)inspection=bigdecimal_is_zero(&iz,value->imaginary);
    if(inspection==BIGDECIMAL_OK)inspection=bigdecimal_is_negative(&rn,value->real);
    if(inspection==BIGDECIMAL_OK)inspection=bigdecimal_is_negative(&in,value->imaginary);
    if(inspection!=BIGDECIMAL_OK){status=mapped(inspection);goto done;}
    const char *symbol=rz?(in?"-π/2":"π/2"):(iz && rn?"π":NULL);
    if(symbol){
        size_t size=strlen(symbol);char *replacement=numforge_malloc(size+1);
        if(!replacement){status=BIGCOMPLEX_OUT_OF_MEMORY;goto done;}
        memcpy(replacement,symbol,size+1);a=replacement;
    }else{
        status=mapped(bigdecimal_format_mode(angle,places,rounding,mode,limit,&a));if(status!=BIGCOMPLEX_OK)goto done;
    }
    /* Omit the radius only when its stored value, rather than display rounding,
     * is exactly one. */
    BigDecimal *one=bigdecimal_create();int comparison=1;
    if(!one){status=BIGCOMPLEX_OUT_OF_MEMORY;goto done;}
    BigDecimalStatus ds=bigdecimal_set_string(one,"1");
    if(ds==BIGDECIMAL_OK)ds=bigdecimal_compare(&comparison,radius,one);
    bigdecimal_destroy(one);if(ds!=BIGDECIMAL_OK){status=mapped(ds);goto done;}
    const char *format=form==BIGCOMPLEX_FORM_TRIGONOMETRIC?
        (comparison==0?"cos(%s) + i*sin(%s)":"(%s)*(cos(%s) + i*sin(%s))"):
        (comparison==0?"e^(i*(%s))":"(%s)*e^(i*(%s))");
    int length;
    if(form==BIGCOMPLEX_FORM_TRIGONOMETRIC)length=comparison==0?snprintf(NULL,0,format,a,a):snprintf(NULL,0,format,r,a,a);
    else length=comparison==0?snprintf(NULL,0,format,a):snprintf(NULL,0,format,r,a);
    if(length<0 || (size_t)length>limit){status=BIGCOMPLEX_VALUE_TOO_LARGE;goto done;}
    text=numforge_malloc((size_t)length+1);if(!text){status=BIGCOMPLEX_OUT_OF_MEMORY;goto done;}
    if(form==BIGCOMPLEX_FORM_TRIGONOMETRIC){if(comparison==0)(void)snprintf(text,(size_t)length+1,format,a,a);else(void)snprintf(text,(size_t)length+1,format,r,a,a);}
    else{if(comparison==0)(void)snprintf(text,(size_t)length+1,format,a);else(void)snprintf(text,(size_t)length+1,format,r,a);}
    *result=text;text=NULL;
done:free(r);free(a);free(text);bigdecimal_destroy(radius);bigdecimal_destroy(angle);return status;
}
static BigComplexStatus complex_sine_cosine(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding, bool cosine, bool hyperbolic,BigComplex *paired)
{
    if (!result || !value) return BIGCOMPLEX_NULL_ARGUMENT;
    if (digits < 1 || digits > INT64_MAX-12 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO ||
        rounding > BIGDECIMAL_ROUND_HALF_EVEN) return BIGCOMPLEX_INVALID_ARGUMENT;
    BigComplex *temporary=bigcomplex_create();
    BigComplex *other=paired ? bigcomplex_create() : NULL;
    BigDecimal *sine=bigdecimal_create(),*cosine_value=bigdecimal_create();
    BigDecimal *sinh_value=bigdecimal_create(),*cosh_value=bigdecimal_create(),*one=bigdecimal_create();
    BigDecimalStatus status=BIGDECIMAL_OUT_OF_MEMORY;
    if (!temporary || (paired && !other) || !sine || !cosine_value || !sinh_value || !cosh_value || !one) goto done;
    TRY(bigdecimal_sin(sine,hyperbolic ? value->imaginary : value->real,digits+12,rounding));
    TRY(bigdecimal_cos(cosine_value,hyperbolic ? value->imaginary : value->real,digits+12,rounding));
    TRY(bigdecimal_sinh(sinh_value,hyperbolic ? value->real : value->imaginary,digits+12,rounding));
    TRY(bigdecimal_cosh(cosh_value,hyperbolic ? value->real : value->imaginary,digits+12,rounding));
    TRY(bigdecimal_set_string(one,"1"));
    /* Quotients need both outputs. Reuse the same guarded scalar evaluations;
     * component composition and rounding match the separate public calls. */
    for(int part=0;part<(paired ? 2 : 1);part++) {
        BigComplex *target=part ? other : temporary;
        bool is_cosine=part ? !cosine : cosine;
        if (hyperbolic) {
            TRY(bigdecimal_mul(target->real,is_cosine ? cosh_value : sinh_value,cosine_value));
            TRY(bigdecimal_mul(target->imaginary,is_cosine ? sinh_value : cosh_value,sine));
        } else {
            TRY(bigdecimal_mul(target->real,is_cosine ? cosine_value : sine,cosh_value));
            TRY(bigdecimal_mul(target->imaginary,is_cosine ? sine : cosine_value,sinh_value));
            if (is_cosine) TRY(bigdecimal_negate(target->imaginary,target->imaginary));
        }
        TRY(bigdecimal_div_significant(target->real,target->real,one,digits,rounding));
        TRY(bigdecimal_div_significant(target->imaginary,target->imaginary,one,digits,rounding));
    }
    commit(result,temporary);
    if(paired) commit(paired,other);
done:
    bigcomplex_destroy(temporary);bigcomplex_destroy(other);bigdecimal_destroy(sine);bigdecimal_destroy(cosine_value);
    bigdecimal_destroy(sinh_value);bigdecimal_destroy(cosh_value);bigdecimal_destroy(one);return mapped(status);
}
BigComplexStatus bigcomplex_sin(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding)
{ return complex_sine_cosine(result,value,digits,rounding,false,false,NULL); }
BigComplexStatus bigcomplex_cos(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding)
{ return complex_sine_cosine(result,value,digits,rounding,true,false,NULL); }
BigComplexStatus bigcomplex_sinh(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding)
{ return complex_sine_cosine(result,value,digits,rounding,false,true,NULL); }
BigComplexStatus bigcomplex_cosh(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding)
{ return complex_sine_cosine(result,value,digits,rounding,true,true,NULL); }
BigComplexStatus bigcomplex_tanh(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding)
{
    if (!result || !value) return BIGCOMPLEX_NULL_ARGUMENT;
    if (digits < 1 || digits > INT64_MAX-24 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO ||
        rounding > BIGDECIMAL_ROUND_HALF_EVEN) return BIGCOMPLEX_INVALID_ARGUMENT;
    BigComplex *temporary=bigcomplex_create(),*denominator=NULL;
    BigDecimal *absolute=NULL,*half=NULL,*one=NULL,*four=NULL,*t=NULL;
    BigDecimal *sine=NULL,*cosine=NULL,*square=NULL,*den=NULL;
    BigComplexStatus status=BIGCOMPLEX_OUT_OF_MEMORY;bool real=false,negative=false;int comparison=0;
    if (!temporary) goto done;
    status=mapped(bigdecimal_is_zero(&real,value->imaginary));
    if (status != BIGCOMPLEX_OK) goto done;
    if (real) {
        status=mapped(bigdecimal_tanh(temporary->real,value->real,digits,rounding));
        goto finish;
    }
    absolute=bigdecimal_create();half=bigdecimal_create();
    if (!absolute || !half) {status=BIGCOMPLEX_OUT_OF_MEMORY;goto done;}
    status=mapped(bigdecimal_abs(absolute,value->real));
    if (status == BIGCOMPLEX_OK) status=mapped(bigdecimal_set_string(half,"0.5"));
    if (status == BIGCOMPLEX_OK) status=mapped(bigdecimal_compare(&comparison,absolute,half));
    if (status != BIGCOMPLEX_OK) goto done;
    if (comparison <= 0) {
        denominator=bigcomplex_create();
        if (!denominator) {status=BIGCOMPLEX_OUT_OF_MEMORY;goto done;}
        status=complex_sine_cosine(temporary,value,digits+12,rounding,false,true,denominator);
        if (status == BIGCOMPLEX_OK) status=bigcomplex_div(temporary,temporary,denominator,digits,rounding);
        goto finish;
    }
    one=bigdecimal_create();four=bigdecimal_create();t=bigdecimal_create();
    sine=bigdecimal_create();cosine=bigdecimal_create();square=bigdecimal_create();den=bigdecimal_create();
    if (!one || !four || !t || !sine || !cosine || !square || !den) {
        status=BIGCOMPLEX_OUT_OF_MEMORY;goto done;
    }
    /* t=exp(-2|x|); D=(1-t)^2+4t*cos(y)^2, strictly positive for x!=0.
     * Avoid both growing exp(2|x|) and subtraction near imaginary-axis poles. */
    BigDecimalStatus ds=bigdecimal_set_string(one,"1");
#define TANH_TRY(call) do { ds=(call); if(ds!=BIGDECIMAL_OK){status=mapped(ds);goto done;} } while(0)
    if (ds != BIGDECIMAL_OK) {status=mapped(ds);goto done;}
    TANH_TRY(bigdecimal_set_string(four,"4"));
    TANH_TRY(bigdecimal_add(t,absolute,absolute));TANH_TRY(bigdecimal_negate(t,t));
    TANH_TRY(bigdecimal_exp(t,t,digits+12,rounding));
    TANH_TRY(bigdecimal_sin(sine,value->imaginary,digits+12,rounding));
    TANH_TRY(bigdecimal_cos(cosine,value->imaginary,digits+12,rounding));
    TANH_TRY(bigdecimal_sub(den,one,t));TANH_TRY(bigdecimal_mul(den,den,den));
    TANH_TRY(bigdecimal_mul(square,cosine,cosine));TANH_TRY(bigdecimal_mul(square,square,t));
    TANH_TRY(bigdecimal_mul(square,square,four));TANH_TRY(bigdecimal_add(den,den,square));
    TANH_TRY(bigdecimal_mul(square,t,t));TANH_TRY(bigdecimal_sub(temporary->real,one,square));
    TANH_TRY(bigdecimal_is_negative(&negative,value->real));
    if (negative) TANH_TRY(bigdecimal_negate(temporary->real,temporary->real));
    TANH_TRY(bigdecimal_div_significant(temporary->real,temporary->real,den,digits,rounding));
    TANH_TRY(bigdecimal_mul(temporary->imaginary,sine,cosine));
    TANH_TRY(bigdecimal_mul(temporary->imaginary,temporary->imaginary,t));
    TANH_TRY(bigdecimal_mul(temporary->imaginary,temporary->imaginary,four));
    TANH_TRY(bigdecimal_div_significant(temporary->imaginary,temporary->imaginary,den,digits,rounding));
#undef TANH_TRY
finish:
    if (status == BIGCOMPLEX_OK) commit(result,temporary);
done:
    bigcomplex_destroy(temporary);bigcomplex_destroy(denominator);bigdecimal_destroy(absolute);bigdecimal_destroy(half);
    bigdecimal_destroy(one);bigdecimal_destroy(four);bigdecimal_destroy(t);bigdecimal_destroy(sine);
    bigdecimal_destroy(cosine);bigdecimal_destroy(square);bigdecimal_destroy(den);return status;
}
BigComplexStatus bigcomplex_tan(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding)
{
    if (!result || !value) return BIGCOMPLEX_NULL_ARGUMENT;
    if (digits < 1 || digits > INT64_MAX-24 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO ||
        rounding > BIGDECIMAL_ROUND_HALF_EVEN) return BIGCOMPLEX_INVALID_ARGUMENT;
    if(!bigint_is_zero(value->imaginary->coefficient)) {
        BigDecimal *absolute=bigdecimal_create(),*half=bigdecimal_create();int comparison=0;
        BigComplexStatus check=BIGCOMPLEX_OUT_OF_MEMORY;
        if(absolute && half) {
            check=mapped(bigdecimal_abs(absolute,value->imaginary));
            if(check==BIGCOMPLEX_OK) check=mapped(bigdecimal_set_string(half,"0.5"));
            if(check==BIGCOMPLEX_OK) check=mapped(bigdecimal_compare(&comparison,absolute,half));
        }
        bigdecimal_destroy(absolute);bigdecimal_destroy(half);
        if(check!=BIGCOMPLEX_OK) return check;
        if(comparison>0) {
            if(digits>INT64_MAX-36) return BIGCOMPLEX_VALUE_TOO_LARGE;
            BigComplex *rotated=bigcomplex_create(),*temporary=bigcomplex_create();
            BigDecimal *one=bigdecimal_create();check=BIGCOMPLEX_OUT_OF_MEMORY;
            if(rotated && temporary && one) {
                /* tan(z)=-i*tanh(i*z). The scaled tanh path avoids growing
                 * exponentials; rotate before final directed rounding. */
                check=mapped(bigdecimal_negate(rotated->real,value->imaginary));
                if(check==BIGCOMPLEX_OK) check=mapped(bigdecimal_copy(rotated->imaginary,value->real));
                if(check==BIGCOMPLEX_OK) check=bigcomplex_tanh(rotated,rotated,digits+12,BIGDECIMAL_ROUND_HALF_EVEN);
                if(check==BIGCOMPLEX_OK) check=mapped(bigdecimal_copy(temporary->real,rotated->imaginary));
                if(check==BIGCOMPLEX_OK) check=mapped(bigdecimal_negate(temporary->imaginary,rotated->real));
                if(check==BIGCOMPLEX_OK) check=mapped(bigdecimal_set_string(one,"1"));
                if(check==BIGCOMPLEX_OK) check=mapped(bigdecimal_div_significant(temporary->real,temporary->real,one,digits,rounding));
                if(check==BIGCOMPLEX_OK) check=mapped(bigdecimal_div_significant(temporary->imaginary,temporary->imaginary,one,digits,rounding));
                if(check==BIGCOMPLEX_OK) commit(result,temporary);
            }
            bigcomplex_destroy(rotated);bigcomplex_destroy(temporary);bigdecimal_destroy(one);return check;
        }
    }
    BigComplex *numerator=bigcomplex_create(),*denominator=bigcomplex_create();
    BigComplexStatus status=BIGCOMPLEX_OUT_OF_MEMORY;
    if (!numerator || !denominator) goto done;
    status=complex_sine_cosine(numerator,value,digits+12,rounding,false,false,denominator);
    if (status == BIGCOMPLEX_OK) status=bigcomplex_div(numerator,numerator,denominator,digits,rounding);
    if (status == BIGCOMPLEX_OK) commit(result,numerator);
done:
    bigcomplex_destroy(numerator);bigcomplex_destroy(denominator);return status;
}
BigComplexStatus bigcomplex_asin(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding)
{
    if (!result || !value) return BIGCOMPLEX_NULL_ARGUMENT;
    if (digits < 1 || digits > INT64_MAX-60 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO ||
        rounding > BIGDECIMAL_ROUND_HALF_EVEN) return BIGCOMPLEX_INVALID_ARGUMENT;
    BigComplex *z=bigcomplex_create(),*t=bigcomplex_create(),*one=bigcomplex_create();
    BigDecimal *unit=bigdecimal_create();
    BigComplexStatus status=BIGCOMPLEX_OUT_OF_MEMORY;
    bool nx=false,ny=false,xzero=false,yzero=false,interior=false;int comparison=0;
    if (!z || !t || !one || !unit) goto done;
#define INVERSE_TRY(call) do { status=(call); if(status!=BIGCOMPLEX_OK)goto done; } while(0)
#define INVERSE_DEC(call) INVERSE_TRY(mapped(call))
    INVERSE_DEC(bigdecimal_is_negative(&nx,value->real));
    INVERSE_DEC(bigdecimal_is_negative(&ny,value->imaginary));
    INVERSE_DEC(bigdecimal_is_zero(&xzero,value->real));
    INVERSE_DEC(bigdecimal_is_zero(&yzero,value->imaginary));
    INVERSE_DEC(bigdecimal_set_string(unit,"1"));
    if (xzero) {
        INVERSE_DEC(bigdecimal_asinh(t->imaginary,value->imaginary,digits,rounding));
    } else if (yzero) {
        INVERSE_DEC(bigdecimal_abs(z->real,value->real));
        INVERSE_DEC(bigdecimal_compare(&comparison,z->real,unit));
        if (comparison<=0) INVERSE_DEC(bigdecimal_asin(t->real,value->real,digits,rounding));
        else {
            INVERSE_DEC(bigdecimal_set_constant_significant(t->real,BIGDECIMAL_CONSTANT_PI,digits+12,rounding));
            INVERSE_DEC(bigdecimal_set_string(z->imaginary,"2"));
            if(nx) INVERSE_DEC(bigdecimal_negate(t->real,t->real));
            INVERSE_DEC(bigdecimal_div_significant(t->real,t->real,z->imaginary,digits,rounding));
            INVERSE_DEC(bigdecimal_acosh(t->imaginary,z->real,digits+12,rounding));
            if(!nx) INVERSE_DEC(bigdecimal_negate(t->imaginary,t->imaginary));
            INVERSE_DEC(bigdecimal_div_significant(t->imaginary,t->imaginary,unit,digits,rounding));
        }
    } else {
        /* Work in x>=0,y<0: sqrt(1-z*z)+i*z adds components with matching
         * signs. Reflect the result only before its final directed rounding. */
        INVERSE_DEC(bigdecimal_abs(z->real,value->real));
        INVERSE_DEC(bigdecimal_abs(z->imaginary,value->imaginary));
        INVERSE_DEC(bigdecimal_compare(&comparison,z->real,unit));interior=comparison<=0;
        INVERSE_DEC(bigdecimal_set_string(one->real,"0.5"));
        INVERSE_DEC(bigdecimal_compare(&comparison,z->imaginary,one->real));interior=interior && comparison<=0;
        INVERSE_DEC(bigdecimal_negate(z->imaginary,z->imaginary));
        INVERSE_TRY(bigcomplex_set_strings(one,"1","0"));
        INVERSE_TRY(bigcomplex_mul(t,z,z));
        INVERSE_TRY(bigcomplex_sub(t,one,t));
        INVERSE_TRY(bigcomplex_sqrt(t,t,digits+24,rounding));
        if(interior) {
            /* The logarithmic formula can drown a tiny imaginary component
             * in the rounded square root's norm error. In this region the
             * principal atan(z/sqrt(1-z*z)) has no quadrant correction. */
            INVERSE_TRY(bigcomplex_div(one,z,t,digits+24,rounding));
            INVERSE_TRY(bigcomplex_atan(t,one,digits+12,rounding));
            INVERSE_TRY(bigcomplex_copy(z,t));
        } else {
            INVERSE_DEC(bigdecimal_sub(t->real,t->real,z->imaginary));
            INVERSE_DEC(bigdecimal_add(t->imaginary,t->imaginary,z->real));
            INVERSE_TRY(bigcomplex_ln(t,t,digits+12,rounding));
            INVERSE_DEC(bigdecimal_copy(z->real,t->imaginary));
            INVERSE_DEC(bigdecimal_negate(z->imaginary,t->real));
        }
        if(nx) INVERSE_DEC(bigdecimal_negate(z->real,z->real));
        if(!ny) INVERSE_DEC(bigdecimal_negate(z->imaginary,z->imaginary));
        INVERSE_DEC(bigdecimal_div_significant(t->real,z->real,unit,digits,rounding));
        INVERSE_DEC(bigdecimal_div_significant(t->imaginary,z->imaginary,unit,digits,rounding));
    }
    commit(result,t);
done:
    bigcomplex_destroy(z);bigcomplex_destroy(t);bigcomplex_destroy(one);bigdecimal_destroy(unit);
    return status;
#undef INVERSE_DEC
#undef INVERSE_TRY
}
BigComplexStatus bigcomplex_acos(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding)
{
    if (!result || !value) return BIGCOMPLEX_NULL_ARGUMENT;
    if (digits < 1 || digits > INT64_MAX-72 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO ||
        rounding > BIGDECIMAL_ROUND_HALF_EVEN) return BIGCOMPLEX_INVALID_ARGUMENT;
    BigComplex *t=bigcomplex_create(),*one=bigcomplex_create();
    BigDecimal *two=bigdecimal_create();BigComplexStatus status=BIGCOMPLEX_OUT_OF_MEMORY;
    if(!t || !one || !two)goto done;
    /* 2*asin(sqrt((1-z)/2)) preserves small acos near z=1. */
    status=bigcomplex_set_strings(one,"1","0");
    if(status==BIGCOMPLEX_OK)status=bigcomplex_sub(t,one,value);
    if(status==BIGCOMPLEX_OK)status=mapped(bigdecimal_set_string(two,"2"));
    if(status==BIGCOMPLEX_OK)status=mapped(bigdecimal_div_significant(t->real,t->real,two,digits+12,rounding));
    if(status==BIGCOMPLEX_OK)status=mapped(bigdecimal_div_significant(t->imaginary,t->imaginary,two,digits+12,rounding));
    if(status==BIGCOMPLEX_OK)status=bigcomplex_sqrt(t,t,digits+12,rounding);
    if(status==BIGCOMPLEX_OK)status=bigcomplex_asin(t,t,digits+12,rounding);
    if(status==BIGCOMPLEX_OK)status=mapped(bigdecimal_mul(t->real,t->real,two));
    if(status==BIGCOMPLEX_OK)status=mapped(bigdecimal_mul(t->imaginary,t->imaginary,two));
    if(status==BIGCOMPLEX_OK)status=mapped(bigdecimal_set_string(two,"1"));
    if(status==BIGCOMPLEX_OK)status=mapped(bigdecimal_div_significant(t->real,t->real,two,digits,rounding));
    if(status==BIGCOMPLEX_OK)status=mapped(bigdecimal_div_significant(t->imaginary,t->imaginary,two,digits,rounding));
    if(status==BIGCOMPLEX_OK)commit(result,t);
done:bigcomplex_destroy(t);bigcomplex_destroy(one);bigdecimal_destroy(two);return status;
}
BigComplexStatus bigcomplex_atan(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding)
{
    if (!result || !value) return BIGCOMPLEX_NULL_ARGUMENT;
    if (digits < 1 || digits > INT64_MAX-48 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO ||
        rounding > BIGDECIMAL_ROUND_HALF_EVEN) return BIGCOMPLEX_INVALID_ARGUMENT;
    BigComplex *t=bigcomplex_create(),*angle=bigcomplex_create();
    BigDecimal *x=bigdecimal_create(),*y=bigdecimal_create(),*one=bigdecimal_create();
    BigDecimal *four=bigdecimal_create(),*d=bigdecimal_create(),*square=bigdecimal_create();
    BigComplexStatus status=BIGCOMPLEX_OUT_OF_MEMORY;bool nx=false,ny=false,xzero=false,yzero=false,zero=false;
    if(!t || !angle || !x || !y || !one || !four || !d || !square)goto done;
#define ATAN_TRY(call) do { status=mapped(call);if(status!=BIGCOMPLEX_OK)goto done; } while(0)
    ATAN_TRY(bigdecimal_is_negative(&nx,value->real));ATAN_TRY(bigdecimal_is_negative(&ny,value->imaginary));
    ATAN_TRY(bigdecimal_is_zero(&xzero,value->real));ATAN_TRY(bigdecimal_is_zero(&yzero,value->imaginary));
    if(yzero){ATAN_TRY(bigdecimal_atan(t->real,value->real,digits,rounding));goto finish;}
    ATAN_TRY(bigdecimal_abs(x,value->real));ATAN_TRY(bigdecimal_abs(y,value->imaginary));
    ATAN_TRY(bigdecimal_set_string(one,"1"));ATAN_TRY(bigdecimal_set_string(four,"4"));
    ATAN_TRY(bigdecimal_mul(square,x,x));ATAN_TRY(bigdecimal_sub(d,y,one));
    ATAN_TRY(bigdecimal_mul(d,d,d));ATAN_TRY(bigdecimal_add(d,d,square));
    ATAN_TRY(bigdecimal_is_zero(&zero,d));
    if(zero){status=BIGCOMPLEX_INVALID_ARGUMENT;goto done;}
    /* Im atan = sign(y)*ln(1+4|y|/(x*x+(|y|-1)^2))/4.
     * Exact addition avoids cancellation between two almost equal logs. */
    ATAN_TRY(bigdecimal_mul(t->imaginary,four,y));
    ATAN_TRY(bigdecimal_div_significant(t->imaginary,t->imaginary,d,digits+24,rounding));
    ATAN_TRY(bigdecimal_add(t->imaginary,t->imaginary,one));
    ATAN_TRY(bigdecimal_ln(t->imaginary,t->imaginary,digits+12,rounding));
    if(ny)ATAN_TRY(bigdecimal_negate(t->imaginary,t->imaginary));
    ATAN_TRY(bigdecimal_div_significant(t->imaginary,t->imaginary,four,digits,rounding));
    ATAN_TRY(bigdecimal_mul(d,y,y));ATAN_TRY(bigdecimal_add(d,d,square));
    ATAN_TRY(bigdecimal_sub(angle->real,one,d));ATAN_TRY(bigdecimal_add(angle->imaginary,x,x));
    status=bigcomplex_arg(t->real,angle,digits+12,rounding);if(status!=BIGCOMPLEX_OK)goto done;
    if(nx || (xzero && ny))ATAN_TRY(bigdecimal_negate(t->real,t->real));
    ATAN_TRY(bigdecimal_set_string(four,"2"));
    ATAN_TRY(bigdecimal_div_significant(t->real,t->real,four,digits,rounding));
finish:commit(result,t);
done:
    bigcomplex_destroy(t);bigcomplex_destroy(angle);bigdecimal_destroy(x);bigdecimal_destroy(y);
    bigdecimal_destroy(one);bigdecimal_destroy(four);bigdecimal_destroy(d);bigdecimal_destroy(square);return status;
#undef ATAN_TRY
}
/* A conservative decimal coefficient width from the binary bit count. Extra
 * input-sensitive precision protects cancellation between almost equal logs.
 * This is not a certified error bound for arbitrary transcendental relations. */
static size_t complex_input_guard(const BigComplex *value,const BigComplex *base)
{
    const BigDecimal *parts[]={value->real,value->imaginary,base->real,base->imaginary};
    size_t guard=0;
    for(size_t i=0;i<4;i++) {
        size_t width=bigint_bit_length(parts[i]->coefficient)/3+1;
        if(width>guard) guard=width;
    }
    return guard;
}
BigComplexStatus bigcomplex_log(BigComplex *result, const BigComplex *value,
    const BigComplex *base, int64_t digits, BigDecimalRoundingMode rounding)
{
    if (!result || !value || !base) return BIGCOMPLEX_NULL_ARGUMENT;
    if (digits < 1 || digits > INT64_MAX-24 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO ||
        rounding > BIGDECIMAL_ROUND_HALF_EVEN) return BIGCOMPLEX_INVALID_ARGUMENT;
    size_t guard=complex_input_guard(value,base);
    if (guard>(uint64_t)(INT64_MAX-digits-24)) return BIGCOMPLEX_VALUE_TOO_LARGE;
    int64_t working=digits+12+(int64_t)guard;
    BigComplex *numerator=bigcomplex_create(),*denominator=bigcomplex_create();
    BigComplexStatus status=BIGCOMPLEX_OUT_OF_MEMORY;bool zero=false;
    if (!numerator || !denominator) goto done;
    status=bigcomplex_ln(numerator,value,working,rounding);
    if (status == BIGCOMPLEX_OK) status=bigcomplex_ln(denominator,base,working,rounding);
    if (status == BIGCOMPLEX_OK) status=bigcomplex_is_zero(&zero,denominator);
    if (status == BIGCOMPLEX_OK && zero) status=BIGCOMPLEX_INVALID_ARGUMENT;
    if (status == BIGCOMPLEX_OK) status=bigcomplex_div(numerator,numerator,denominator,digits,rounding);
    if (status == BIGCOMPLEX_OK) commit(result,numerator);
done:
    bigcomplex_destroy(numerator);bigcomplex_destroy(denominator);return status;
}
BigComplexStatus bigcomplex_pow(BigComplex *result, const BigComplex *value,
    const BigComplex *exponent, int64_t digits, BigDecimalRoundingMode rounding)
{
    if (!result || !value || !exponent) return BIGCOMPLEX_NULL_ARGUMENT;
    if (digits < 1 || digits > INT64_MAX-24 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO ||
        rounding > BIGDECIMAL_ROUND_HALF_EVEN) return BIGCOMPLEX_INVALID_ARGUMENT;
    bool zero=false, exponent_zero=false, real=false, negative=false;
    BigComplexStatus status=bigcomplex_is_zero(&zero,value);
    if (status == BIGCOMPLEX_OK) status=bigcomplex_is_zero(&exponent_zero,exponent);
    if (status != BIGCOMPLEX_OK) return status;
    BigComplex *temporary=bigcomplex_create();
    if (!temporary) return BIGCOMPLEX_OUT_OF_MEMORY;
    if (exponent_zero) status=bigcomplex_set_strings(temporary,"1","0");
    else if (zero) {
        status=mapped(bigdecimal_is_zero(&real,exponent->imaginary));
        if (status == BIGCOMPLEX_OK && !real) status=BIGCOMPLEX_INVALID_ARGUMENT;
        if (status == BIGCOMPLEX_OK) status=mapped(bigdecimal_is_negative(&negative,exponent->real));
        if (status == BIGCOMPLEX_OK && negative) status=BIGCOMPLEX_DIVISION_BY_ZERO;
    } else {
        size_t guard=complex_input_guard(value,exponent);
        if(guard>(uint64_t)(INT64_MAX-digits-24)) {status=BIGCOMPLEX_VALUE_TOO_LARGE;goto done;}
        status=bigcomplex_ln(temporary,value,digits+12+(int64_t)guard,BIGDECIMAL_ROUND_HALF_EVEN);
        if (status == BIGCOMPLEX_OK) status=bigcomplex_mul(temporary,exponent,temporary);
        if (status == BIGCOMPLEX_OK) status=bigcomplex_exp(temporary,temporary,digits,rounding);
    }
    if (status == BIGCOMPLEX_OK) commit(result,temporary);
done:
    bigcomplex_destroy(temporary);return status;
}
BigComplexStatus bigcomplex_ln(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding)
{
    if (!result || !value) return BIGCOMPLEX_NULL_ARGUMENT;
    if (digits < 1 || digits > INT64_MAX-12 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO ||
        rounding > BIGDECIMAL_ROUND_HALF_EVEN) return BIGCOMPLEX_INVALID_ARGUMENT;
    int64_t scale=common_scale(value);
    if(scale>INT64_MAX/2 || scale<INT64_MIN/2) {
        if(digits>INT64_MAX-44) return BIGCOMPLEX_VALUE_TOO_LARGE;
        BigComplex *normalized=bigcomplex_create(),*temporary=bigcomplex_create();
        BigDecimal *squared=bigdecimal_create(),*factor=bigdecimal_create(),*ln10=bigdecimal_create(),*two=bigdecimal_create();
        BigComplexStatus check=BIGCOMPLEX_OUT_OF_MEMORY;char text[32];
        if(normalized && temporary && squared && factor && ln10 && two) {
            /* ln|z|=ln|z*10^scale|-scale*ln(10). For extreme common
             * exponents the correction avoids an unrepresentable square. */
            check=copy_scaled(normalized,value,scale);
            if(check==BIGCOMPLEX_OK) check=bigcomplex_abs_squared(squared,normalized);
            if(check==BIGCOMPLEX_OK) check=mapped(bigdecimal_ln(temporary->real,squared,digits+32,BIGDECIMAL_ROUND_HALF_EVEN));
            if(check==BIGCOMPLEX_OK) check=mapped(bigdecimal_set_string(two,"2"));
            if(check==BIGCOMPLEX_OK) check=mapped(bigdecimal_div_significant(temporary->real,temporary->real,two,digits+32,BIGDECIMAL_ROUND_HALF_EVEN));
            if(check==BIGCOMPLEX_OK) check=mapped(bigdecimal_set_string(ln10,"10"));
            if(check==BIGCOMPLEX_OK) check=mapped(bigdecimal_ln(ln10,ln10,digits+32,BIGDECIMAL_ROUND_HALF_EVEN));
            snprintf(text,sizeof(text),"%lld",(long long)scale);
            if(check==BIGCOMPLEX_OK) check=mapped(bigdecimal_set_string(factor,text));
            if(check==BIGCOMPLEX_OK) check=mapped(bigdecimal_mul(factor,factor,ln10));
            if(check==BIGCOMPLEX_OK) check=mapped(bigdecimal_sub(temporary->real,temporary->real,factor));
            if(check==BIGCOMPLEX_OK) check=mapped(bigdecimal_round_significant(temporary->real,temporary->real,digits,rounding));
            if(check==BIGCOMPLEX_OK) check=bigcomplex_arg(temporary->imaginary,value,digits,rounding);
            if(check==BIGCOMPLEX_OK) commit(result,temporary);
        }
        bigcomplex_destroy(normalized);bigcomplex_destroy(temporary);bigdecimal_destroy(squared);
        bigdecimal_destroy(factor);bigdecimal_destroy(ln10);bigdecimal_destroy(two);return check;
    }
    BigComplex *temporary=bigcomplex_create();
    BigDecimal *squared=bigdecimal_create(),*two=bigdecimal_create();
    BigComplexStatus status=BIGCOMPLEX_OUT_OF_MEMORY;
    if (!temporary || !squared || !two) goto done;
    /* No rounded magnitude before ln: preserve small nonzero log magnitudes
     * for values close to the unit circle. */
    status=bigcomplex_abs_squared(squared,value);
    if (status == BIGCOMPLEX_OK) status=mapped(bigdecimal_ln(temporary->real,squared,digits+12,rounding));
    if (status == BIGCOMPLEX_OK) status=mapped(bigdecimal_set_string(two,"2"));
    if (status == BIGCOMPLEX_OK) status=mapped(bigdecimal_div_significant(
        temporary->real,temporary->real,two,digits,rounding));
    if (status == BIGCOMPLEX_OK) status=bigcomplex_arg(temporary->imaginary,value,digits,rounding);
    if (status == BIGCOMPLEX_OK) commit(result,temporary);
done:
    bigcomplex_destroy(temporary);bigdecimal_destroy(squared);bigdecimal_destroy(two);
    return status;
}
BigComplexStatus bigcomplex_exp(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding)
{
    if (!result || !value) return BIGCOMPLEX_NULL_ARGUMENT;
    if (digits < 1 || digits > INT64_MAX-12 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO ||
        rounding > BIGDECIMAL_ROUND_HALF_EVEN) return BIGCOMPLEX_INVALID_ARGUMENT;
    BigComplex *temporary=bigcomplex_create();
    BigDecimal *radius=bigdecimal_create(), *one=bigdecimal_create();
    BigDecimalStatus status=BIGDECIMAL_OUT_OF_MEMORY;
    if (!temporary || !radius || !one) goto done;
    TRY(bigdecimal_exp(radius,value->real,digits+12,rounding));
    TRY(bigdecimal_cos(temporary->real,value->imaginary,digits+12,rounding));
    TRY(bigdecimal_sin(temporary->imaginary,value->imaginary,digits+12,rounding));
    TRY(bigdecimal_mul(temporary->real,temporary->real,radius));
    TRY(bigdecimal_mul(temporary->imaginary,temporary->imaginary,radius));
    TRY(bigdecimal_set_string(one,"1"));
    TRY(bigdecimal_div_significant(temporary->real,temporary->real,one,digits,rounding));
    TRY(bigdecimal_div_significant(temporary->imaginary,temporary->imaginary,one,digits,rounding));
    commit(result,temporary);
done:
    bigcomplex_destroy(temporary);bigdecimal_destroy(radius);bigdecimal_destroy(one);
    return mapped(status);
}
BigComplexStatus bigcomplex_set_polar(BigComplex *result, const BigDecimal *radius,
    const BigDecimal *angle, int64_t digits, BigDecimalRoundingMode rounding)
{
    if(!result || !radius || !angle)return BIGCOMPLEX_NULL_ARGUMENT;
    if(digits<1 || digits>INT64_MAX-12 || rounding<BIGDECIMAL_ROUND_TOWARD_ZERO || rounding>BIGDECIMAL_ROUND_HALF_EVEN)
        return BIGCOMPLEX_INVALID_ARGUMENT;
    bool negative=false,zero=false;BigDecimalStatus status=bigdecimal_is_negative(&negative,radius);
    if(status!=BIGDECIMAL_OK)return mapped(status);
    if(negative)return BIGCOMPLEX_INVALID_ARGUMENT;
    status=bigdecimal_is_zero(&zero,radius);if(status!=BIGDECIMAL_OK)return mapped(status);
    BigComplex *temporary=bigcomplex_create();if(!temporary)return BIGCOMPLEX_OUT_OF_MEMORY;
    if(!zero){
        TRY(bigdecimal_cos(temporary->real,angle,digits+12,rounding));
        TRY(bigdecimal_sin(temporary->imaginary,angle,digits+12,rounding));
        TRY(bigdecimal_mul(temporary->real,temporary->real,radius));
        TRY(bigdecimal_mul(temporary->imaginary,temporary->imaginary,radius));
    }
    commit(result,temporary);
done:bigcomplex_destroy(temporary);return mapped(status);
}

/* Rotations reuse the guarded inverse-trigonometric kernels and their cuts.
 * Round after the final sign changes so directed rounding remains directed. */
static BigComplexStatus inverse_hyperbolic(BigComplex *result,const BigComplex *value,
    int64_t digits,BigDecimalRoundingMode rounding,int operation)
{
    if (!result || !value) return BIGCOMPLEX_NULL_ARGUMENT;
    if (digits<1 || digits>INT64_MAX-84 || rounding<BIGDECIMAL_ROUND_TOWARD_ZERO ||
        rounding>BIGDECIMAL_ROUND_HALF_EVEN) return BIGCOMPLEX_INVALID_ARGUMENT;
    BigComplex *rotated=bigcomplex_create(),*inverse=bigcomplex_create(),*temporary=bigcomplex_create();
    BigDecimal *one=bigdecimal_create();BigComplexStatus status=BIGCOMPLEX_OUT_OF_MEMORY;
    bool negative=false;
    if (!rotated || !inverse || !temporary || !one) goto done;
#define HYP_DEC(call) do { status=mapped(call);if(status!=BIGCOMPLEX_OK)goto done; } while(0)
    if (operation==1) status=bigcomplex_acos(inverse,value,digits+12,BIGDECIMAL_ROUND_HALF_EVEN);
    else {
        HYP_DEC(bigdecimal_negate(rotated->real,value->imaginary));
        HYP_DEC(bigdecimal_copy(rotated->imaginary,value->real));
        status=operation==0 ? bigcomplex_asin(inverse,rotated,digits+12,BIGDECIMAL_ROUND_HALF_EVEN) :
            bigcomplex_atan(inverse,rotated,digits+12,BIGDECIMAL_ROUND_HALF_EVEN);
    }
    if (status!=BIGCOMPLEX_OK) goto done;
    HYP_DEC(bigdecimal_copy(temporary->real,inverse->imaginary));
    HYP_DEC(bigdecimal_negate(temporary->imaginary,inverse->real));
    if (operation==1) {
        HYP_DEC(bigdecimal_negate(temporary->real,temporary->real));
        HYP_DEC(bigdecimal_negate(temporary->imaginary,temporary->imaginary));
        HYP_DEC(bigdecimal_is_negative(&negative,temporary->real));
        if (negative) {
            HYP_DEC(bigdecimal_negate(temporary->real,temporary->real));
            HYP_DEC(bigdecimal_negate(temporary->imaginary,temporary->imaginary));
        }
    }
    HYP_DEC(bigdecimal_set_string(one,"1"));
    HYP_DEC(bigdecimal_div_significant(temporary->real,temporary->real,one,digits,rounding));
    HYP_DEC(bigdecimal_div_significant(temporary->imaginary,temporary->imaginary,one,digits,rounding));
    commit(result,temporary);
done:
    bigcomplex_destroy(rotated);bigcomplex_destroy(inverse);bigcomplex_destroy(temporary);bigdecimal_destroy(one);
    return status;
#undef HYP_DEC
}
BigComplexStatus bigcomplex_asinh(BigComplex *result,const BigComplex *value,int64_t digits,BigDecimalRoundingMode rounding)
{ return inverse_hyperbolic(result,value,digits,rounding,0); }
BigComplexStatus bigcomplex_acosh(BigComplex *result,const BigComplex *value,int64_t digits,BigDecimalRoundingMode rounding)
{ return inverse_hyperbolic(result,value,digits,rounding,1); }
BigComplexStatus bigcomplex_atanh(BigComplex *result,const BigComplex *value,int64_t digits,BigDecimalRoundingMode rounding)
{ return inverse_hyperbolic(result,value,digits,rounding,2); }
