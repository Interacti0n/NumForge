#ifndef MYSCIENCECALC_BIGINT_H
#define MYSCIENCECALC_BIGINT_H

#include <stdint.h>

typedef struct BigInt BigInt;

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
int bigint_copy( /*Create a copy of a BigInt*/
    BigInt *destination, 
    const BigInt *source
);
int bigint_set_string( /*Transform string to BigInt*/
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

int bigint_compare_abs( /*Compare two absolute values of BigInts*/
    const BigInt *a, 
    const BigInt *b
);
int bigint_compare( /*Compare two BigInts*/
    const BigInt *a, 
    const BigInt *b
);
int bigint_is_zero( /*Check if a BigInt is zero*/
    const BigInt *value
);
int bigint_is_one( /*Check if a BigInt is one*/
    const BigInt *value
);
int bigint_is_negative( /*Check if a BigInt is negative*/
    const BigInt *value
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Arithmetic operation functions for BigInt.
------------------------------------------------------------------------------------------------------------------------------
*/

int bigint_add( /*Add two BigInts (a+b)*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
);
int bigint_sub( /*Subtract two BigInts (a-b)*/
    BigInt *result, 
    const BigInt *a, 
    const BigInt *b
);
int bigint_mul( /*Multiply two BigInts (a*b)*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
);
int bigint_div( /*Divide two BigInts (a/b)*/
    BigInt *quotient,
    BigInt *remainder,
    const BigInt *a,
    const BigInt *b
);
int bigint_mod( /*Modulo operation for BigInts (a%b)*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
);
int bigint_div_mod( /*Divide and modulo operation for BigInts (a/b and a%b)*/
    BigInt *quotient,
    BigInt *remainder,
    const BigInt *a,
    const BigInt *b
);
int bigint_pow( /*Exponentiation for BigInts (base^exponent)*/
    BigInt *result,
    const BigInt *base,
    const BigInt *exponent
);
int bigint_gcd( /*Greatest common divisor for BigInts (gcd(a,b))*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
);
int bigint_lcm( /*Least common multiple for BigInts (lcm(a,b))*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
);
int bigint_factorial( /*Calculate factorial of a BigInt (n!)*/
    BigInt *result,
    const BigInt *value
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Bitwise operation functions for BigInt.
------------------------------------------------------------------------------------------------------------------------------
*/

int bigint_and( /*Bitwise AND for BigInts (a&b)*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
);
int bigint_or( /*Bitwise OR for BigInts (a|b)*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
);
int bigint_xor( /*Bitwise XOR for BigInts (a^b)*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
);
int bigint_not( /*Bitwise NOT for BigInts (~a)*/
    BigInt *result,
    const BigInt *a
);
int bigint_shift_left( /*Left shift for BigInts (a<<n)*/
    BigInt *result,
    const BigInt *a,
    size_t n
);
int bigint_shift_right( /*Right shift for BigInts (a>>n)*/
    BigInt *result,
    const BigInt *a,
    size_t n
);
int bigint_increment( /*Increment a BigInt by 1 (a++)*/
    BigInt *value
);
int bigint_decrement( /*Decrement a BigInt by 1 (a--)*/
    BigInt *value
);
int bigint_abs( /*Absolute value of a BigInt (|a|)*/
    BigInt *result,
    const BigInt *value
);
int bigint_negate( /*Negate a BigInt (-a)*/
    BigInt *result,
    const BigInt *value
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
bool bigint_is_prime( /*Check if a BigInt is prime (basic check)*/
    const BigInt *value
);
bool bigint_is_perfect_square( /*Check if a BigInt is a perfect square*/
    const BigInt *value
);

#endif