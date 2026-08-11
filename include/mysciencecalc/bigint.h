#ifndef MYSCIENCECALC_BIGINT_H
#define MYSCIENCECALC_BIGINT_H

#include <stdint.h>

typedef struct BigInt BigInt;



BigInt *bigint_create(void);
void bigint_destroy(BigInt *value);

int bigint_set_uint64(BigInt *value, uint64_t number);

char *bigint_to_string(const BigInt *value);
int bigint_set_string(BigInt *value, const char *string);

int bigint_copy(BigInt *destination, const BigInt *source);

int bigint_compare(const BigInt *a, const BigInt *b);

int bigint_add(BigInt *result, const BigInt *a, const BigInt *b);

int bigint_sub(BigInt *result, const BigInt *a, const BigInt *b);

int bigint_mul(BigInt *result, const BigInt *a, const BigInt *b);

#endif