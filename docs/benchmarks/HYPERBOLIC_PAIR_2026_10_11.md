# Shared sinh/cosh evaluation — 11 October 2026

This follows [shared sine/cosine evaluation](COMPLEX_PAIR_2026_10_11.md).
The next duplicated work in ordinary complex forward functions was the
exponential and reciprocal used by separate scalar sinh and cosh calls.

## Implementation

The private `bigdecimal_sinh_cosh_pair` computes `exp(abs(x))` and its reciprocal
once, then builds both outputs with the same 24 guard digits, intermediate
half-even rounding and final requested rounding as the public scalar calls.
For `abs(x) <= 0.5`, sinh still uses its original power series: subtracting two
nearly equal exponentials would lose tiny arguments. Cosh retains its formula.
The public scalar APIs and their implementation paths are unchanged.

Both outputs are staged privately and committed only after all fallible work
succeeds. They must be distinct, either can alias the input, and neither changes
on an allocation failure or deadline interruption. Zero follows the same atomic
contract. No public C API or calculator resource limit changes.

The ordinary complex sin/cos/sinh/cosh composition and its unscaled quotients
use the new pair. Scaled tan/tanh already use different exponential formulas;
the `1+i` tan/tanh probes below therefore have unchanged allocation counts.

## Measurements

Baseline is `603450c`, measured with a clean working tree before editing or
rebuilding. Both phases use the same MSVC x64 Release build configuration with
benchmark and failure instrumentation enabled. Three fresh-process calculator
samples per expression run serially, with no builds or tests running. Baseline
samples precede changed samples; medians describe this local machine, not a
portable speed guarantee or a CI timing threshold.

Calculator settings are Auto notation, 2,000 decimal places, 5 seconds and the
existing 64 MiB cumulative requested-allocation budget. Every sample succeeds.

| Expression | Before median | After median | Before requested bytes | After requested bytes |
| --- | ---: | ---: | ---: | ---: |
| `sin(1+i)` | 0.798 s | 0.598 s | 58,936,247 | 43,081,818 |
| `cos(1+i)` | 0.783 s | 0.594 s | 58,938,361 | 43,083,932 |
| `sinh(1+i)` | 0.787 s | 0.588 s | 58,936,247 | 43,081,818 |
| `cosh(1+i)` | 0.790 s | 0.594 s | 58,936,245 | 43,081,816 |
| `tan(1+i)` | 0.640 s | 0.622 s | 43,431,881 | 43,431,881 |
| `tanh(1+i)` | 0.559 s | 0.566 s | 42,368,604 | 42,368,604 |

For the four ordinary functions, cumulative requested bytes fall by about
26.9%, and the local median time by about 24–25%. Remaining allocation-budget
headroom increases from about 7.8 MiB to 22.9 MiB for these specific expressions.
Requested bytes accumulate all allocations; they are not simultaneous memory
usage, process RSS, or a promise that every expression at this precision fits.

Separate C API `--memory` probes use 2,000 significant digits per component,
without the calculator budget. For complex sine, requested bytes fall from
59,447,374 to 43,603,019. Tracked peak live payload stays at 30,044 bytes. Thus
this change reduces repeated allocation work, rather than the live-memory peak.
Calculator and C API precision contracts and timing scopes are different.

## Correctness and failure checks

- 216 pair/public equivalence cases: 12 inputs, all six rounding modes and three
  alias arrangements at 34 significant digits. Inputs cover zero, signs, tiny
  magnitudes, the 0.5 series boundary and values immediately around it.
- Exhaustive allocation and deterministic deadline injection through eventual
  success for zero, a small series argument and a negative exponential argument,
  with three alias arrangements. The shared test retains the existing sin/cos
  precision; sinh/cosh uses four output digits plus its normal guards. Its bound
  extends to 16,384 positions because 4,096 was insufficient for all checkpoints.
- Invalid pointers, identical outputs, precision overflow and invalid rounding
  preserve destinations.
- MSVC x64: 21 targeted CTest suites passed. GCC x64 and actual MSVC Win32:
  nine each. These cover numeric oracles, complex references at 12/34/100/250
  digits, scalar functions and allocation failures; MSVC also covers calculator,
  session/application/API paths and complex memory checks.
- MSVC and GCC `bigcomplex_tests --trig-only` passed, including exhaustive
  allocation failures in eight forward-function/quotient scenarios.
- Full C API component text before/after agrees for all six probes at 2,000
  significant digits. Independent Python Decimal references at 2,240 and 2,400
  working digits agree for sin/cos/sinh/cosh, match both C API phases, and validate
  every changed calculator result at 2,000 decimal places. Reference trigonometry
  uses direct Taylor series at one; exponentials use Python Decimal.

Raw CSV/TSV samples, reference generator and expected outputs remain in ignored
`build/performance-hyperbolic-pair-2026-10-11`. Automated correctness checks are
committed; local generated measurements remain build artifacts.
