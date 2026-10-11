# Factorial resource budgets and final text conversion — 11 October 2026

The practical factorial argument cap of 100000 is removed. The grouped
algorithm still uses a uint64 counter: its representation bound is
`UINT64_MAX` (18446744073709551615), also exposed by the existing
`BIGINT_FACTORIAL_MAX_N` macro for source compatibility. Larger arguments are
rejected before expansion. This is not a guarantee that a representable
factorial can complete.

The calculator/HTTP pipeline retains its shared five-second monotonic budget,
64 MiB cumulative allocation budget, 512 KiB single-allocation bound and 65536
output-byte bound. Ordinary standalone C API calls still require the caller to
install a runtime budget; they do not gain an implicit five-second deadline.

The factor loop checks cancellation on every factor and exits before its
uint64 counter could wrap. Private grouped multiplication now checks every
256 limbs, so one growing accumulator scan cannot indefinitely postpone
cancellation. Factorial checks the budget again before publishing. All work
uses a private accumulator; failure preserves the destination, including an
input/output alias. Cancellation is cooperative, not an OS-enforced real-time
guarantee: scheduling, allocator latency and bounded work between checks can
affect the precise return time.

## Fresh measurements

Ryzen 7 7435HS, Windows x64, MSVC 19.51.36247.0, Release. Source baseline is
`738cc96` plus this cap/cancellation/diagnostic change. Builds and tests finished
before serial measurements; frequency/affinity and unrelated background work
were not controlled. Raw recordings, exact independent references and scripts
are in ignored `build/factorial-unbounded-2026-10-11`.

| Complete calculator request, Auto / 10 places | Outcome | Time |
| --- | --- | ---: |
| `100001!` | Success 3/3 | 1,928.938 ms median |
| `150000!` | Success 3/3 | 4,752.538 ms median |
| `200000!` | Time limit | 4,984.898 ms, one probe |
| `1000000!` | Time limit | 4,997.775 ms, one probe |
| `18446744073709551615!` | Time limit | 4,998.795 ms, one probe |

Successful calculator calls match independently computed Python `math.factorial`
values rounded with Decimal to the complete scientific output. The failure
probes return no output and report a time-budget failure rather than a factorial
input-cap rejection. `150000!` has little deadline headroom on this PC; these
outcomes are not portable completion promises.

Separate unrestricted BigInt API probes compare the **entire exact integer**
against Python references: 456579 decimal digits for `100001!` and 711273 for
`150000!`. Their single validated timing samples are 1,394.954 ms and
3,491.058 ms; separate memory probes verify cleanup. Do not subtract these
single samples from the calculator medians to infer an exact conversion cost.
The opt-in integer factorial sweep now includes both inputs, while quick CI
fixtures remain unchanged.

## Logarithm calculation versus final output conversion

The [theta/AGM prototype](LOGARITHM_AGM_PROTOTYPE_2026_10_11.md) benchmark now
reports a separate `format_ms` column for converting the completed decimal
to its entire canonical text. Reference checking and final text conversion
remain outside the calculation timer. This column covers `bigdecimal_to_string`,
not browser rendering, HTTP transport or every calculator notation policy.
Internal coefficient conversions during arithmetic still count as calculation.

Fresh serial three-process medians for `ln(2)`, 5000 significant digits:

| Algorithm | Calculation | Full result to text |
| --- | ---: | ---: |
| Current public series | 5,476.681 ms | 0.261 ms |
| Experimental theta/AGM | 604.476 ms | 0.266 ms |

Every complete value matches its independent reference. The production ln
algorithm is unchanged. A 60-second diagnostic deadline lets the series
finish, with the existing memory bounds. These fresh timings differ somewhat
from the earlier recording; the conclusion remains that final text conversion
is a tiny fraction of logarithm cost at this precision.

## Verification

- MSVC x64 and GCC native: 29 selected arithmetic, public API, calculator,
  application/session/web, reference and benchmark suites each pass. One old
  compact-factorial rejection fixture was updated from `2E5!` to `2E19!` to
  retain a genuine representation-bound rejection under the new contract.
- Win32 and portable GCC division: nine corresponding selected suites each
  pass, including independent numeric/division references and fault tests.
- New deterministic tests interrupt 100001, 1000000 and UINT64_MAX factorials
  with separate/aliased outputs, interrupt inside a 1024-limb private multiply,
  and verify scalar/typed calculator deadlines for operator and named forms.
- Five targeted browser/HTTP tests pass, including successful assignment of
  `factorial(100001)`, rejection above uint64 range without changing variables/
  ans, and scientific display in both languages on mobile/desktop.
- C API documentation, both web API languages, calculator guide, function
  library hints, design documents and changelogs reflect the new domain.

Historical benchmark reports retain the argument limits of their measured
source snapshots. No release, push or running-server replacement is part of
this change.
