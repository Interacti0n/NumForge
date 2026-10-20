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

`design/` explains implementation decisions. `reference/` describes supported
interfaces and mathematical semantics. Public C headers contain the authoritative
parameter, ownership and failure contracts; guides link to them instead of copying
declarations. [Web assets](../apps/web/assets/README.md) describes browser sources.
The Python guide owns installation, shared input/result policy and short workflows;
module references own their C/Python operation details. The Python package README
is a self-contained introduction for package metadata, not another full reference.

Selected baselines under `performance/` and release verification under `project/`
describe their recorded revision, not current guarantees. Superseded working
reports and intermediate result files remain available in Git history.
Future proposals, such as [accounts](design/ACCOUNT_DESIGN.md), are explicitly
labelled and do not establish implemented functionality.

Keep one canonical home for each contract or procedure. Link from other pages;
remove obsolete proposals when their decisions have been absorbed into current
documentation. Keep reproducible benchmark evidence. Generated files and local
measurements belong under ignored `build/` directories. Check links with
`node scripts/check_docs.cjs` from any working directory.

Long references are intentional when they contain distinct domains, precision or
failure rules. Start with a guide; follow only the operation contract you need.
Generated C declarations come from the public headers, not another maintained
copy. The [documentation workflow](project/DOCUMENTATION.md) explains generation,
example checks and where new material belongs.

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
