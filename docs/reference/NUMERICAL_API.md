# Numerical API

`<numforge/numerical.h>` exposes exact rational grids, adaptive interval scheduling
and bounded real-root searches; `<numforge/curve.h>` supplies sampled curve
acceptance policies and `<numforge/phase.h>` locates periodic-phase anchors.
All belong to `NumForge::numforge` and need neither an
expression engine nor application state.
The public headers are authoritative for callbacks, precision, ownership and failures.

| Task | Reference |
| --- | --- |
| Exact grids, adaptive scheduling, curve acceptance and periodic-phase anchors | [Grids and sampling](SAMPLING_API.md) |
| Single bracket search, multiple-root scanning and Python callbacks | [Root search](ROOT_API.md) |

Choose a topic below for its detailed C/Python contract.

## Exact uniform grids

See [exact uniform grids](SAMPLING_API.md#exact-uniform-grids).

## Python exact grids

See [python exact grids](SAMPLING_API.md#python-exact-grids).

## Adaptive interval scheduling

See [adaptive interval scheduling](SAMPLING_API.md#adaptive-interval-scheduling).

## Sampled curve acceptance

See [sampled curve acceptance](SAMPLING_API.md#sampled-curve-acceptance).

## Periodic-phase anchors

See [periodic-phase anchors](SAMPLING_API.md#periodic-phase-anchors).

## Use and result meaning

See [use and result meaning](ROOT_API.md#use-and-result-meaning).

## Examples and availability

See [examples and availability](ROOT_API.md#examples-and-availability).

## Python single-root search

See [python single-root search](ROOT_API.md#python-single-root-search).

## Multiple-root scanning

See [multiple-root scanning](ROOT_API.md#multiple-root-scanning).

## Python multiple-root scanning

See [python multiple-root scanning](ROOT_API.md#python-multiple-root-scanning).

## Cost and verification

See [cost and verification](ROOT_API.md#cost-and-verification).
