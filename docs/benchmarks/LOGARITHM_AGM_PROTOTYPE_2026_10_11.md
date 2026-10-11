# Theta/AGM logarithm prototype — 11 October 2026

Historical prototype snapshot: the subsequent
[bounded interval integration](LOGARITHM_AGM_INTEGRATION_2026_10_11.md) implements
a separate production path. The fixed-guard experiment described below remains
benchmark-only; its timings and limitations are not those of the bounded path.

This implements the first experiment from the
[logarithm algorithm review](LOGARITHM_ALGORITHM_REVIEW_2026_10_11.md).
It lives entirely in the opt-in `logarithm_agm_benchmark` executable. Production
`bigdecimal_ln`, complex functions, the calculator and HTTP endpoints retain
their existing algorithms. No public C API, release version or limit changes.

## Experimental path

The prototype evaluates the Sasaki–Kanada theta/AGM identity described in the
review, using the existing per-call significant-arithmetic workspace, sqrt and
dynamic pi factory. Pi calculation is included on every high-precision call;
there is no shared cache or precomputed high-precision pi fixture.

It expands the positive argument by squaring until the omitted theta powers
are small, computes the short sparse theta series, iterates arithmetic and
geometric means, divides pi by the resulting mean and reverses the expansion.
Values below one use a reciprocal and a negated answer. One is exact zero.
The provisional inclusive `[0.999, 1.001]` band uses the unchanged public series
to avoid poorly conditioned expansion near one. This band is experimental,
not a final eligibility rule for all inputs.

All scratch objects are per-call, cleaned up on every exit. The external
destination is committed only on success, including an input/output alias.
Experiments accept 1–10,000 requested significant digits and 40–512 additional
working digits. Those are harness bounds, not changes to NumForge limits.

**This path is not yet certified for general correct rounding.** It uses a
small first omitted term, fixed extra precision and equality of rounded AGM
iterates as feasibility criteria. References and repeated precision checks do
not replace error bounds on the entire theta tail, the AGM, reciprocal/
expansion and final rounding. Those remain required before production selection.

## Fresh comparison

Windows x64, Ryzen 7 7435HS, MSVC 19.51.36247.0, Release. Production baseline
is `8c7ac7a` (the documentation-only `ae6b566` does not alter it). Current
sources add this isolated benchmark and validation. Three fresh processes per
algorithm/input/precision were run serially, after builds and reference/fault
checks finished. CPU frequency/affinity and unrelated background work were
not controlled. These are **C API calculation times**, not complete calculator,
formatting or HTTP request times.

| Input | Significant digits | Current series median | Theta/AGM median |
| --- | ---: | ---: | ---: |
| `ln(2)` | 34 | 0.186 ms | 0.569 ms |
| `ln(2)` | 250 | 1.539 ms | 2.076 ms |
| `ln(2)` | 1,000 | 45.871 ms | 25.507 ms |
| `ln(2)` | 2,000 | 334.583 ms | 96.151 ms |
| `ln(2)` | 5,000 | 5,888.040 ms | 608.398 ms |
| `ln(0.5)` | 5,000 | 5,010.342 ms | 614.163 ms |
| `ln(10)` | 5,000 | 5,136.722 ms | 645.162 ms |

All 42 timed calls match the entire independent reference exactly; all use
nearest/even final rounding. Theta/AGM uses 80 additional working digits;
the public series retains its existing 24. The CLI `extra` parameter applies
only to the experimental algorithm. A 60-second diagnostic deadline allows
the series to finish; the 64 MiB cumulative / 512 KiB single-allocation limits
remain unchanged. Timing samples do not enable the live-allocation ledger.

At 5,000 digits, `ln(2)` is approximately **9.7 times faster** in this comparison.
Allocation calls are 14,166 for the series and 12,061 for theta/AGM; cumulative
requested bytes are 30,955,993 and 23,147,217 respectively. At 1,000 and 2,000
digits the experiment requests more cumulative memory than the current series,
despite lower CPU time. Speed alone should not determine a universal crossover.

A separate live-memory probe with the actual five-second deadline completes
theta/AGM successfully, matches the full 5,000-digit reference and records
181,932 peak tracked bytes, including the input, output and reference objects.
The corresponding series probe reaches the time limit and preserves its
destination. These probes verify complete cleanup; they are separate from the
three timing samples above.

