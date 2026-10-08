# Calculator variables

Variables live in the current CLI or browser session. Type `x = 2/3` and confirm
with Enter or the calculate button (`=`), then use `x*3` to get exactly `2`.
A live preview shows the prospective result without saving the variable.

The browser Session panel has History and Variables tabs inside the scrollable
right sidebar; functions and session controls scroll together on desktop. The list
shows all 32 supported variables independently of the last 16 history entries. Clicking a
variable inserts its name at the input caret. Each displayed value is a snapshot
of its last confirmed assignment using that assignment's display settings;
very long text is shortened. The server retains the full typed value and clicking
the name uses that value, including exact fractions and units. Preview or failed
assignments leave the list unchanged. Tool/language navigation keeps the list;
New session clears it together with the calculator session. Reload retains the
tab's ID and fetches the authoritative variable/history lists from the server.

Use the × button beside a variable to delete it and reclaim one slot. Deletion
leaves `ans`, history and random state unchanged; other variables retain their
copied values. A deleted name becomes undefined until assigned again. The UI
removes the row only after the server confirms deletion. An uncertain response
can be retried with the same button and revision before further calculations.

HTTP clients can send `POST /api/evaluate?precision=10&angle=rad&client=…&revision=N&action=delete-variable`
with a plain-text body containing the case-sensitive name (without whitespace).
An existing session and a newer positive revision are required. Success returns
`{"ok":true,"result":""}`. Repeating the same deletion revision/name immediately
is safe; missing valid names are a successful no-op. Invalid names, stale
revisions and expired sessions use the ordinary calculator error schema.
Deletion invalidates previews and prevents replay of older assignments from
restoring the deleted variable. This command is not an expression function.

Variables and `ans` also retain [Quantity](../reference/QUANTITIES.md) values and dimensions:
`x = qty(3; "m")` followed by `x*x` gives `9 m²`. A later scalar assignment
replaces the unit metadata as well as the number. `qty` and `convert` are
reserved function names.

## Syntax and lifetime

- One top-level assignment: `name = expression`. Reassignment is allowed.
- Names contain 1–31 ASCII letters, are case-sensitive, and must not be `ans`,
  a constant (`e`, `π`, `φ`) or a registered function name. ASCII `pi` and
  `phi` are ordinary available names. Digits/underscores are
  not part of this initial naming grammar. Up to 32 distinct names per session.
- Use names anywhere an expression accepts a value: `sqrt(x)`, `x+y`, `2*x`.
- Undefined names produce a located error. Chained/nested assignments and
  user-defined functions are not supported.
- New session, CLI `reset`, explicit HTTP reset/release, server restart or session eviction
  discards variables. In-app navigation/language changes keep the same session.
  Reload also retains the tab's ID. Server snapshots synchronize displayed lists;
  no disk persistence, account or cross-tab sharing is added.

## Stored values

A variable stores the computed value, not a formula. `y=x` copies the current
value of `x`; later changes to `x` do not change `y`. Exact integers and fractions
remain exact, regardless of the displayed precision. An approximate result such
as `sqrt(2)` keeps its original computed precision; increasing display precision
does not recompute it. Assign it again to compute at the new settings.

```text
x = 2/3     -> 2/3
x*3         -> 2
x = x+1     -> 5/3
y = x       -> 5/3
x = 7       -> 7
y           -> 5/3
```

Successful assignments also update `ans` and history as ordinary confirmations.
Failed assignments preserve variables, `ans`, history and confirmed random
state. Replaying the latest confirmation with the same request revision does
not apply it twice. A fresh confirmation of `x=x+1` increments it again.

The retained state is bounded by the variable count and existing per-value
allocation limits. The history's previous memory estimate excludes these
additional snapshots; this remains a local loopback application, not a public
multi-user service. UI and HTTP tests cover preview/commit, reset, isolation,
precision, retries and failure preservation.
