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
| Build or maintain documentation | [Documentation workflow](project/DOCUMENTATION.md) |
| Inspect measured results | [Performance evidence](performance/README.md) |
| Check published release verification | [Release history](project/RELEASES.md) |

Public C headers own parameters, ownership and error contracts. Guides explain
workflows; references explain operation meaning; design pages explain decisions.
Keep one owner and link from elsewhere. Selected measurements describe their
recorded revision, while superseded diaries remain in Git history. New pages
follow the [documentation workflow](project/DOCUMENTATION.md).

## File tree

```text
docs/
  README.md, API.md, TESTING.md   entry points
  guides/                       user and integration workflows
  reference/                    mathematical and interface contracts
  design/                       implementation decisions and proposals
  testing/                      verification procedures
  performance/
    README.md                   evidence index and retention rules
    protocols/                  how each benchmark measures work
    baselines/                  selected results, decisions and limits
    data/                       CSV evidence used by those baselines
  project/                      roadmap, contribution/release/docs rules,
                                structural audit and release verification
  images/                       real product screenshots
```

Executable benchmark tools and reusable input/reference fixtures live in the
repository's `benchmarks/`; routine output, generated documentation and local
reports go under ignored `build/`.
