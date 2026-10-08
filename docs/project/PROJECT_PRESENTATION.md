# Project presentation and maintenance

The README should let a visitor understand the project quickly, see real
behavior and find a reliable way to try it. Public presentation must describe
implemented capabilities and link to the detailed contracts rather than imply
that roadmap items already work.

## Presentation assets

| Item | Where it lives / current state |
| --- | --- |
| Hero | README title, existing `web/logo.png`, concise positioning and navigation links. |
| Quick demo | README's verified exact decimal, integer, fraction/root and high-precision examples. |
| Status badges | Live GitHub CI and latest-release badges; MIT/C17/CMake requirements verified from the repo. |
| Screenshots | `docs/images/calculator-desktop.png` and `calculator-mobile.png`, captured from the real local Release server. |
| Architecture | README Mermaid diagram and `docs/design/ARCHITECTURE.md` with ownership and client/library boundaries. |
| Benchmarks | README excerpt of recorded results; detailed methodology and experiments remain in the benchmark docs. |
| Getting started | Source release/clone links, CMake build and platform-specific application launch commands. |
| Contribution path | Root `CONTRIBUTING.md`, testing guide and GitHub issue link. |
| Capability/roadmap distinction | `ROADMAP.md` names implemented functionality and informational UI routes. |
| GitHub metadata | Description and topics were updated separately on GitHub; Git commits do not set these fields. |

Screenshots show the 2.1.0 preparation build, captured on 8 October 2026
in English with Full precision and Fraction notation. The capture records four
confirmed expressions: `0.1+0.2`, `2^128`, `sqrt(4/9)` and `1/3+1/6`. The large
integer history entry uses full Auto scientific output before switching to
Fraction. The last result is exactly `1/2` with a separate `≈ 0.5` hint.

## Refresh screenshots

Build the server after web changes. Install the browser-test dependencies and
Chromium using the process in [TESTING.md](../TESTING.md), then run:

```sh
node tests/browser/capture_showcase.js build/Release/numforge_web.exe
```

For single-configuration builds, pass `build/numforge_web` instead. Use the actual
path for a separate local build. The script starts its own server on loopback
port 18773, verifies results and history, captures 1600×1000 desktop and 390-pixel
mobile layouts using headless Chromium, and closes its browser/server afterward.
It does not change a running user server. Leave the port unused before capture.

Keep images as real product captures, without fabricated numbers or mock controls.
Inspect both images after capture for clipping, stale state and readable text.
No personal data, credentials or browser chrome should be included. The mobile
image is a full-page capture, displayed under a collapsible README section to
keep the opening compact. Keep alt text meaningful and captions honest about the
development version shown.

## Keep claims current

- Live CI/release badges resolve on GitHub. A badge is not a substitute for
  checking the relevant workflow or release; MIT/C17/CMake labels are static.
- Update demo examples when syntax or display settings change. Verify them with
  the real calculator, including the precision/notation needed for the output.
- Benchmark summaries must include machine/compiler and repeated-run context.
  Do not present private experiments as shipped optimizations, or compare
  mismatched precision and input sizes.
- Keep future UI routes and public-hosting limitations visible in the roadmap.
- Update the GitHub description/topics manually when the product scope changes.
  The current description covers C17 integers, exact decimals, rational numbers,
  scientific functions and optional CLI/browser calculators. Topics include the
  numeric types, arbitrary precision, numerical computing, calculator and CMake.

## Remaining presentation work

- Prebuilt binary downloads: source release exists; Windows/Linux portable
  packaging runs on release publication once the workflow is committed/pushed;
  see [RELEASE_PACKAGING.md](RELEASE_PACKAGING.md). The
  downloads are not published yet.
- Hosted demo: local build-and-run path exists; deployment and isolation design
  are still needed before offering a public calculator.
- More benchmark highlights: direct BigDecimal/higher-function measurements are
  implemented in [DECIMAL_MATH_BENCHMARKS.md](../benchmarks/DECIMAL_MATH_BENCHMARKS.md). Add only
  verified results with their recorded machine/range context; retain the stress
  matrix's explicit timeouts and tangent-pole accuracy limitation.
- Portfolio connection: deliberately deferred at the owner's request. No
  portfolio links, deployment or integration were added.

Commit and push presentation changes to update the public README. After
publishing them, check GitHub's Markdown, Mermaid and image rendering as well as
the live badge destinations.
