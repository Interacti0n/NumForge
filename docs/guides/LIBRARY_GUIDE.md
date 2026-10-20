# Using the NumForge library

Use this guide to build and consume the current source. For a prebuilt archive,
start with the [SDK quick start](SDK.md). Published 2.4.0 archives have a smaller
API than current source; installed headers are authoritative for that package.

## Build and install

Requires CMake 3.20+ and a C17 compiler. This builds the numeric library and
optional expression engine, without applications or test dependencies:

```sh
cmake -S . -B build-library -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DNUMFORGE_BUILD_APPS=OFF \
  -DCMAKE_INSTALL_PREFIX=/your/prefix
cmake --build build-library --config Release --parallel
cmake --install build-library --config Release
```

For the numeric library alone, add `-DNUMFORGE_BUILD_ENGINE=OFF`. Testing,
applications, Python, fuzzers and benchmarks must also be disabled for that
configuration; they are clients of the engine. Ordinary numeric consumers need
no application sessions, browser assets, Node.js, Python or database.

## CMake consumer

```cmake
find_package(NumForge CONFIG REQUIRED)
add_executable(example example.c)
target_link_libraries(example PRIVATE NumForge::numforge)
```

Configure with `-DCMAKE_PREFIX_PATH=/your/prefix`. The target supplies include
paths and the static library. For expressions, link `NumForge::engine` instead;
it transitively links the numeric library. Both targets support C++ via the
public headers' C linkage. Match compiler, architecture and C runtime when
consuming a prebuilt static library.

## Choose an interface

| Task | Contract |
| --- | --- |
| Integer, decimal or rational operations | [Numeric C API](../reference/C_API.md) |
| Complex values and principal functions | [Complex API](../reference/BIGCOMPLEX.md) |
| Expressions and prepared parameters | [Engine API](../reference/ENGINE_API.md) |
| Polynomial, matrix, statistics, units or numerical methods | [API index](../API.md) |
| Python | [Python guide](PYTHON.md) |

Use typed numeric/module APIs when inputs are already values. Expression parsing
is optional. Precision units, domains, aliasing, ownership and failure rules
belong to each installed header and its linked reference, not this build guide.

## Ownership and failures

Create owned values with `*_create()` and destroy them with their matching
`*_destroy()` function. Free successful allocated strings with `free()`.
Check every returned status. The API reference documents exceptions to general
aliasing/failure rules and the optional cooperative runtime budgets.

## Example

This complete program computes sqrt(2) at 40 working significant digits and
formats ten decimal places. Stored approximation and display precision differ:

```c
#include <numforge/bigdecimal.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    BigDecimal *value = bigdecimal_create();
    char *text = NULL;
    int outcome = 1;
    if (value == NULL) return outcome;
    if (bigdecimal_set_string(value, "2") != BIGDECIMAL_OK ||
        bigdecimal_sqrt(value, value, 40, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_format(value, 10, BIGDECIMAL_ROUND_HALF_EVEN, &text) != BIGDECIMAL_OK)
        goto cleanup;
    puts(text); /* 1.4142135624 */
    outcome = 0;
cleanup:
    free(text);
    bigdecimal_destroy(value);
    return outcome;
}
```

More complete examples live in `examples/sdk/`. The installed C/C++ consumer and
SDK checks described in [testing](../TESTING.md) exercise these interfaces
outside the repository.
