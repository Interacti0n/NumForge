# Browser function graphs

Open **Graphs / Grafy** (`/graph?lang=en` or `/graph?lang=sk`). Enter an expression
such as `x*x`, `sin(t)` or `1/x`, then press **Plot / Vykresliť**. Computation runs
only on submission. The view starts at −10 to 10 around zero.

Sample density and the parameter are automatic. The C parser chooses the unique
variable not present in the selected calculator session. `sin(t)` uses `t`;
`a*x` uses `x` if `a` is stored. Multiple unknown names are rejected rather than
guessed. With no free variable the graph uses `x`, shadowing a referenced session
`x` or plotting a constant expression. Constants, function aliases and strings
are AST nodes, not candidate letters. Names remain case-sensitive.

**View and settings / Rozsah a nastavenia** contains optional increasing x bounds,
calculation precision (1–10000 significant digits, default 34) and radians/degrees.
Bounds are finite decimal literals, including scientific notation, not expressions
such as `pi` or `1/3`. They describe the displayed interval, not a domain restriction.
Precision controls approximate C mathematics; it does not control graph density.
Some functions retain at least 34 working digits. Plot coordinates still use the
rounded 12-place projection, regardless of a higher calculation precision.

## Calculator variables

Enable **Use calculator variables and ans** for read-only snapshots in the same
browser tab. Sampling does not create/reset a session or change variables, ans,
histories, previews or random state. The graph parameter shadows the session name
with the same spelling. A changed session revision rejects the graph request.

## Curves and gaps

The [adaptive C HTTP sampler](../reference/ADAPTIVE_GRAPH_HTTP_API.md) evaluates
midpoints and refines intervals with curvature or mixed point classifications.
The browser draws only connections accepted by the sampler. Domain errors,
complex values (including zero imaginary components), quantities and intervals
still unresolved at refinement bounds remain gaps. There are no point markers
or manual connection/sample-count controls. The sample table stays accessible.

Connections are heuristic, not proofs of continuity. Narrow poles, discontinuities,
isolated valid values or oscillations can escape finite sampling. Gaps can also
omit a continuous but unresolved steep curve. A message reports exhausted
refinement. Initial real samples suggest y bounds; newly discovered poles do not
inflate the view. The curve is clipped to those bounds, which can also omit newly
discovered extrema. Pan/zoom and explicit y control remain future work.

Coordinates are rounded scientific strings from C. Browser overflow or underflow
is omitted, never plotted as a false zero. Collapsed x coordinates prevent drawing.
One five-second budget includes preparation, every sample and serialization, with
existing allocation and response bounds; expensive requests fail atomically.

Settings persist per tab across language/navigation. Editing cancels pending work
and clears stale results. The original uniform sampling HTTP endpoint remains
available to API clients, but the web uses the adaptive endpoint.
