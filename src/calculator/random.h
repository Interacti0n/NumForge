#ifndef NUMFORGE_CALCULATOR_RANDOM_H
#define NUMFORGE_CALCULATOR_RANDOM_H

#include <stdint.h>

/* Private, non-cryptographic calculator PRNG. A nonzero state is required. */
uint64_t calculator_random_seed(void);
void calculator_random_decimal(uint64_t *state, char text[37]);

#endif
