# API guide

Choose the interface you use. Arithmetic, application state and HTTP transport
have different ownership and compatibility contracts.

| Reference | What it covers |
| --- | --- |
| [Public C API](reference/C_API.md) | BigInt, BigDecimal, BigRational, BigComplex, BigRationalComplex, units, runtime scopes and ownership. |
| [BigComplex foundation](reference/BIGCOMPLEX.md) | Standalone complex arithmetic, precision and proposed integration stages. |
| [Calculator expressions](reference/CALCULATOR_EXPRESSIONS.md) | Operators, named calls, precision, syntax and limits. |
| [Local HTTP API](reference/HTTP_API.md) | Evaluation, preview/confirmation, client IDs, revisions and errors. |
| [Session HTTP API](reference/SESSION_HTTP_API.md) | Authoritative variables/history, typed snapshots, reset/release, functions and conversion history. |
| [Unit HTTP API](reference/UNIT_HTTP_API.md) | Catalogue and compatible expression conversions. |
| [Web/API coverage](reference/WEB_API_PARITY.md) | Supported web actions and application API gaps. |

## Values and sessions

- [Variables](guides/VARIABLES.md): confirmed assignments, deletion, snapshots and lifetime.
- [Units](reference/UNITS.md): exact conversion, case-sensitive IDs and sourced definitions.
- [Quantities](reference/QUANTITIES.md): `qty(...)`, dimensions, arithmetic and temperature rules.
- [Browser calculator](guides/CALCULATOR_WEB.md) and [converter](guides/UNIT_CONVERTER.md): user controls.

The public installed interface is the seven headers under `include/numforge/`.
Calculator/session headers are private. The HTTP server is a local loopback tool;
it is not a separately versioned public Internet service. See
[Application design](design/APPLICATION_DESIGN.md) for the boundaries.
