# Exact descriptive statistics

Implemented in current post-2.4.0 source. Include `numforge/statistics.h` and link
`NumForge::numforge`. The batch API works independently of the expression engine,
matrix module, Python and applications. Published 2.4.0 archives predate it.
The public header's Doxygen comments define the authoritative contract.

## Dataset summary

`numforge_statistics_describe(&summary, values, count)` accepts a nonempty array
of borrowed BigRational pointers. Every entry is one real observation; duplicates
count separately and repeated pointers are allowed. The operation computes an
owned immutable `NumForgeStatisticsSummary`, without retaining the dataset.

| Query | Meaning |
| --- | --- |
| `numforge_statistics_summary_count(s)` | Number of observations |
| `numforge_statistics_summary_sum(s)` | Exact sum |
| `numforge_statistics_summary_minimum(s)` | Smallest observation |
| `numforge_statistics_summary_maximum(s)` | Largest observation |
| `numforge_statistics_summary_mean(s)` | Arithmetic mean, sum/n |
| `numforge_statistics_summary_variance_population(s)` | Sum of squared deviations from the mean divided by n |
| `numforge_statistics_summary_variance_sample(s)` | Sum of squared deviations divided by n-1; NULL if n=1 |

All numeric fields are exact reduced fractions. A recurring mean such as 1/3 is
retained exactly and never rounded before variance computation. There is no
precision parameter. A singleton has population variance zero and **undefined**
sample variance; the NULL sample field does not mean zero. Empty input is invalid.

This is an unweighted batch summary, with no missing-value omission, streaming
state, stored dataset object, median, covariance, regression or distribution fit.
The existing BigDecimal scalar sequence/statistics APIs and calculator functions
retain their separate precision/rounding contracts.

## Algorithm

For anchor x_0 and d_i=x_i-x_0, accumulation computes S=sum(d_i), Q=sum(d_i^2).
The reported values use exact rational arithmetic:

```text
mean = x_0 + S/n
sum = n*mean
population variance = (n*Q-S*S)/(n*n)
sample variance = (n*Q-S*S)/(n*(n-1)), when n>1
```

Differences are squared instead of the original values, avoiding unnecessarily
large common-offset squares. Cancellation remains exact, including for tiny
spreads around 1000-digit offsets. The algorithm uses a linear number of rational
operations and a fixed number of scalar work objects, plus result storage.
Integer/denominator growth and reduced-fraction arithmetic can dominate the cost;
constant object count is not a constant-byte memory guarantee.

## Ownership, errors and runtime

Success assigns a new summary without freeing a previous destination. Every
failure preserves *summary and all inputs. Required NULL pointers/entries return
`BIGRATIONAL_NULL_ARGUMENT`; zero count returns INVALID_ARGUMENT; count/pointer
storage overflow returns VALUE_TOO_LARGE before entries are read. Numeric size/
scale and allocation errors retain the BigRationalStatus contract. There is no
partial report on failure.

Fields are borrowed immutable scalars until summary destruction. Never mutate,
destroy or use them as numeric outputs; copy via bigrational_copy for independent
ownership. Inputs and their array can be released immediately after successful
construction. `numforge_statistics_summary_destroy` accepts NULL. Queries do not
allocate: NULL summaries return zero count and NULL fields. Synchronize shared
handles externally; independently owned handles can be used independently.

No implicit resource scope is started. Input validation, arithmetic and field
construction inherit runtime.h limits without resetting counters or ending the
caller-owned scope. Cooperative time/memory exhaustion returns OUT_OF_MEMORY;
`numforge_budget_failure` distinguishes the cause. Allocation volume is cumulative
requested bytes, not live/peak memory. No scope means no library limits; budgets
are cooperative rather than process isolation.

## C example

The full [SDK example](../../examples/sdk/statistics.c) constructs the typed
samples [0,0,1], then calls:

```c
NumForgeStatisticsSummary *summary = NULL;
BigRationalStatus status = numforge_statistics_describe(&summary, values, 3);
if (status == BIGRATIONAL_OK) {
    const BigRational *mean = numforge_statistics_summary_mean(summary); /* 1/3 */
    const BigRational *population = numforge_statistics_summary_variance_population(summary); /* 2/9 */
    const BigRational *sample = numforge_statistics_summary_variance_sample(summary); /* 1/3 */
    /* Read/copy these borrowed scalars before destroying summary. */
    (void)mean; (void)population; (void)sample;
}
numforge_statistics_summary_destroy(summary);
```

Build/run installed SDK examples with:

```sh
cmake -S examples/sdk -B build/statistics-example -DCMAKE_PREFIX_PATH=/path/to/installed
cmake --build build/statistics-example --config Release
ctest --test-dir build/statistics-example -C Release --output-on-failure
```

## Python and interface availability

The source-built Python adapter delegates arithmetic to C. Start with the
[summary workflow](../guides/PYTHON.md#exact-descriptive-statistics) for a working
example.

`describe` consumes a nonempty iterable of exact real number() inputs: int,
Fraction, finite Decimal/numeric text and exact real Result. Binary floats,
booleans, approximate/complex values and missing values are rejected. Empty input
raises ValueError, Python type errors raise TypeError, and C failures use Error
with the existing status names and offset zero.

`StatisticsSummary` is frozen, with copied Fraction fields and integer count;
variance_sample is Fraction or None. It retains no native/input handle. Python
integer-string conversion limits apply to Fraction conversion. The operation
accepts time_limit_ms, allocation_bytes and single_allocation, with the
[standard execution defaults](../guides/PYTHON.md#errors-lifetime-and-execution-policy);
there is no precision option. Input number() conversion precedes the
summary scope; native copies, computation and field serialization share it.
Python object allocation/Fraction conversion lie outside C budget accounting.
Native calls retain the GIL.

The new summary interface is available in C and source-built Python. A calculator
`describe(...)` expression, web dataset editor, REST route and WASM binding have
not been added. Existing scalar calculator statistics remain available.

## Verification

Nine C cases cover known population/sample values, signed recurring fractions,
singletons, aliases/duplicates, ownership, 1000-digit offsets and 1000-digit tiny
spreads, invalid inputs/queries, allocation failure at each allocation and
deterministic cooperative cancellation. Installed C/C++ consumers use only the
numeric library. Seven Python tests add 25 independent Fraction pairwise-variance
fixtures, translation/scaling/permutation identities, input/lifetime checks and
budget failure/recovery through serialization. See the [benchmark and validation
report](../benchmarks/STATISTICS_2026_10_19.md).
