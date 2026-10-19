# Initial Python adapter verification — 19 October 2026

The initial adapter links the public C numeric/engine targets into a CPython
extension. Python handles input/result conversion and object ownership; arithmetic,
principal roots, expression evaluation and exact polynomial solving stay in C.
Package version: 0.1.0.dev0; linked core version: 2.4.0 development source.

## Local verification

Windows x64, CPython 3.12.14, MSVC 19.51.36247, C17 Release `/MD`, warnings as errors.
The extension also passed GCC 16.2 C17 syntax/warning checks against the local
Python headers; that is not a second runtime-platform validation.

- Native adapter build and direct smoke calls: passed.
- All 22 Python tests: passed, including independent Decimal sqrt(2) at 1000
  digits, six rounding modes, lossless integer/decimal/rational inputs,
  exact/approximate provenance, explicit parameters, aliases/lifetime, UTF-8
  diagnostics, resource failure recovery and exact equation descriptors.
- `scripts/check_python_package.py`: passed source-distribution creation,
  extraction, wheel build from the extracted source, isolated target installation,
  module-origin assertions, copied tests and the standalone example.
- The source distribution includes the C unit catalogue `.inc` dependency and
  excludes build output. The wheel contains the Python facade, the linked native
  extension and package/license metadata; no NumForge DLL or source tree is needed
  at runtime. The initial Windows wheel is approximately 135 KB compressed.
- Existing C/application Release regression: **64/64 CTests passed** after the
  optional build integration. Python remains disabled in ordinary C builds.
- Documentation links and whitespace checks passed.
- A final nested archive build exposed MSBuild's path-length limit. The native
  cache now uses a shorter interpreter-specific path, and the nested archive
  build was repeated with that layout.

The package checker uses a fresh directory under build/, an installed target
directory and `python -I`; it asserts both facade and extension originate from
the installed directory. It does not install globally or publish artifacts.

## CI and remaining scope

The new Python workflow is configured for Ubuntu 24.04 / CPython 3.10 and 3.12,
Windows 2022 / 3.12 and macOS 15 / 3.12. Those remote jobs have not run as part of
this local verification. Broad wheel publication and additional interpreter/ABI
coverage remain release work.

Native calls retain the GIL. Free-threaded/PyPy execution, parallel native calls,
instant Ctrl-C cancellation, Python arithmetic operators, numerical interval roots,
quantities and persistent sessions are outside this initial adapter. Existing
cooperative budgets and approximation limitations remain unchanged. No new
mathematical algorithm or performance improvement is claimed.

See the [Python guide](../guides/PYTHON.md) for the actual API and reproduction
command; [the architecture](../design/ARCHITECTURE.md) records subsequent work.
