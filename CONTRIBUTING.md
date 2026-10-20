# Contributing

Build and run tests with `cmake -P cmake/LocalBuild.cmake` using a C17 compiler,
CMake 3.20+ and Git. The first test build fetches pinned Unity. Node.js enables
independent numeric, architecture, CLI and web checks.

## Make a focused change

1. Choose an owner using the [architecture](docs/design/ARCHITECTURE.md).
   New mathematical capabilities follow the [feature checklist](docs/project/FEATURE_CHECKLIST.md).
2. Keep mathematics in C and adapters thin. Use public opaque APIs across module
   boundaries; register sources explicitly in their owning CMake manifest.
3. Preserve exactness, rounding, aliasing and failure guarantees. Use independent
   references and relevant edge/high-precision and allocation/deadline tests.
4. Run the [checks matching the change](docs/TESTING.md), plus
   `node scripts/check_architecture.cjs`, `node scripts/check_docs.cjs` and
   `git diff --check`. Public API changes also need installed consumer checks.
5. Describe the problem, resulting behavior and validation in the PR. Update the
   [single changelog](CHANGELOG.md) for user-visible changes.

Use repository-qualified private includes and `<numforge/...>` public includes.
Comments explain invariants, ownership and non-obvious numerical choices; avoid
restating code. Edit browser sources under `apps/web/assets/` and rebuild the
embedded assets; keep Slovak and English behavior aligned.

Link to canonical [documentation](docs/project/DOCUMENTATION.md) instead of
copying contracts or test inventories. Keep routine builds, traces and recordings
under ignored `build/`; reusable benchmark tools belong in `benchmarks/`.
An optimization needs matching, independently validated Release measurements.

Report bugs through [GitHub issues](https://github.com/Interacti0n/NumForge/issues)
with version, OS/compiler, minimal input, expected reference and actual output.
