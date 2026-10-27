# Documentation workflow

## Put information in one place

| Content | Owner |
| --- | --- |
| Purpose, first build | Root README |
| User workflows | `docs/guides/` |
| C parameters/statuses/ownership | Installed headers → Doxygen |
| Mathematical meaning and transport rules | `docs/reference/` |
| Implementation invariants | `docs/design/` |
| Verification procedure | `docs/TESTING.md`, `docs/testing/` |
| Benchmark options / selected evidence | `benchmarks/README.md` / `docs/performance/` |
| Plans, completion gates, packaging | `docs/project/` |

Link to the owner instead of copying contracts. Keep domains, precision, failure
and ownership rules; remove diaries and obsolete plans. Mark proposals and
historical measurements explicitly. Current source can differ from downloads.

## Build locally

Requires the full checkout, Python 3.10+, Doxygen 1.9+ and optional doc dependencies:

```sh
python -m venv build/docs-venv
build/docs-venv/bin/python -m pip install -r docs/requirements.txt
build/docs-venv/bin/python scripts/build_docs.py
```

PowerShell: use `build/docs-venv/Scripts/python.exe`. Supply `--doxygen PATH`
if needed. Output: `build/docs/site/index.html`; only generated content/c/site
children are replaced. Keep hand-written files elsewhere.

`mkdocs.yml` gives each maintained page one canonical entry, grouped by user
task; [reader styles](../assets/reader.css) bound long code/tables on small screens.
Extend an existing topic before adding a page. Doxygen reads public headers;
successful generation does not prove every older declaration is fully annotated.

## Verify edits

```sh
node scripts/check_docs.cjs
python tests/test_docs.py
python scripts/build_docs.py
python scripts/check_doc_examples.py --prefix /path/to/sdk --output build/docs-c-examples
python scripts/check_doc_examples.py --python-site build/python-package-check/site-packages
```

Use an engine-enabled installed SDK, an [isolated wheel](../TESTING.md#installed-packages-and-python)
and fresh example output directories. Preserve anchors or update their links.
Inspect rendered navigation/tables/code. Keep generated sites and scratch results
out of Git; retain only [useful measurement evidence](../performance/README.md#retention-policy).
CI checks staging, generation and installed examples; configured jobs are not
proof that local changes passed remotely.

## Product screenshots

Use real captures, identify development source versus published packages and
refresh affected views after UI changes:

```sh
node tests/browser/capture_showcase.js build/dev/Release/numforge_web.exe
```

Adjust the executable path for the generator. The script owns port 18773 and
closes its browser/server. Inspect clipping, labels and result accuracy; keep
only maintained images. Dates/captions belong beside the images, not here.
