# Numerical references and parser limits

## Independent numerical oracle

`tests/oracles/test_numeric_oracle.js` compares `numeric_oracle_driver` with
independent JavaScript BigInt/rational arithmetic: 7012 seeded cases
(`0x12345678`), coefficients up to 140 digits, six rounding modes, exact-first
and fixed/significant division. This includes 700 integer-call cases and roots
up to 280 digits. Integer-root references use binary search, not Newton iteration.
Another 520 real-root cases compare rational powers/midpoints: 480 rounded and
40 exact finite roots.

`transcendental_references.tsv` contains 319 exp/log/trig/hyperbolic references
generated with mpmath 1.3.0 at 220 and 300 decimal digits, rejecting inconsistent
rounded references. Regenerate with `generate_transcendental_references.py`;
CTest reads frozen TSVs and needs no mpmath. Nearest rounding/domain errors require
exact agreement; directed modes currently allow/report one requested-place ULP.
This is high-precision reference evidence, not certified correct rounding.

Focused suites additionally cover extreme scales, zero/sign/domain boundaries,
aliases, aggregation/statistical divisors, exact roots, dynamic 520-digit
constants, large-angle reduction, cancellation and unchanged outputs after
allocation/deadline failure. Expensive paths sample documented failure positions;
cheap representative paths exhaust them. Preserve every discovered issue as a
small regression; identities alone are insufficient independent references.

Run through [Testing](../TESTING.md), or filter CTest with `-R numeric_oracle`.
Failures print input/expected/actual. Generated drivers are trusted test tools,
not public input parsers.

## Parser fuzz smoke and precision contract

`parser_fuzz_tests` parses 3000 deterministic byte strings twice, checking
repeatable statuses/offsets and cleanup without evaluating random costly
factorials/exponents. Calculator contracts fix adjacency and intermediate
rounding; CLI tests exercise UTF-8/CRLF, EOF, length boundaries and recovery.

Portable parser/numeric/formatter fuzz harnesses bound input to 256 bytes,
100 ms cooperative work, 1 MiB cumulative/64 KiB single allocations. Numeric
round trips and deterministic non-mutating formatting are checked where the
output exponent fits the numeric parser. Resource exhaustion is permitted.
The allocation-free HTTP framing harness accepts 8191 bytes; focused HTTP/socket
tests cover origin, limits, incomplete/duplicate headers and deadlines.

Coverage-guided Unix/Clang smoke run:

```sh
cmake -S . -B build-fuzz -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=OFF -DNUMFORGE_BUILD_FUZZERS=ON -DNUMFORGE_ENABLE_SANITIZERS=ON
cmake --build build-fuzz --parallel 2
mkdir -p build-fuzz/corpus-parser
build-fuzz/fuzz_parser build-fuzz/corpus-parser -dict=tests/fuzz/numforge.dict -max_total_time=30 -timeout=5 -max_len=256
```

Use separate corpora for numbers/formatter. HTTP uses `http.dict`, max_len=8191.
CI runs four 30-second campaigns with 512 MiB RSS caps and retains findings.
Replay failures and add focused regressions. Short campaigns are not exhaustive.
[libFuzzer](https://llvm.org/docs/LibFuzzer.html).

## Independent complex references

Numerical complex suites and eight allocation-failure shards run separately.
Shard s injects positions s+1, s+9, … after counting allocations; all covered
positions run. Executables without arguments retain exhaustive single-process
behavior.

`generate_complex_references.py` uses mpmath 1.3.0 at 600/800 digits and requires
matching strings. The corpus covers 744 unary, 156 binary div/log/pow and 384
stability cases at 12/34/100/250 significant digits. Include six rounding modes,
cuts, near poles, tiny components, ±1e100 magnitudes and signed-int64 exponent
limits. Adjust negative-imaginary asinh cut values to NumForge's documented lip.
`--binary`/`--stability` select subsets.

Exact BigInt decimal comparison allows two componentwise significant-place ULPs;
expected zero uses `2*10^(1-digits)`. Nonzero components use their own exponent,
so dropping a tiny component fails. Explicit C assertions supplement references
whose precision cannot resolve extreme tails. This is sampled numerical evidence,
not a uniform error/correct-rounding proof. Temporary input files avoid Windows
pipe backpressure and are cleaned up.

Benchmark builds add `complex_memory_smoke`: 146 independently checked half-even
cases at 34 digits with zero tracked allocations after each case. Full `--memory`
runs 584 cases across four precisions, separately from timings. Details:
[complex performance protocol](../performance/protocols/COMPLEX.md).
