# Documentation guide

Choose an entry point; detailed contracts and procedures live in their own guide.

| Task | Start here |
| --- | --- |
| Use the browser calculator | [Calculator](guides/CALCULATOR_WEB.md), [variables](guides/VARIABLES.md), [graphs](guides/GRAPHS.md), [equations](guides/EQUATIONS.md), [units](guides/UNIT_CONVERTER.md) |
| Use the library | [C/C++ library guide](guides/LIBRARY_GUIDE.md), [SDK](guides/SDK.md), [Python](guides/PYTHON.md) |
| Look up supported operations | [API index](API.md), then the linked `reference/` contracts |
| Extend the platform | [Architecture](design/ARCHITECTURE.md), [contributing](../CONTRIBUTING.md), [feature checklist](project/FEATURE_CHECKLIST.md) |
| Validate a change | [Testing](TESTING.md), [numerical methodology](testing/NUMERIC_TESTING.md) |
| Measure performance | [Benchmark tools](../benchmarks/README.md), [measurement methodology](testing/PERFORMANCE.md) |
| Plan or publish a release | [Roadmap](project/ROADMAP.md), [release packaging](project/RELEASE_PACKAGING.md), [changelog](../CHANGELOG.md) |
| Deploy remotely | [Hosting](guides/HOSTING.md) |

`design/` explains implementation decisions. `reference/` describes supported
interfaces and mathematical semantics. Public C headers contain the authoritative
parameter, ownership and failure contracts; guides link to them instead of copying
declarations. [Web assets](../apps/web/assets/README.md) describes browser sources.

Dated files under `benchmarks/`, `testing/` and `project/` preserve measurements
and release evidence. They describe the recorded revision, not current guarantees.
Future proposals, such as [accounts](design/ACCOUNT_DESIGN.md), are explicitly
labelled and do not establish implemented functionality.

Keep one canonical home for each contract or procedure. Link from other pages;
remove obsolete proposals when their decisions have been absorbed into current
documentation. Keep reproducible benchmark evidence. Generated files and local
measurements belong under ignored `build/` directories. Check links with
`node scripts/check_docs.cjs` from any working directory.
