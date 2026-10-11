# Logarithm algorithm review — 11 October 2026

This is a feasibility review, not a production algorithm change or a measured
NumForge speedup. The current baseline is `8c7ac7a`; see the
[decimal workspace measurements](DECIMAL_WORKSPACE_2026_10_11.md).

## Recommendation

Prototype a high-precision real logarithm using the **Sasaki–Kanada theta/AGM
formula**, alongside the current reduced atanh series. Select a crossover only
after complete C API and calculator measurements, including calculation of pi.
Keep the series for low precision and arguments sufficiently close to one.

The current implementation takes square roots until the argument is in
`[0.9, 1.1]`, then sums odd powers divided by their orders. Its series convergence
is geometric, not quadratic. Workspace reuse reduces allocations without
reducing the number of terms. A 5,000-place `ln(2)` still takes about six seconds
in the diagnostic run and exceeds the unchanged five-second production limit.

## Candidates

| Candidate | Fit for the current project | Main unresolved cost |
| --- | --- | --- |
| Precision-dependent argument reduction + current series | Small implementation change; useful comparison baseline | More full-precision square roots, cancellation near one, repeated rounded division remain |
| Rectangular splitting / fixed-point series | Reduces full-size products and can avoid repeated general division; reusable for other elementary functions | Requires a new evaluation/error-control layer; convergence remains geometric |
| Binary splitting of exact rational series | Attractive for small rational arguments such as `ln(2)` | A general high-precision decimal argument can produce very large exact intermediates; our multiplication is still quadratic |
| Newton iteration solving `exp(y)=x` | Quadratic local convergence; precision can grow between steps | Our full-precision `exp` still costs about 2.5 seconds at 5,000 places; intermediate steps, reduction and precision control add cost |
| Approximate AGM logarithm | Reuses sqrt, pi and decimal arithmetic | Must bound the asymptotic remainder and handle the scaling correction/cancellation |
| Theta/AGM logarithm | Reuses existing primitives, exact mathematical identity, quadratic AGM phase | Theta tails, finite-precision error, pi cost and argument expansion need validation |

This ranking is an inference from the current code and measurements, not a
benchmark comparison of implemented candidates. Newton convergence alone does
not remove the cost of its exponential evaluations. A faster multiplication
algorithm could change the ranking, but is not a prerequisite for a prototype.

## Concrete theta/AGM prototype

For `y > 1`, put `q = 1/y` and use

```
theta2(q^4) = 2 * (q + q^9 + q^25 + ...)
theta3(q^4) = 1 + 2 * (q^4 + q^16 + q^36 + ...)
ln(y) = pi / (4 * AGM(theta2(q^4)^2, theta3(q^4)^2))
```

Expand the argument by repeated squaring, `y = x^(2^k)`, and divide the result
by `2^k`. For `0 < x < 1`, use a sufficiently accurate reciprocal and negate the
answer. Treat `x = 1` exactly; retain the current series near one to avoid
amplifying argument-expansion/reciprocal error and excessive expansion counts.

The AGM steps are `(a+b)/2` and `sqrt(a*b)`. Finite-precision termination must
use an error bound, not just equality of rounded iterates. The sparse theta
series can use recurrence and adaptive truncation; a first experiment can
expand enough that its first omitted term is tiny. Bounds must include the
entire tail, propagation into the AGM, arithmetic rounding and the final
division. A threshold on one omitted term is not by itself a rounding proof.

This needs no call to `ln(2)` or `exp`. NumForge already provides dynamic pi
calculation, decimal sqrt and the per-call significant-arithmetic workspace.
Dynamic pi currently has its own allocation-heavy path, so its cost must be
included even if a later private paired computation can share some work.
No global precision cache is required for the first implementation.

The identity and argument-expansion method are described in Brent and
Zimmermann, *Modern Computer Arithmetic*, sections 4.8.3–4.8.4
([author-hosted text](https://maths-people.anu.edu.au/~brent/pd/mca-cup-0.5.9.pdf)).
The approximate AGM alternative and cancellation error are described in the
[MPFR algorithm notes, section 4.5](https://www.mpfr.org/algorithms.pdf).
Arb documents both Newton logarithms and division-avoiding rectangular
splitting in its [elementary-function internals](https://arblib.org/arb.html).

## Independent feasibility model

An isolated Python standard-library Decimal model evaluates the theta/AGM
formula and compares it with Decimal's independent `ln`. Pi is generated with
the Chudnovsky series. It checks `2`, `0.5`, `10` and `1.01` at 100, 1,000 and
5,000 significant digits, each with 80 and 160 extra working digits: **24
cases**, all matching the reference under six final rounding modes. Each also
has relative error below `10^(-(digits+30))` at the higher working precision.

| `ln(2)` precision | Argument squarings | AGM iterations |
| --- | ---: | ---: |
| 100 digits | 4 | 11 |
| 1,000 digits | 7 | 17 |
| 5,000 digits | 9 | 21 |

The model uses three theta2 terms and three theta3 terms (including its constant
term). The omitted theta3 powers begin at `q^36`; argument expansion makes
those tails very small. AGM counts exclude pi calculation and arithmetic
inside sqrt. These counts establish feasibility, not NumForge timings,
exhaustive numerical coverage or a proof of correct rounding. The model does
not exercise production budgets or extreme inputs. Its script and output are
retained locally in ignored `build/logarithm-research-2026-10-11/model.py` and
`results.json`. Production sources are unchanged by this review.

## Integration and acceptance

1. Implement a private experimental path, retaining the public C API and the
   current series for comparison. Use per-call storage and checked precision/
   scale arithmetic. Do not raise calculator limits.
2. Derive truncation/rounding bounds and allow extra working precision when
   needed. Preserve all six rounding modes and exact-one/domain behavior.
   Avoid claiming that the existing fixed 24 guard digits certify the new path.
3. Compare full independent references at 34, 250, 1,000, 2,000, 5,000 and
   10,000 digits, including below/above one, extremely close to one, extreme
   decimal scales and values near rounding boundaries. Check logarithm bases,
   complex logarithms and functions composed from real ln.
4. Inject allocation failures and deadline checkpoints, including input/output
   aliases; preserve the destination on failure and free all private scratch.
   Check native MSVC/GCC, Win32 and portable division.
5. Measure three isolated complete calls per input/precision, with full-output
   validation, cold pi computation, unchanged budgets and separate allocation
   probes. Choose the crossover from successful measurements; retain the
   existing path where it wins or avoids numerical conditioning problems.

The immediate target is completing `ln(2)` at 5,000 output places within the
existing five-second limit. No subsecond target or specific speedup is promised
before measuring the implementation on NumForge's own arithmetic.
