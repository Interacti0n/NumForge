# Numerical root domain checks — 18 October 2026

Follow-up to the [numerical audit](NUMERIC_ROOT_AUDIT_2026_10_18.md), baseline
`6f197db`. The audit's false root is now withheld without changing precision,
tolerances, bisection limits or the shared time/memory budgets.

## Behavior

Before publishing a non-exact candidate, the solver checks both original
prepared ASTs on its final rational interval. A restricted real-continuity
whitelist propagates exact rational enclosures. Division must exclude zero from
the denominator range, sqrt must have a nonnegative argument range, and ln or
unary log require a strictly positive range. Arithmetic, abs, powers and common
globally continuous real functions compose these checks. Unsupported or unknown
ranges do not establish a domain and produce **domain_unverified**, count 0,
no coordinate/residual and an empty roots array. No cancellation removes holes.

The audit case, with t=x-0.1234,

```text
t + 1E-30*t/abs(t) = 0
```

has no root and is undefined at t=0. Its final interval encloses that hole, so
abs(t) cannot be established as a nonzero denominator. It now returns
domain_unverified at precisions 16, 34 and 100. The separate `--adversarial`
benchmark now expects this diagnostic and passes; it is also a CTest regression.

Safe division is retained: `1/(x-0.1234)=3` on [0.3,0.6] converges normally,
including its equivalent negative-denominator and negative-power forms.
`1/sqrt(x)=3` and `ln(sqrt(x))=1` retain their valid local domains. Exactly zero
typed rational samples keep their existing pointwise acceptance path: the
original equation is valid at that point, even if its initial interval has a
hole elsewhere. An actually sampled invalid point remains domain_gap.

The new checks are deliberately incomplete. Dependencies can overestimate
ranges; unsupported calls, tan, step functions and some nested expressions
remain unverified even when valid. No rounded transcendental endpoint is used
as an enclosure. The [API contract](../reference/NUMERIC_ROOT_HTTP_API.md)
documents supported rules, unknown ranges, the 2048-node work cap and inherited
budgets. Finite-precision signs/residuals still do not provide certified
existence or root-error bounds. There is no new uniqueness/completeness claim.

The browser explains the new outcome in Slovak and English, and never offers a
copyable root for it. Pre-change stored numerical results are discarded while
keeping inputs; current results still survive same-tab navigation/reload.

## Cost

Same GCC 16.2 UCRT Release/WX profile, 27 cases × precisions 16/34/100 × three
serial repetitions. Raw local data:
`build/numeric-root-domains-2026-10-18/{gcc,adversarial}.csv`.
All 243 ordinary rows retain their expected outcomes; the independent Python
180-digit reference reevaluates all 126 found coordinates successfully.

Precision-34 medians, complete C solve:

| Case | Baseline ms | Domain checks ms | Extra cumulative bytes |
| --- | ---: | ---: | ---: |
| cos(x)=x | 7.63 | 7.35 | 3,502 |
| exp(x)=2 | 8.59 | 8.69 | 3,561 |
| x^3=2 | 4.35 | 4.65 | 20,819 |
| (x-0.1234)^3=0 | 3.52 | 3.86 | 22,153 |
| large-offset cancellation | 3.35 | 2.95 | 13,606 |
| sqrt(x)=0.1 | 3.32 | 2.99 | 4,542 |
| sin(1000*x)=0 | 4.23 | 4.64 | 8,380 |

Separate local recordings are noisy; negative differences are not speedups.
Evaluation counts remain unchanged. The check runs only before publishing a
non-exact candidate, not at every bisection step. Worst row was 78.23 ms;
maximum cumulative allocation volume remained 13,057,405 bytes in the
nonconvergent domain-boundary logarithm. No measured row approached 5 s.

## Validation and next work

- MSVC 19.44 v143/WX: full build and all 60 CTest entries passed (40.97 s).
- GCC 16.2 Release/WX/allocation statistics: 11 numerical test groups and both
  benchmark checks passed; no live tracked allocations remain.
- Tests cover denominator holes, cancellation, both denominator signs, powers,
  constants, abs, real sqrt/ln domains, safe nested compositions, unsupported
  branches, every observed allocation failure in a complete domain check,
  inherited memory failure and forced deadline checkpoints with atomic outputs.
- Chromium: all 19 equation/guide cases passed (43.6 s), including both new
  localized diagnostics, obsolete-result invalidation and mobile/desktop layouts.
- Production solver still uses bisection. Next: benchmark a safeguarded
  high-precision Brent experiment while retaining these domain checks and the
  existing baseline; review scaling/tolerances separately.

No push, deployment, release or live-server/tunnel change is part of this work.