A supplementary MSVC probe at **10,000 significant digits** also completes
with the actual five-second / 64 MiB limits: 2,818.187 ms, 52,963,339 cumulative
requested bytes and 360,857 peak tracked bytes including references. Its whole
result matches the independent Decimal reference. This is one live-memory
probe, not a three-run timing median. Twelve additional exact checks cover
all six rounding modes with 80 and 160 extra digits at this precision.

For `ln(2)` at 5,000 digits, the prototype performs nine argument squarings
and 22 AGM iterations. Counts exclude arithmetic inside sqrt and pi. Lower
precision loses to the existing series, supporting a hybrid rather than a
wholesale replacement. A conservative **1,000-digit initial candidate crossover**
is reasonable for subsequent testing, but has only been timed across the three
inputs above and is not installed in production.

## Reference, ownership and portability checks

- Checked-in smoke fixture: 120 exact references, 34 and 250 significant
  digits, all six rounding modes. Inputs include `2`, `0.5`, `10`, `1.01`,
  `0.99`, `1`, `1+10^-40`, `10^1000`, `10^-1000` and `123.456`.
- Native MSVC and GCC each pass 600 exact checks: the same ten inputs at
  34, 250, 1,000, 2,000 and 5,000 digits, with both 80 and 160 extra working
  digits and all six final rounding modes. The independent standard-library
  Decimal logarithms agree after target rounding at both extra precisions.
- MSVC x64, GCC native, MSVC Win32 and GCC portable division each pass the
  smoke suite (240 exact checks per build), domain/deadline checks and
  **25,270 deterministic allocation/deadline checkpoints** with separate and
  aliased outputs. Exhaustive prototype fault injection uses 34-digit `ln(2)`;
  it does not exhaustively inject the dynamic high-precision pi branch.
- Win32 and portable division additionally match complete 5,000-digit `ln(2)`
  references with 80/160 extra digits, aliasing and live-memory probes. Their
  arithmetic is much slower: those verification runs used a 60-second
  deadline and ran concurrently. They are not comparative speed measurements
  or evidence of fitting a five-second limit on those paths.
- High-precision memory probes and injected failures verify scratch cleanup.
  A failed call preserves the original destination; separate input ownership
  is checked during exhaustive fault injection.

The prototype never supplies pi from an external reference. Python is used
only to generate independent logarithm fixtures. Ordinary builds omit the
prototype and instrumentation. Existing CI benchmark selection is unchanged;
no GitHub CI run or release was performed for this local experiment.

## Reproduce

Build with `NUMFORGE_BUILD_BENCHMARKS=ON`. With testing enabled, run:

```sh
ctest --test-dir build/factorial-bench -C Release -R logarithm_agm --output-on-failure
```

Generate optional larger references using Python's standard library:

```sh
python benchmarks/generate_logarithm_agm_cases.py --max-digits 5000 --output build/logarithm-agm/cases.tsv
node benchmarks/check_logarithm_agm_benchmark.cjs build/factorial-bench/Release/logarithm_agm_benchmark.exe build/logarithm-agm/cases.tsv
```

The benchmark CLI is:

```text
logarithm_agm_benchmark agm|series input digits extra rounding deadline-ms reference-file|- [--memory|--alias]
logarithm_agm_benchmark --fault-checks
```

Reference files contain an exact decimal value, compared numerically after the
timed call. `-` permits diagnostic failure outcomes; read the CSV status and
budget reason rather than interpreting process exit zero as a successful
calculation. A required reference mismatch returns a failed process status.
`--memory` enables the live ledger and checks complete cleanup; keep it separate
from ordinary timing. `--alias` evaluates into the input object.

The CSV `format_ms` column separately times canonical `bigdecimal_to_string`
after calculation, outside its timer. It excludes browser rendering and HTTP
transport. The [subsequent factorial/formatting report](FACTORIAL_BUDGETS_2026_10_11.md)
compares calculation and final text conversion with fresh serial measurements.

Raw CSV, independent high-precision fixtures, the serial recording script and
`measurements.json` are retained in ignored
`build/logarithm-research-2026-10-11`.

## Next production step

Derive conservative error bounds/adaptive precision for the eligible real path,
extend difficult rounding/conditioning inputs and dynamic-pi failure coverage,
then add a private precision-dependent dispatcher while retaining the public
series fallback. Recheck complete calculator requests, real/complex compositions
and all supported division paths before choosing a production threshold.
The measured improvement makes that work worthwhile; the experimental timing
is not yet an improvement users receive through the calculator.
