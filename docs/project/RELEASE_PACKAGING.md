# Portable application releases

GitHub's automatic source ZIP/tar.gz downloads contain code, not runnable
applications. The portable binary workflow adds these separate assets to an
existing release after platform tests and extracted-archive checks succeed:

- `NumForge-X.Y.Z-win-x64.zip`
- `NumForge-X.Y.Z-linux-x64.tar.gz`
- A `.sha256` checksum file beside each archive.

The workflow is prepared in `.github/workflows/release-binaries.yml`; creating
the workflow locally does not publish any downloads. It must be committed and
pushed before it can run on GitHub. New release tags must include the workflow
and helper; manual dispatch is available from the default branch. Existing
release assets are not overwritten and existing tags are not moved.

## What users download

Each archive extracts into a folder with the same version/platform name:

```text
NumForge-X.Y.Z-win-x64/
  numforge_web.exe
  calculator.exe
  START_HERE.txt
  LICENSE
  CHANGELOG.md
  CHANGELOG_SHORT.md
  BUILDINFO.json
```

The Linux folder contains `numforge_web` and `calculator` without `.exe`.
Users extract the entire archive, then double-click `numforge_web.exe` on Windows
or run `./numforge_web` from a Linux terminal. The browser opens the calculator
on loopback; its server process stays running until closed. CLI users launch
`calculator.exe` or `./calculator`. A compiler, CMake, Node.js, Python and a
database are not needed on the user's machine. Web assets are embedded.

The application archive is separate from the source/library distribution: it
does not ship headers, compiler-specific static libraries or developer builds.
Library consumers continue to use the source release and CMake install workflow.

## Version identity

Always build the exact release tag. `NumForge-2.1.0-*` must be produced from
`v2.1.0`, and a 2.0.0 backfill must still use `v2.0.0`. Binary downloads must
match GitHub's source archives. The development tree declares 2.1.0; that does
not mean the release has been published. See the [2.1 checklist](RELEASE_2_1.md).

Packaging automation is checked out separately from release source. This permits
backfilling binary assets for an older tag without changing that tag. The helper
validates the CMake source version, build source path, production configuration
and binary architecture; `BUILDINFO.json` records the exact commit, compiler,
configuration and runtime dependencies. Builds are reproducible procedures, not
a guarantee of byte-identical archives across toolchain versions and timestamps.

Tags use `vX.Y.Z`, optionally with a prerelease suffix such as `v2.1.0-rc.1`.
Prerelease archives retain that suffix in their filenames and build metadata;
the numeric `X.Y.Z` part must match the CMake project version.

## Build, verify and publish

1. Commit/push the packaging workflow and helper before tagging a new release.
   Update the CMake project version to match the planned tag's numeric version.
2. Publish a GitHub release for that tag. The `release: published` event starts
   packaging automatically, including releases published from drafts and
   prereleases. Saving a draft or pushing a tag alone does not start packaging.
3. Windows and Linux jobs check out the published tag, build Release tests and run CTest.
   They then create separate production builds with tests/benchmarks disabled.
4. Each package is archived, extracted and checked from the extracted folder:
   CLI decimal/integer/fraction results, HTTP fraction evaluation and embedded
   calculator assets. The workflow saves the resulting archives/checksums as
   Actions artifacts for inspection.
5. Both platforms must succeed before the upload job automatically attaches
   the verified files to that release. The upload job checks SHA-256 and uses
   GitHub CLI; missing releases or duplicate asset names fail rather than silently
   creating a release or replacing downloads.

Only the publishing job has `contents: write`; build jobs have read access.
Runs for the same tag are serialized; a new run does not cancel an active one.
Binaries appear after packaging completes, so source downloads may be available
first. Failed builds leave the release's existing assets intact; inspect the
Actions logs before retrying. Existing files are never silently replaced.

### Manual dry runs, backfills and retries

In GitHub Actions, choose **Release application binaries**, enter an existing
tag (for example `v2.1.0`) and leave **publish** unchecked for a dry run. Archives
and checksums are saved as Actions artifacts. Run with **publish** checked to
attach both packages to the existing release. This also supports older tags
that predate the workflow. Duplicate asset names fail; if an upload partially
succeeds, reconcile those assets before retrying instead of overwriting them.

If another Actions workflow publishes releases using the repository's
`GITHUB_TOKEN`, GitHub does not trigger another workflow from that event.
That future release-creation workflow must explicitly dispatch packaging or use
a suitable GitHub App/token. Publishing through the GitHub UI triggers it normally.

## Compatibility

Windows builds use x64 MSVC Release with `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`
to link the C runtime statically. The helper inspects imports and rejects
compiler-runtime dependencies that would require additional DLLs. Windows system
DLLs remain required. Binaries are unsigned; code signing and reputation handling
are separate future distribution improvements.

Linux x64 is built on the pinned Ubuntu 22.04 runner, rather than whichever
distribution `ubuntu-latest` selects in the future. The helper checks `ldd`,
records imported libraries and extracts the highest required GLIBC symbol version
into `BUILDINFO.json` and `START_HERE.txt`. Users need a compatible glibc system
at or above that recorded version; this is not an Alpine/musl archive or a fully
static Linux build. Binary checks run on the build runner; broader distro testing
can be added as compatibility requirements develop.

Keep ARM64 and macOS packages separate until those exact archives are built and
tested; do not label an x64 build as universal. Installers, automatic updates and
code signing can follow later. Portable archives are the first distribution path.

## Local packaging

For current 2.1 verification, see [Release 2.1 preparation](RELEASE_2_1.md).
The following 2.0 validation is historical, not evidence for a new release.

Local validation on 2026-10-07 built the exact `v2.0.0` source with MSVC
19.51 and a static Release runtime: all 28 CTest tests passed. The Windows
archive passed extracted CLI/HTTP/embedded-asset checks and imported only Windows
system DLLs. Existing-archive preservation and version-mismatch rejection were
also checked. Linux validation and GitHub asset publication are still pending;
the local Windows archive is an ignored build output, not a published release.
The automatic-release update passed local YAML parsing, trigger/dependency
checks, Bash syntax and valid/invalid tag checks. A prerelease-named Windows
archive also passed extraction checks and retained its suffix in build metadata.
The GitHub-hosted end-to-end run is still pending.

The helper requires Python 3.12 (CI) or a Python version with tar extraction
filters; Windows ZIP packaging was checked with Python 3.11.9. Configure a clean
production Release build of the exact source tag, with `NUMFORGE_BUILD_APPS=ON`,
`BUILD_TESTING=OFF`, `NUMFORGE_BUILD_BENCHMARKS=OFF`. Set the static MSVC runtime
on Windows. Then run, substituting the actual source/build/commit paths:

```sh
python scripts/package_apps.py --source release-source --build build-apps \
  --output build/portable-output --version 2.1.0 \
  --commit FULL_SOURCE_COMMIT_SHA --platform win-x64
```

Use `--platform linux-x64` on Linux and include a Release build type. Existing
package names and extraction directories are preserved: use a fresh output path
for each attempt. SHA-256 files are emitted only after smoke verification succeeds.

Reference: [manual workflow runs](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow),
[release events](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#release),
[release uploads](https://cli.github.com/manual/gh_release_upload), and
[CMake MSVC runtime selection](https://cmake.org/cmake/help/latest/variable/CMAKE_MSVC_RUNTIME_LIBRARY.html).
