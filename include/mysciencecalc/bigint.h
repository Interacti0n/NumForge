#ifndef MYSCIENCECALC_BIGINT_H
#define MYSCIENCECALC_BIGINT_H

#include <stddef.h>
#include <stdbool.h>

typedef struct BigInt BigInt;

/*
------------------------------------------------------------------------------------------------------------------------------
    Status codes returned by every BigInt operation that can fail.

    BIGINT_OK is 0, matching the usual C convention for "success" - note
    this is the OPPOSITE of the old int-returning API, which used 0 for
    failure and 1 for success. Code written against the old API needs
    updating from `if (!bigint_add(...))` to `if (bigint_add(...) != BIGINT_OK)`.
------------------------------------------------------------------------------------------------------------------------------
*/
typedef enum BigIntStatus
{
    BIGINT_OK = 0,              /*Operation completed successfully*/
    BIGINT_NULL_ARGUMENT,       /*A required pointer argument was NULL*/
    BIGINT_OUT_OF_MEMORY,       /*Allocation failed, or a required size doesn't fit in size_t*/
    BIGINT_DIVISION_BY_ZERO,    /*The divisor was zero*/
    BIGINT_INVALID_ARGUMENT,    /*e.g. bigint_set_string given an empty, sign-only, or non-numeric string*/
    BIGINT_NEGATIVE_ARGUMENT,   /*Operation requires a non-negative argument and didn't get one
                                 *(bigint_pow's exponent, bigint_and/or/xor's operands, bigint_factorial's input)*/
    BIGINT_VALUE_TOO_LARGE       /* Input is finite but exceeds the supported
                                  * range or practical limits of the operation */
} BigIntStatus;

const char *bigint_status_to_string( /*Human-readable description of a BigIntStatus, for logging/debugging*/
    BigIntStatus status
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Operation functions for BigInt.
------------------------------------------------------------------------------------------------------------------------------
*/

BigInt *bigint_create( /*Create a new BigInt*/
    void
);
void bigint_destroy( /*Free the memory allocated for a BigInt*/
    BigInt *value
);
BigIntStatus bigint_copy( /*Create a copy of a BigInt*/
    BigInt *destination, 
    const BigInt *source
);
BigIntStatus bigint_set_string( /*Transform string to BigInt.
                                   "" and a bare sign ("+" or "-") are BIGINT_INVALID_ARGUMENT.
                                   "0" -> 0, "+123" -> 123, "-123" -> -123.*/
    BigInt *value,
    const char *string
);
char *bigint_to_string( /*Transform BigInt to string*/
    const BigInt *value
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Comparision functions for BigInt.
------------------------------------------------------------------------------------------------------------------------------
*/

int bigint_compare( /*Compare two BigInts*/
    const BigInt *a, 
    const BigInt *b
);
bool bigint_is_zero( /*Check if a BigInt is zero*/
    const BigInt *value
);
bool bigint_is_one( /*Check if a BigInt is one*/
    const BigInt *value
);
bool bigint_is_negative( /*Check if a BigInt is negative*/
    const BigInt *value
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Arithmetic operation functions for BigInt.

    Aliasing: unless documented otherwise, every function below supports
    arbitrary aliasing between its output and input BigInt* arguments -
    bigint_add(x, x, y), bigint_mul(x, x, x), bigint_div_mod(x, y, x, y)
    are all fully supported and computed correctly. The one documented
    exception: if quotient and remainder are passed as the SAME object to
    bigint_div_mod, the function will return BIGINT_INVALID_ARGUMENT and not modify either.
------------------------------------------------------------------------------------------------------------------------------
*/

BigIntStatus bigint_abs( /*Absolute value of a BigInt (|a|)*/
    BigInt *result,
    const BigInt *value
);
BigIntStatus bigint_negate( /*Negate a BigInt (-a)*/
    BigInt *result,
    const BigInt *value
);
BigIntStatus bigint_add( /*Add two BigInts (a+b)*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
);
BigIntStatus bigint_sub( /*Subtract two BigInts (a-b)*/
    BigInt *result, 
    const BigInt *a, 
    const BigInt *b
);
BigIntStatus bigint_mul( /*Multiply two BigInts (a*b)*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
);
BigIntStatus bigint_div( /*Divide two BigInts (a/b), truncating toward zero. Discards the remainder - use bigint_div_mod if you need both.*/
    BigInt *quotient,
    const BigInt *a,
    const BigInt *b
);
BigIntStatus bigint_mod( /*Modulo operation for BigInts (a%b). Discards the quotient - use bigint_div_mod if you need both.*/
    BigInt *remainder,
    const BigInt *a,
    const BigInt *b
);
BigIntStatus bigint_div_mod( /*Divide and modulo operation for BigInts (a/b and a%b), truncating toward zero.
                                quotient and remainder must be distinct objects */
    BigInt *quotient,
    BigInt *remainder,
    const BigInt *a,
    const BigInt *b
);
BigIntStatus bigint_pow( /*Exponentiation for BigInts (base^exponent). Negative exponents are not supported.*/
    BigInt *result,
    const BigInt *base,
    const BigInt *exponent
);
BigIntStatus bigint_gcd( /*Greatest common divisor for BigInts (gcd(a,b))*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
);
BigIntStatus bigint_lcm( /*Least common multiple for BigInts: lcm(a,b) = (|a|/gcd(a,b)) * |b|*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
);
BigIntStatus bigint_factorial( /* Calculate n!. Requires n >= 0. Returns BIGINT_VALUE_TOO_LARGE if n exceeds the supported input range.*/
    BigInt *result,
    const BigInt *value
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Bitwise operation functions for BigInt. Currently only supports non-negative BigInts for these operations.
    TODO: implement for negative numbers (two's complement representation).
------------------------------------------------------------------------------------------------------------------------------
*/

BigIntStatus bigint_and( /*Bitwise AND for BigInts (a&b). Operands must be non-negative.*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
);
BigIntStatus bigint_or( /*Bitwise OR for BigInts (a|b). Operands must be non-negative.*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
);
BigIntStatus bigint_xor( /*Bitwise XOR for BigInts (a^b). Operands must be non-negative.*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
);
BigIntStatus bigint_not( /*Bitwise NOT for BigInts (~a), defined arbitrary-precision as -(a+1)*/
    BigInt *result,
    const BigInt *a
);
BigIntStatus bigint_shift_left( /*Left shift for BigInts (a<<n)*/
    BigInt *result,
    const BigInt *a,
    size_t n
);
BigIntStatus bigint_shift_right( /*Right shift for BigInts (a>>n), truncating toward zero*/
    BigInt *result,
    const BigInt *a,
    size_t n
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Additional utility check functions for BigInt.
------------------------------------------------------------------------------------------------------------------------------
*/

bool bigint_is_even( /*Check if a BigInt is even*/
    const BigInt *value
);
bool bigint_is_odd( /*Check if a BigInt is odd*/
    const BigInt *value
);
bool bigint_is_probable_prime( /*Check if a BigInt is prime (bounded trial division, may produce false positives for some very large numbers)*/
                               /*TODO: Implement probabilistic primality test (Miller-Rabin)*/
    const BigInt *value
);
bool bigint_is_perfect_square( /*Check if a BigInt is a perfect square*/
    const BigInt *value
);

#endif