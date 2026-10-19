# NumForge SDK: start here

Use the `sdk-win-x64.zip` or `sdk-linux-x64.tar.gz` release asset to link the
public numeric C API from C or C++. Download its matching `.sha256` file and
verify the checksum before extracting the whole archive. This SDK contains
BigInt, BigDecimal, BigRational, complex, units and runtime APIs. SDKs built from
the current post-2.4.0 source also contain exact polynomial solving and the optional public expression engine,
not sessions or the HTTP server. Older published archives retain their original
numeric-only scope. For HTTP clients, use the separate application download and
its documented HTTP API.

## Choose a compatible toolchain

- **Windows x64:** MSVC Release static library, built with the dynamic C runtime
  `/MD`. Use an x64 MSVC toolchain compatible with the compiler version in
  `BUILDINFO.json` and `/MD` in your application. This is not a MinGW library.
  `/MT` and Debug `/MDd` builds need a matching library built from source.
  Running your application requires the corresponding Microsoft Visual C++
  runtime. Do not mix CRTs: returned strings are freed with your `free()`.
- **Linux x64:** GCC Release static library for glibc systems. The GCC version
  and build glibc are recorded in `BUILDINFO.json`. Use the build glibc version
  or newer as the supported baseline; this is not an Alpine/musl SDK.
- Other architectures, compilers, runtime choices and instrumented builds:
  build from the release source with CMake instead.

You need CMake 3.20+ and a C/C++ compiler to develop with the SDK. Node.js,
Python and the NumForge source tree are not needed to build the included example.

## Build the included example

Open a terminal in the extracted SDK directory. Use a Visual Studio developer
terminal on Windows:

```sh
cmake -S example -B example-build -DCMAKE_BUILD_TYPE=Release
cmake --build example-build --config Release
ctest --test-dir example-build -C Release --output-on-failure
```

The included example locates the SDK beside its own directory. On Windows,
add `-A x64 -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL` to the configure
command when using the Visual Studio generator. Build with `--config Release`.
The example prints `0.1 + 0.2 = 0.3` using exact decimal arithmetic.

When `NumForge::engine` is available, the example project also builds and tests
`numforge_engine_example`, evaluating sqrt(2) at 1000 working significant digits.
See `engine.h` and the standalone engine contract in the full source documentation.
The project also builds a typed-parameter example and, when `polynomial.h` is
present, a standalone exact polynomial example linked only to the numeric library.
When `matrix.h` is present, `numforge_matrix_example` checks an exact determinant
and rational linear-system solution using only the numeric target.
`numforge_linear_system_example` demonstrates exact rank, free coordinates and a
complete rectangular solution family from the same public matrix header.
When `statistics.h` is present, `numforge_statistics_example` checks an exact
recurring mean and population/sample variances from the numeric library.

## Use in your project

```cmake
find_package(NumForge 2.2 CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE NumForge::numforge)
```

Configure your project with `-DCMAKE_PREFIX_PATH=/absolute/path/to/extracted-sdk`.
Include public headers such as `<numforge/bigint.h>` or
`<numforge/bigdecimal.h>`. CMake supplies include paths and the library; the
export is relocatable. C++ headers provide C linkage automatically. A static
library is linked into your application; there is no NumForge DLL to copy.

For stateless scalar expressions, link `NumForge::engine` instead and include
`<numforge/engine.h>`. It transitively links the numeric library. The public
engine supports typed results and reusable expressions without a server.

Current source builds also provide `<numforge/numerical.h>` in
`NumForge::numforge` for [callback-based root search](../reference/NUMERICAL_API.md).
The `example/numerical.c` project needs no expression engine. Published 2.4.0
archives predate this addition.

Check return statuses, destroy owned numeric objects with their matching
`*_destroy()` functions and free returned allocated strings with `free()`.
See the version-matched online [C API reference](https://github.com/Interacti0n/NumForge/blob/v2.4.0/docs/reference/C_API.md)
and [library guide](https://github.com/Interacti0n/NumForge/blob/v2.4.0/docs/guides/LIBRARY_GUIDE.md).

The SDK includes only `include/`, `lib/` (with CMake exports), `example/`, this
guide, `LICENSE` and `BUILDINFO.json`. Source and full documentation are available
from the same GitHub release. The library is licensed under MIT.
