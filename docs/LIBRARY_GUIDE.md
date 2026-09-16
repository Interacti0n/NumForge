# Using the NumForge library

NumForge installs a standalone C library. The calculator and local web server
are optional clients; applications that only need arbitrary-precision numbers
link `NumForge::numforge` and include the public headers.

## Build and install

```sh
cmake -S . -B build-library -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DNUMFORGE_BUILD_APPS=OFF \
  -DCMAKE_INSTALL_PREFIX=/your/prefix
cmake --build build-library --parallel
cmake --install build-library
```

The installed package exports `NumForge::numforge`. It contains BigInt,
BigDecimal and optional runtime-budget support; parser, evaluator and HTTP code
are not part of the installed numeric target.

## CMake consumer

```cmake
find_package(NumForge 1.0 CONFIG REQUIRED)
add_executable(example example.c)
target_link_libraries(example PRIVATE NumForge::numforge)
```

Include `<numforge/bigint.h>` for arbitrary-precision signed integers and
`<numforge/bigdecimal.h>` for exact base-10 values. `<numforge/runtime.h>` is
optional and provides an explicitly scoped, thread-local allocation/deadline
budget. Ordinary numeric calls start no budget.

## Ownership and failures

Create values with `*_create()` and release them with `*_destroy()`; destroying
`NULL` is safe. Text returned by `bigint_to_string()`,
`bigdecimal_to_string()` or `bigdecimal_format()` belongs to the caller and is
released with `free()`.

Mutating operations return a status and preserve their destination on failure.
Arithmetic supports output/input aliasing unless a header comment documents an
exception. `bigint_div_mod()` requires distinct quotient and remainder objects.
The types are opaque; do not depend on their storage layout.

## Example

```c
#include <numforge/bigdecimal.h>
#include <stdlib.h>

int main(void)
{
    BigDecimal *value = bigdecimal_create();
    char *text = NULL;
    if (value == NULL) return 1;
    if (bigdecimal_set_string(value, "2") != BIGDECIMAL_OK ||
        bigdecimal_sqrt(value, value, 40, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_format(value, 10, BIGDECIMAL_ROUND_HALF_EVEN, &text) != BIGDECIMAL_OK)
    {
        bigdecimal_destroy(value);
        return 1;
    }
    /* use text ... */
    free(text);
    bigdecimal_destroy(value);
    return 0;
}
```

For complete signatures, domains, rounding, runtime budgets and threading
guidance see [API.md](API.md). The installed package is exercised by the
repository's C and C++ package-consumer tests.

Real roots and the `bigdecimal_exp`, `bigdecimal_ln`, `bigdecimal_log10` and
`bigdecimal_log` functions accept an explicit significant-digit count and
rounding mode. Logarithm arguments must be positive; an explicit logarithm
base must also be positive and different from one.
