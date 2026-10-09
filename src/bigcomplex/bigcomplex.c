#include <numforge/bigcomplex.h>
#include <numforge/runtime.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


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
BigComplexStatus bigcomplex_abs(BigDecimal *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding)
{
    if (result == NULL || value == NULL) return BIGCOMPLEX_NULL_ARGUMENT;
    if (digits < 1 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO || rounding > BIGDECIMAL_ROUND_HALF_EVEN)
        return BIGCOMPLEX_INVALID_ARGUMENT;
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
    complex_status=bigcomplex_mul(square,temporary,temporary);
    if (complex_status == BIGCOMPLEX_OK) complex_status=bigcomplex_equal(&equal,square,value);
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
        status=bigcomplex_ln(temporary,value,digits+12,rounding);
        if (status == BIGCOMPLEX_OK) status=bigcomplex_mul(temporary,exponent,temporary);
        if (status == BIGCOMPLEX_OK) status=bigcomplex_exp(temporary,temporary,digits,rounding);
    }
    if (status == BIGCOMPLEX_OK) commit(result,temporary);
    bigcomplex_destroy(temporary);return status;
}
BigComplexStatus bigcomplex_ln(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding)
{
    if (!result || !value) return BIGCOMPLEX_NULL_ARGUMENT;
    if (digits < 1 || digits > INT64_MAX-12 || rounding < BIGDECIMAL_ROUND_TOWARD_ZERO ||
        rounding > BIGDECIMAL_ROUND_HALF_EVEN) return BIGCOMPLEX_INVALID_ARGUMENT;
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
