# Exact statistics and linear regression

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
state, stored dataset object, median or distribution fit.
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

## Paired covariance

`numforge_statistics_covariance(result, left, right, count, sample)` writes an
exact reduced fraction to an initialized owned scalar. The arrays contain paired
observations by index; repeated pointers and duplicate observations are allowed.
Both arrays have `count` entries. Nothing is retained. The destination may alias
any input scalar; every failure preserves the destination and all inputs.

Population covariance (`sample=false`) divides the sum of products of deviations
from the respective means by n. Sample covariance (`sample=true`) divides by n-1
and requires at least two pairs. Population covariance of one pair is zero;
sample covariance of one pair is invalid. Signs are retained, and covariance
with the same dataset equals its corresponding variance.

For anchors x_0, y_0, let dx_i=x_i-x_0, dy_i=y_i-y_0,
Sx=sum(dx_i), Sy=sum(dy_i), P=sum(dx_i*dy_i). C computes
`(n*P-Sx*Sy)/(n*n)` or `(n*P-Sx*Sy)/(n*(n-1))` using exact rational operations.
This avoids unnecessarily large common-offset products, uses a linear number of
rational operations and a fixed number of work scalars. Integer storage and
denominator growth remain input-dependent.

Required NULL pointers/entries return NULL_ARGUMENT, zero count or a sample
singleton returns INVALID_ARGUMENT, and pointer-storage overflow returns
VALUE_TOO_LARGE before reading entries. Numeric errors and inherited resource
limits follow the same scalar/runtime contract as the summary. No implicit
budget, precision setting or rounding is introduced.

The [SDK example](../../examples/sdk/statistics.c) also computes sample
covariance of [0,0,1] with itself, returning 1/3. Python exposes the same kernel:

```python
from fractions import Fraction
import numforge as nf

assert nf.covariance([1, 2, 3], [3, 1, -1]) == Fraction(-4, 3)
assert nf.covariance([1, 2, 3], [3, 1, -1], sample=True) == Fraction(-2)
```

`covariance(left, right, *, sample=False, time_limit_ms=10000,
allocation_bytes=64*1024*1024, single_allocation=512*1024)` returns an independent
Fraction. Both iterables follow the exact-real input rules of `describe` below.
They must be nonempty and equally sized; a sample singleton raises ValueError.
`sample` must be a bool. Native copies, computation and serialization share the
C budget, after input number() conversion. Python object/Fraction allocation is
outside that accounting. No calculator expression, REST route or WASM wrapper
is provided for this operation.

## Exact simple linear regression

`numforge_statistics_linear_regression(&fit, x, y, count)` fits the unweighted
least-squares line `y = slope*x + intercept`. Require at least two paired rational
observations and nonconstant x. Duplicates and repeated/cross-aliased pointers
count separately; constant y gives a horizontal line. The implementation reuses
the anchored exact summaries and covariance:

```text
slope = population_covariance(x,y) / population_variance(x)
intercept = mean(y) - slope*mean(x)
```

These are the [ordinary least-squares coefficients](https://www.itl.nist.gov/div898/handbook/pmd/section4/pmd431.htm).
No rounding occurs, even for noisy data or tiny differences around large offsets.
Exact arithmetic does not imply measurement accuracy or predictive validity.
Weights, through-origin fitting, R-squared and inferential statistics are outside
this API.

Success returns an owned immutable `NumForgeLinearRegression`; failures preserve
`*fit` and every input, and success does not free an earlier destination.
`numforge_linear_regression_count` returns the count; `_slope` and `_intercept`
return borrowed immutable fractions. Release with `numforge_linear_regression_destroy`.
Input storage can be released immediately. NULL queries return zero/NULL and
NULL destruction is accepted. Inherited budgets and ownership follow the public
header; constant x or fewer than two pairs return INVALID_ARGUMENT. Required
NULL pointers/entries return NULL_ARGUMENT; pointer-storage overflow is rejected
before entries are read. No partial fit is returned.

```python
fit = nf.linear_regression([1, 2, 3], [1, 2, 2])
assert (fit.count, fit.slope, fit.intercept, fit.exact) == (3, Fraction(1, 2), Fraction(2, 3), True)
```

Python returns frozen `LinearRegression(count, slope, intercept)` with independent
Fraction coefficients. Inputs follow `describe`; `time_limit_ms`,
`allocation_bytes` and `single_allocation` have the same defaults and scope as
covariance. Insufficient/mismatched datasets raise ValueError, rejected types
raise TypeError, and constant x raises Error with status `invalid_argument` and
offset zero. There is no precision option. Native calls retain the GIL.

The [SDK example](../../examples/sdk/statistics.c) fits (0,0), (1,0), (1,1),
returning slope 1/2 and intercept 0. C and source-built Python are available;
no expression function, HTTP dataset route or browser editor is added.

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
report](../performance/baselines/MODULES.md#statistics).

Covariance adds C pairing/sign/fraction, singleton/alias, thousand-digit scale,
validation and exhaustive allocation/deadline failure checks. Python adds an
independent pairwise identity over 40 population/sample rational fixtures,
symmetry, translation/scaling, permutation and variance identities, input and
budget recovery checks. Installed C/C++ consumers and the SDK example exercise
the public numeric-only API. See the [covariance measurements and validation](../performance/baselines/MODULES.md#covariance).

Regression checks cover noisy/fractional/horizontal/two-point fits, duplicated
and borrowed prior-output inputs, independent ownership, 1000-digit offsets and
spreads, constant-x/validation failures, every allocation failure and inherited
budget cancellation/recovery. Python adds independent Fraction pairwise-difference
references, residual normal equations and permutation/affine identities. Installed
C/C++ consumers and the existing SDK/Python examples exercise the same C API.

The [regression benchmark](../performance/baselines/MODULES.md#linear-regression)
uses independently known reversed-progressions at two-point, ordinary, 1000-digit
offset and tiny-spread scales; no speed threshold is a correctness assertion.
