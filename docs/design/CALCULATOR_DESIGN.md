# Calculator design

Shared typed expressions plus application sessions. [Architecture](ARCHITECTURE.md)
owns source boundaries; [syntax](../reference/CALCULATOR_EXPRESSIONS.md) owns inputs.

## Modules

`api/` owns public/legacy entry points and status mapping; parser owns tokens/AST;
evaluators select exact, approximate, complex and quantity branches; values and
formatting retain separate ownership. Function/constant/RNG registries are shared.

## Evaluation policy and errors

Exact integers/rationals remain typed; approximate branches materialize them only
when needed. Working significant digits differ from output places. Full skips
final rounding, never reverses approximation.

Private `significant_division=true` derives coefficient exponents with checked
scales; false preserves legacy fixed-scale behavior. Public decimal division
keeps its own contract. Constants cache per evaluation/precision, preventing
earlier low precision from contaminating later operands.

Parsing through formatting/cleanup shares one cooperative scope; nested calls
inherit it. Resource and time failures map distinctly; ordinary allocation
failure stays out-of-memory. No forced thread termination.

Internal offsets are zero-based UTF-8 bytes; CLI/HTTP display one-based Unicode
columns. Transport-size and parser-depth limits are separate.

## Current implementation status

The pipeline is implemented. Type/exactness tags survive formatting but do not
certify arbitrary composed accuracy. [Roadmap](../project/ROADMAP.md).

## Confirmed session variables

Top-level assignments borrow resolved values during evaluation. Confirmation
stages result/history/variable copies before atomic publication; failure preserves
ans, history, variables and RNG. Approximate stored values are not recomputed
when display precision changes. Session-sensitive calls cannot reuse unrelated
cached values. [Session protocol](../reference/SESSION_HTTP_API.md) ·
[Browser state](WEB_DESIGN.md).
