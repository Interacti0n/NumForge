# NumForge SDK: start here

Use the `sdk-win-x64.zip` or `sdk-linux-x64.tar.gz` release asset to link the
public numeric C API from C or C++. Download its matching `.sha256` file and
verify the checksum before extracting the whole archive. This SDK contains
BigInt, BigDecimal, BigRational, units and runtime APIs, not the calculator
parser, sessions or HTTP server. For HTTP clients, use the separate application
download and its documented HTTP API.

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

Check return statuses, destroy owned numeric objects with their matching
`*_destroy()` functions and free returned allocated strings with `free()`.
See the version-matched online [C API reference](https://github.com/Interacti0n/NumForge/blob/v2.2.0/docs/reference/C_API.md)
and [library guide](https://github.com/Interacti0n/NumForge/blob/v2.2.0/docs/guides/LIBRARY_GUIDE.md).

The SDK includes only `include/`, `lib/` (with CMake exports), `example/`, this
guide, `LICENSE` and `BUILDINFO.json`. Source and full documentation are available
from the same GitHub release. The library is licensed under MIT.
