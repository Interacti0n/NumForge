#ifndef NUMFORGE_CORE_STORAGE_H
#define NUMFORGE_CORE_STORAGE_H

#include <numforge/bigrationalcomplex.h>
#include "src/runtime/storage.h"

/* Private, allocation-free queries over opaque values. Count object allocations
 * and reserved limb capacity, excluding allocator metadata/RSS. NULL is zero;
 * overflow saturates. No storage layouts cross this boundary. */
size_t bigint_storage_bytes(const BigInt *value);
size_t bigdecimal_storage_bytes(const BigDecimal *value);
size_t bigrational_storage_bytes(const BigRational *value);
size_t bigcomplex_storage_bytes(const BigComplex *value);
size_t bigrationalcomplex_storage_bytes(const BigRationalComplex *value);

#endif
