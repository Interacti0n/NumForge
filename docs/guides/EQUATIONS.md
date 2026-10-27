# Browser equations

Open **Equations / Rovnice** (`/solve`). Enter an equation and choose **Solve**
or press Enter. Examples fill inputs without running a calculation.

| Mode | Example | Result / scope |
| --- | --- | --- |
| Whole equation | `3x+1=7`, `x^2=2` | Exact linear/quadratic roots, including radicals and complex roots |
| Coefficients a, b, c | `a=0, b=3, c=-1` | Solves `a*x^2+b*x+c=0`; `x=1/3` |
| Numerical interval root | `cos(x)=x` on `[0,1]` | One checked approximate real root |
| Multiple interval roots | `sin(x)=0` on `[-10,10]` | Bounded search; the root list may be incomplete |

Exact modes require rational real constants, lowercase `x` and degree at most
two at every step. `3x`, parentheses, constant division and powers 0/1/2 work;
`x/x`, functions of x, irrational coefficients and saved variables do not.
Use numerical modes for supported non-polynomial equations.

Exact roots have copy buttons and separate decimal previews. Preview precision
is 1–1000 significant digits (default 16); it does not change the exact root.
Repeated roots appear once with multiplicity. Approximate previews are not
certified rounding intervals.

### Editor and examples

**Functions & constants** edits the current equation or selected coefficient.
Search accepts localized names/aliases; insertion wraps selected text or places
the caret inside a call. Use `;` between arguments. Close/Escape retains edits;
Solve/Enter submits the form. Opening the editor or choosing an example does not
calculate. Exact modes offer exact constant operations; numerical modes add
supported functions of x. Manual input works if the catalogue is unavailable.

**Recent equations** keeps eight completed input sets in the current tab.
Restoring cancels pending work, restores mode/bounds/precision and waits for
Solve. Remove entries with × or clear the list. This is input history, not
permanent storage. Tool/language navigation retains fields and bounded results;
closing the tab clears browser state.

## Show a solution in the graph

**Show in graph** adds `(left side)-(right side)` and centers near a selected
real root. Existing functions remain; matching expressions are reused. With all
four slots occupied, remove a function first. Complex/unrepresentable roots have
no graph action. The camera is approximate; graph markers require independent
C verification. Transfer requires session storage in the same tab.

## Numerical interval search

Use a continuous interval with opposite signs at its endpoints. The unknown is
lowercase x; angles use radians. Working precision is 16–100 digits. Width and
residual tolerances are fixed at `1E-12`; preview digits do not tighten them.

**Checked point** reveals the full coordinate and residual; **Copy point** copies
that coordinate. C rechecks approximate candidates at higher precision and
conservatively checks the local real domain/continuity. A valid expression can
remain unverified. No confirmed sign change does not mean no root exists.
Narrow/shift the interval or increase precision when advised.

## Multiple roots on an interval

The scan samples 128 sections, refines sign changes and tests additional contact
candidates. It retains checked coordinates/residuals, including exact rational
contacts such as `1/3`. Undefined samples leave gaps. It stops at 32 roots,
4096 coordinates or the shared calculation budget.

**An empty or completed list is not a proof that all roots were found.** Narrow
contacts, close roots and dense oscillations can be missed. More digits alone
do not fix undersampling. All modes run in C under the shared ten-second/memory
budget; editing cancels pending presentation.

[Exact HTTP contract](../reference/EQUATION_HTTP_API.md) ·
[Numerical acceptance, outcomes and limits](../reference/NUMERIC_ROOT_HTTP_API.md)
