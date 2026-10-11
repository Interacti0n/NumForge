# Checked factorial tree admission, 12 October 2026

This follows [factorial stress](FACTORIAL_STRESS_2026_10_11.md) and
[zero normalization](ZERO_NORMALIZATION_2026_10_11.md). The former coarse
`n*bit_length(n)` allocation estimate rejected 233,000 and 234,000 despite
successful forced-tree measurements under the application limits. Admission
now uses checked integer bounds for the factor ranges and the real workspace
capacity rule. Public APIs, factorial counter range, 8192 tree entry threshold
and application limits are unchanged.

## Bounds and shared workspace planning

For a range, the planner sums `bit_length(k)` over its factors by at most 64
power-of-two buckets. Since `k < 2^bit_length(k)`, this bounds product bits
without floating-point logarithms or a loop over all factors.

The full range is `[2,n]`. The enclosing upper-half range `[floor(n/2)+1,n]`
dominates either root child: it contains at least as many factors as the lower
child, with pointwise larger values, and encloses the upper child. Products in
descendants are no larger than their root child. Thus its rounded-up limb count
is a safe maximum multiplication operand size throughout the tree.

The product allocation bound `B` is eight times
`ceil(full_range_bit_bound/64)+2`, covering both rounded operand lengths in an
output buffer. Workspace planning shares the exact `workspace_frame` helper
with allocation: for operand length `m`, use `h=ceil(m/2)+1`, round `4*h`
limbs up to a power of two starting at 32, then continue with `m=h` until below
32. The largest planned request is the maximum of product bytes and all frame
requests. This refactoring preserves the actual allocation capacity policy.

For cumulative requests, each depth contributes twice its final frame capacity
in bytes, conservatively covering every earlier geometric reallocation. Product
buffers are bounded per tree level by `B`, with at most `bit_length(n)` levels.
The total is
`B*bit_length(n) + workspace_growth_requests + 8*n + 32768`.
The remaining terms cover rounding at nodes/leaves and leaf reservation using
the global factor bit length. For trees with more than 32 factors, balanced
leaves have at least 16 factors, hence fewer than `n/8+2` total nodes. Per-node
rounding is bounded by two limbs, while the global-bit leaf overestimate is
less than `2*n` bits; `8*n+32768` conservatively covers both. Small trees are
covered by the fixed allowance. All sums/products and capacity growth are
checked for size overflow, including on Win32.

The plan neither allocates nor consumes/poisons a budget. Admission compares
the bounds with the current remaining cumulative budget and single-allocation
limit. All actual allocation and cancellation checks still apply. Failed
admission retains grouped scalar multiplication, with normal atomic failure
and alias guarantees. No retry occurs after a sticky allocation failure.

## Boundary and complete requests

With a fresh 64 MiB cumulative and 512 KiB single-request budget:

| n | Cumulative tree bound, bytes | Largest tree request bound, bytes | Tree admitted |
| ---: | ---: | ---: | --- |
| 8,192 | 334,160 | 16,384 | Yes |
| 233,000 | 12,871,712 | 524,288 | Yes |
| 234,000 | 12,952,944 | 524,288 | Yes |
| 234,536 | 13,962,016 | 524,288 | Yes |
| 234,537 | 15,010,600 | 1,048,576 | No |
| 235,000 | 15,033,168 | 1,048,576 | No |
| 250,000 | 15,760,560 | 1,048,576 | No |

234,536 is the present conservative admission boundary under those budgets,
not a new input cap or proof of the largest physically computable factorial.
Forced trees for 234,537 and 235,000 still fit the real default budget in this
matrix: integer bit-length upper bounds retain some margin. At 250,000 the
forced tree actually fails the single-request memory limit. Further tightening
or splitting scratch remains separate work.

