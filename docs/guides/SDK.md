# NumForge SDK: start here

Download the matching SDK archive and `.sha256`, verify, then extract the whole
folder. Published 2.4.0 SDKs are numeric-only; newer source SDKs add modules and
the optional expression engine. Installed headers/BUILDINFO.json define the
package scope. Application/session/HTTP internals are not SDK interfaces.

## Choose a compatible toolchain

- Windows x64: compatible MSVC Release static library and `/MD`. Match
  BUILDINFO.json and install the corresponding Visual C++ runtime. MinGW,
  `/MT` and `/MDd` need a matching source build; never mix CRT ownership.
- Linux x64: GCC/glibc, with the recorded build glibc or newer; not Alpine/musl.
- Other compilers/architectures/instrumentation: build from source.

Development needs CMake 3.20+ and a C/C++ compiler, not Python, Node or the source
tree. The linked library needs no NumForge DLL.

## Build the included example

In the extracted folder (MSVC developer terminal on Windows):

```sh
cmake -S example -B example-build -DCMAKE_BUILD_TYPE=Release
cmake --build example-build --config Release
ctest --test-dir example-build -C Release --output-on-failure
```

For Visual Studio add `-A x64 -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL`
to configure. The example locates its sibling SDK. Newer source packages enable
additional module/engine examples according to installed headers/targets.

## Use in your project

```cmake
find_package(NumForge CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE NumForge::numforge)
```

Configure with `-DCMAKE_PREFIX_PATH=/absolute/sdk`. Include public headers;
CMake supplies paths/libraries and C++ linkage. Use `NumForge::engine` plus
`<numforge/engine.h>` for expressions; it transitively links numeric code.

Check statuses, destroy owned handles and `free()` returned strings. The archive
contains include/, lib/exports, examples, guide, LICENSE and BUILDINFO.json.
Full source/docs come with the matching download.
[C API](https://github.com/Interacti0n/NumForge/blob/main/docs/reference/C_API.md) ·
[Source installation](https://github.com/Interacti0n/NumForge/blob/main/docs/guides/LIBRARY_GUIDE.md).
