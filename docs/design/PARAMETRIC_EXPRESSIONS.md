# Parametric expression foundation

The internal C application layer now provides a reusable expression handle for
future graph sampling and numerical solvers. It shares the calculator's typed
evaluation stage: exact integers/fractions, complex numbers, quantities, angle
units and approximation metadata follow the existing calculator contracts.

The [bounded HTTP graph sampler](../reference/GRAPH_HTTP_API.md) now calls this
foundation through the C application operation in `src/application/graph.h`.
The [browser graph page](../guides/GRAPHS.md) now displays sampled real points.
Adaptive sampling, solvers and an installed public SDK evaluator remain future work.
The installed SDK remains the numeric C API. Graph sampling does not implement
mathematics in JavaScript or reparse the expression for every point.

## C application operation

Declarations are in `src/application/expression.h`:

- `application_expression_prepare` parses one expression and returns an owned
  opaque handle. The parameter follows [session naming rules](../guides/VARIABLES.md#syntax-and-lifetime)
  and shadows a session variable of the same name. Assignments and every
  registered random-call alias are rejected, including nested calls.
- Only referenced variables and `ans` are copied, each once, into owned typed
  snapshots. The context is copied too. Input text and the source session may
  subsequently change or be destroyed without invalidating the handle.
- `application_expression_sample` evaluates borrowed typed parameter values
  against the prepared AST. It returns an owned array of typed samples. There
  is no string substitution, reparsing or conversion of results to display text.
- `application_expression_destroy` and
  `application_expression_samples_destroy(samples, count)` release the owned
  resources. Callers initialize output pointers and destroy old outputs before
  replacing them: success publishes a new object; failure preserves the pointer.

Preparation and sampling do not change variables, ans, preview/cache values,
history, random state or session revisions. Handles temporarily borrow one
parameter during execution and must not be used concurrently or reentrantly.
Samples own their results independently of the handle and inputs.

## Errors and resource limits

Preparation retains original UTF-8 byte offsets for syntax, forbidden random
calls and unresolved names. Missing `ans` fails during preparation. Released
sessions cannot be used as sources.

A batch accepts 1–1024 parameter values. Each value must be a valid typed
calculator value with its numeric presence marker populated. Division by zero,
invalid numeric domains, dimension errors and unsupported functions produce
individual empty samples with their `CalculatorError`. Later points still run.
The graph adapter must additionally classify non-real/quantity outputs and
discontinuities; complex results are not silently projected onto the real axis.

Resource exhaustion, scale/size overflow and structural input errors abort the
whole batch. No partially filled result array is published. An allocation fault
or deadline leaves the handle reusable after the enclosing budget is ended.

All points share one time budget and the existing cumulative 64 MiB allocation /
512 KiB single-allocation bounds. Nested calls inherit a caller's runtime scope
and do not clear or end it. A graph/solver adapter spanning preparation and
multiple adaptive batches must install one outer budget before preparation.
Separate calls without an outer scope are separate bounded operations, not an
unlimited graph/solver request. Output formatting and HTTP serialization must
also remain inside that adapter's outer operation budget.

## Next integration

1. The application sampler and HTTP contract now generate exact inclusive
   uniform grids, classify domain/complex/quantity points and preserve sessions.
   Adaptive sampling and continuity/pole detection remain separate work.
2. Responsive 2D `y = f(x)` plotting, axes and accessible point data are available.
   Add bounded adaptive sampling and discontinuity handling next. A finite grid
   alone does not prove continuity.
3. Reuse the prepared evaluator for a bounded real root solver with residuals,
   convergence/error reporting and one budget for the complete solve.

## Verification

`application_expression_tests` checks exact results, complex/quantity parameters,
parameter shadowing, source-text/session lifetime, preserved session bytes,
angle/context snapshots, domain gaps, naming/random restrictions, atomic
preparation and sampling failures, cancellation and recovery. Allocation-enabled
builds also require zero live allocations after cleanup and verify that a budget
large enough for one point cannot pay for two by resetting per-point limits.
Existing calculator/session/HTTP numerical references exercise the shared typed
evaluation stage after its extraction from the one-shot pipeline.
