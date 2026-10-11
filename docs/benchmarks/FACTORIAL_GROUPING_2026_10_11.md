# Grouped factorial factors — 11 October 2026

Follow-up to [integer conversion and combinatorics](PERFORMANCE_OPTIMIZATIONS_2026_10_11.md).
The previous complete `100000!` took 4.803–4.837 seconds after those changes.
This change only groups factorial factors; conversion and formatting are unchanged.

## Algorithm and choice

Accumulate consecutive factors in a `uint64_t`. Before including factor `i`,
check `group > UINT64_MAX / i`. If so, multiply the private BigInt accumulator
by the completed group and start the next group with `i`. Flush the final group
after the loop. This reduces large limb scans from 99,999 to 27,439 for `100000!`.
No arithmetic wraps, decimal approximation or public API change is introduced.

The factorial argument cap remains 100000. The calculator retains its 5-second
deadline and all existing memory/output budgets. Budget checkpoints still run
for every factor. Failed allocation/cancellation destroys the private temporary
and preserves the caller's destination, including input/output aliases.

The existing recursive product-tree experiment remains benchmark-only. On this
machine, grouping wins at every measured comparison size and requests fewer
allocation bytes. There is no reason to add its recursive production path now.

## Measurements

Same Windows PC, MSVC Release, serial execution with no concurrent tests/builds.
Raw BigInt rows use seven timed samples plus the harness's separate warm-ups and
allocation probe. Every result is compared with the complete independent native
JavaScript BigInt factorial reference outside timing. The timed operation alone
is reported; fixture parsing and reference construction are excluded.

| n | Grouped factorial median | Product-tree median |
| --- | --- | --- |
| 10000 | 7.74 ms | 14.72 ms |
| 50000 | 251.07 ms | 452.09 ms |
| 100000 | 1.283 s | 2.021 s |

For `100000!`, the grouped samples span 1.231–1.356 s; the tree samples span
1.931–2.141 s. The pre-grouping direct factorial median was 4.054 s in
[the original audit](PERFORMANCE_AUDIT_2026_10_10.md).

| Allocation probe for 100000! | Grouped | Product tree |
| --- | --- | --- |
| Allocation calls | 16 | 199997 |
| Cumulative requested bytes | 524280 | 4935680 |
| Peak tracked live bytes | 524296 | 568808 |

The probe includes the harness's reused destination; peak live and cumulative
requests are distinct metrics. Grouped allocation calls/bytes match the old
single-factor loop. Process memory and decimal serialization are separate.

The complete calculator pipeline, Auto with 10 places and the normal 5000-ms
deadline, succeeded in all three fresh-process runs:
**1.772320, 1.792260 and 1.791666 seconds**. Each independently checks the expected
output `2.824229408E+456573`, requests 1,341,227 allocation bytes and emits 19 bytes.
This is about 2.7 times faster than the preceding complete pipeline and provides
over three seconds of measured headroom on this PC. It is not a guarantee for
other machines or for serial HTTP queue latency.

Raw CSV and generated fixtures are kept locally in ignored
`build/performance-factorial-groups-2026-10-11`. The first attempt to select
`factorial-0` used an audit fixture subset that did not contain that case; it
produced no measurement. Regenerating the independent factorial fixtures with
`bigint_cases.js --factorial-only` resolved the setup error.

## Verification

- MSVC integer and allocation suites pass, as do calculator, contract, complex
  numeric, application and session suites rebuilt with the new library.
- GCC integer and allocation suites pass, including the final expanded checks.
- Tests cover `19!`, `20!`, `21!`, `22!` and `50!` across group/limb boundaries,
  aliases, zero/one, negative arguments and unchanged maximum argument checks.
- Every allocation failure and every deterministic deadline checkpoint for
  aliased `50!` is injected. Failures preserve the input; success must not report
  an ignored injected failure. Existing non-aliased allocation tests remain.
- Full exact references also verify 0, 1, 20, 100, 1000, 10000, 50000 and 100000.
- No timing assertions or CI wall-clock gates are added.

Reproduce after building benchmark targets:

```text
node benchmarks/bigint_cases.js <fixtures.tsv> --factorial-only
bigint_arithmetic_benchmark <fixtures.tsv> --case factorial-100000
bigint_arithmetic_benchmark <fixtures.tsv> --case tree-100000
calculator_limit_benchmark "100000!" 10 0 5000
```

No server restart, push or release is part of this change.
