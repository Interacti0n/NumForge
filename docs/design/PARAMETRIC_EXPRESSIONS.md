# Parametric expression foundation

Private `src/application/expressions/expression.h` prepares typed expressions
for graph/root adapters. Installed consumers use the separate opaque
[public engine](../reference/ENGINE_API.md).

## C application operation

- `application_expression_prepare` owns the parsed AST, copied context and one
  typed snapshot per referenced variable/ans. The parameter shadows a session
  name under [naming rules](../guides/VARIABLES.md#syntax-and-lifetime). Assignment
  and every random alias are rejected. Source text/session can then be destroyed.
- `application_expression_sample` borrows typed parameters, returning an owned
  sample array without reparsing, source substitution or display round trips.
- Destroy handles/samples with their matching functions. Initialize outputs;
  success publishes new ownership, failure preserves pointers. Samples outlive
  handles/inputs. Handles are neither concurrent nor reentrant.

Preparation/sampling never mutate session values, preview/history/RNG or revisions.

## Errors and resource limits

Errors retain original UTF-8 byte offsets. Missing ans/unresolved names fail
preparation; released sessions are invalid sources. Batches accept 1–1024 valid
typed parameters. Domain/dimension errors produce individual empty samples;
later points continue. Graph adapters classify non-real/quantity values explicitly.

Resource/scale/structural failures abort the whole batch, preserving output.
Handles recover after the enclosing failed scope ends. All points share one
budget; nested calls never reset/end it. Graph/solver workflows must wrap
preparation, batches, formatting and serialization in one outer scope.

## Next integration

Uniform/adaptive/screen graph adapters and checked numerical roots already use
this foundation. Grids/root kernels are public; expression/domain adaptation
remains private. Sampling and discovered points do not prove completeness.
[Graph contracts](../API.md) · [Numerical equations](../reference/NUMERIC_ROOT_HTTP_API.md).

## Verification

`application_expression_tests` covers snapshots, lifetime, typed domains,
parameter shadowing, failures and shared budgets. Allocation-enabled fixtures
require zero live allocations and ensure a one-point budget cannot fund two
points by resetting counters. [Testing](../TESTING.md).
