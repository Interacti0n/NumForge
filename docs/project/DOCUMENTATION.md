# Documentation workflow

## Put information in one place

| Material | Owner | Keep elsewhere as |
| --- | --- | --- |
| Project purpose and first build | Root README | Short links from other entry points |
| User/developer workflows | `docs/guides/` | API links, not copied operation contracts |
| C parameters, statuses, precision and ownership | Installed `include/numforge/` headers | Generated Doxygen declarations |
| Mathematical meaning and interface-specific rules | `docs/reference/` | Links from guides/API index |
| Implementation choices and invariants | `docs/design/` | Links to current contracts |
| Verification commands and methodology | `docs/TESTING.md`, `docs/testing/` | Dated evidence links |
| Benchmark executable/options | `benchmarks/README.md` | Links to methods and measured reports |
| Plans and release procedure | `docs/project/` | Roadmap status and links |
| Selected measurements and algorithm decisions | [Performance evidence](../performance/README.md) | Date/revision-qualified baselines and supporting data |
| Published release outcomes | [Release verification](RELEASES.md) | Feature chronology in the changelog |

Current source and published packages can differ. Mark proposals explicitly;
preserve relevant historical failures in selected comparisons even after fixes.
Superseded diaries remain in Git history, rather than accumulating as active pages.
Generated catalogue
tables and distinct API domains can be long; avoid shortening away failure,
precision or ownership rules. A short introduction may repeat context, not the
full contract.

## Build locally

Build from the full source checkout/archive, rather than the minimal Python
package source distribution. Documentation tools are optional and independent of the library build. Use
Python 3.10+, MkDocs from `docs/requirements.txt` and Doxygen 1.9+ on PATH.
Create a dedicated virtual environment and install the requirements there:

```sh
python -m venv build/docs-venv
build/docs-venv/bin/python -m pip install -r docs/requirements.txt
build/docs-venv/bin/python scripts/build_docs.py
```

In PowerShell use `build/docs-venv/Scripts/python.exe` for those two commands.
If Doxygen is elsewhere, append `--doxygen /absolute/path/to/doxygen`.
The helper does not install tools, open a server, push or publish.

Output is `build/docs/site/index.html`: guides/references plus linked generated
C declarations under `c-api/`. The Markdown source remains browsable in the
repository. The helper stages existing pages and linked supporting files, keeping
their relative paths. It replaces only its dedicated `build/docs/content`, `c`
and `site` outputs. Do not store hand-written material in those generated folders.

MkDocs navigation is maintained in the root `mkdocs.yml`; every maintained page
has one canonical entry. Start with user tasks: calculator, integration and
development. Nest API contracts under integration and implementation/testing/
performance details under development. The start page offers the shortest path
to a first result; it is not a file inventory. Extend an existing topic before
adding a page or another top-level menu group.
Doxygen reads installed headers only, never private implementation sources.
`EXTRACT_ALL` includes older declarations whose prose predates Doxygen tags;
their source views retain those comments. Successful generation does not certify
complete parameter documentation. Improve annotations when changing an API,
following the [feature checklist](FEATURE_CHECKLIST.md).

Configuration options follow the [MkDocs configuration reference](https://www.mkdocs.org/user-guide/configuration/)
and [Doxygen manual](https://www.doxygen.nl/manual/config.html). No external theme
or hosted service is required.

## Verify edits

1. Run `node scripts/check_docs.cjs` for repository links/anchors.
   `python tests/test_docs.py` checks source staging, bounded cleanup and complete
   navigation coverage in isolated fixtures under `build/`.
2. Run `python scripts/build_docs.py` for strict rendered links and C doc warnings.
3. Run affected examples against installed packages:

   ```sh
   python scripts/check_doc_examples.py --prefix /path/to/sdk --output build/docs-c-examples
   python scripts/check_doc_examples.py --python-site build/python-package-check/site-packages
   ```

   The C check compiles the complete library/engine examples and requires an
   engine-enabled SDK plus a fresh output directory. The Python check executes
   the guide/module examples and verifies the installed module origin. Use the
   [package check](../TESTING.md#installed-packages-and-python) to prepare that wheel.
   Partial snippets are usage illustrations, not standalone programs. Never
   present a future interface as an existing callable API.
4. Inspect navigation, tables, code and images in the generated pages. Keep the
   generated site/tools and scratch measurements out of commits.

For a new page, add its canonical navigation entry. Prefer extending an existing
topic over adding another progress report. Apply the
[performance retention policy](../performance/README.md#retention-policy) to measurements.
Preserve existing section anchors when possible. If a topic moves, update links
or retain a small section pointing to its new owner. Update the roadmap only when
the documented completion gate is actually met.

The documentation CI workflow checks staging/navigation, builds both outputs and
compiles the C examples. Python package CI executes its examples against the
isolated installed wheel. CI configuration is not a claim of a completed remote
run; all build output remains unpublished until a separate release action.

## Product screenshots

Use real product captures and captions stating their version. Current README
images show the 2.1.0 preparation build, captured on 8 October 2026 in English
with Full precision and Fraction notation. Refresh them after relevant UI changes:

```sh
node tests/browser/capture_showcase.js build/Release/numforge_web.exe
```

Pass the actual executable path; single-configuration builds omit `Release/`.
The script starts its own loopback server on port 18773, checks the calculator
results, captures desktop/mobile layouts and closes its browser/server. Inspect
both images for clipping and legibility. Keep demonstration claims consistent
with the current API and distinguish published archives from development source.
