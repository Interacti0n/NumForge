# Calculator variables

Confirm `x=2/3`, then `x*3` → `2`. Previews do not save assignments.
The last confirmed value is `ans`.

In **Session → Variables**, click a name to insert it; × deletes it after server
confirmation. Deletion preserves ans, history and other copied values. Retry an
uncertain deletion before further calculations. History and variable lists scroll
inside their panel; reload fetches authoritative server state.

## Syntax and lifetime

- One top-level `name=expression`; reassignment is allowed.
- Names: 1–31 ASCII letters, case-sensitive; no digits/underscores. `ans`,
  constants, `i` and function names are reserved; `I`, `pi` and `phi` are available.
- Adjacent letters form one name: use `x*y` or `x*i`, not `xy` or `xi`.
- Up to 32 variables. Undefined names, nested/chained assignments and custom
  functions are rejected.
- Navigation/reload retain the tab's session. New session, reset/release, expiry,
  eviction or server restart discard values. There is no disk/account persistence.

## Stored values

Variables store **values, not formulas**. After `y=x`, changing x leaves y intact.
Exact fractions and quantities remain exact despite rounded displays. An
approximation keeps its computed digits; reassign it at higher precision to
recompute. Successful assignments update ans/history; failures preserve state.

Examples: `z=1/3+2/3*i` supports `re(z)`, `im(z)`, `conj(z)`, `abs(z)` and
`arg(z)` (radians). `d=qty(3;"m")` followed by `d*d` gives `9 m²`.

[Session HTTP ordering, deletion and snapshots](../reference/SESSION_HTTP_API.md) ·
[Quantity rules](../reference/QUANTITIES.md) ·
[Retained-memory limits](HOSTING.md#capacity-and-state)