Measurements started late on 11 October and finished on 12 October, Windows
x64, Ryzen 7 7435HS, MSVC 19.51 Release, numeric baseline `b73ef2c`. Three
serial cold phase-pipeline processes used forward/reverse/forward order, with
no overlapping builds/tests and no affinity/frequency lock. An ordinary
production calculator call also checked each outcome. Ten scientific places,
half-even rounding and all default budgets were active. Independent Python
factorial integers were rounded in Node before measurement and successful
outputs compared byte for byte. Medians include evaluation and display:

| Expression | Complete request, ms | Outcome |
| --- | ---: | --- |
| `8192!` | 5.181 | Success |
| `233000!` | 2,770.063 | Success |
| `234000!` | 2,776.404 | Success |
| `234536!` | 2,761.432 | Success |
| `234537!` | 4,991.005 | Time limit on scalar fallback |
| `235000!` | 4,987.350 | Time limit on scalar fallback |
| `250000!` | 4,986.126 | Time limit on scalar fallback |

The saved baseline executable separately reproduced time-limit failures at
233,000 and 234,000. Failure times are cancellation observations, not completed
computations. Timings must not be compared directly with earlier runs on this
unlocked host to infer a regression for neighboring inputs.

At 234,536, raw factorial requested 9,029,904 bytes against the conservative
13,962,016 bound, with about 2,013,096 bytes peak extra live allocation in a
separate tracked invocation. A full calculator request used 11,104,610
cumulative bytes. The plan covers the raw tree; later projection/formatting
remains subject to the same shared application budget. It does not preallocate
or reserve the reported bytes, and live allocation is not process RSS.

## Verification

- 28 complete Python references per MSVC/GCC build through 250,000, including
  public/forced paths, fresh and aliased destinations, and both adjacent boundary
  inputs. Each completed under its computed tree memory bounds with the normal
  five-second deadline; tracked memory returned to zero after cleanup.
- `--plan-budget` is a diagnostic scope using the computed bounds, which can
  exceed application limits for rejected inputs. This validates upper bounds;
  the ordinary-budget stress matrix above separately validates real admission.
- 4,566 complete independent multiplication/workspace references passed on each
  compiler, including odd/unequal shapes, signs, aliases and corrupt-oracle rejection.
- Exact cumulative/single-budget boundaries, non-consuming repeated preflight,
  overflow/unrepresentable counters and the existing constrained scalar fallback
  passed. Allocation/deadline fault checks still preserve destinations.
- Four full builds passed: MSVC, GCC, Win32 and forced portable division.
  Nineteen selected suites passed on each native compiler and ten on each
  portable/Win32 build, including faults, properties, numeric references,
  calculator/session/API checks. The ordinary noninstrumented app build passed.
- A temporary loopback server matched independent results for 233,000/234,000/
  234,536 factorial, committed 234,536 into a fresh session, previewed `ans` and
  read the correct compact history. The user's running server/tunnel was unchanged.

## Reproduce

```powershell
python benchmarks/generate_karatsuba_factorials.py build/factorial-admission.tsv --workspace --admission
build/factorial-bench/Release/bigint_arithmetic_benchmark.exe --factorial-plan 234536
build/factorial-bench/Release/bigint_arithmetic_benchmark.exe build/factorial-admission.tsv --plan-budget
node benchmarks/run_factorial_stress.cjs build/factorial-bench/Release/calculator_limit_benchmark.exe build/factorial-bench/Release/bigint_arithmetic_benchmark.exe build/factorial-admission.tsv build/factorial-admission-recording
ctest --test-dir build/factorial-bench -C Release -R 'allocation_failure|multiplication_fault|karatsuba|normalization_fault' --output-on-failure
```

Configure/build opt-in benchmarks and tests first. Recording directory must be
empty. The stress runner obtains plan metadata from the C executable instead
of duplicating a stale JavaScript formula. Local ignored
`build/factorial-admission` holds full fixtures, CSV/JSON samples, hashes,
baseline outcomes and HTTP checks. The public guide/API need no limit changes;
the updated design documents explain algorithm admission.
