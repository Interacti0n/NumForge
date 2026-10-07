# Local unit-conversion HTTP API

The loopback `numforge_web` server exposes the same sourced unit registry and
conversion core as the [C API](UNITS.md). The [SK/EN browser page](UNIT_CONVERTER.md)
uses these routes. They are intended for the local application, with the existing
request framing, origin checks and calculator resource limits.

## Catalogue

`GET /api/units` returns `{ "ok": true, "units": [...] }`. Each entry has
`id`, `symbol`, `quantity`, `name_en`, `name_sk`, `source_url` and `pi_power`.
Use the case-sensitive ASCII `id` in requests; localized names and Unicode
symbols are display labels. Order and array indexes are not identifiers.

Quantity values are `length`, `area`, `volume`, `mass`, `time`, `speed`,
`temperature`, `temperature_interval`, `information` and `angle`.
The client can filter matching quantities; the server independently checks
compatibility. Temperature points and temperature intervals are distinct.

## Conversion

Send `POST /api/convert?from=km&to=m` with a plain UTF-8 expression body,
for example `1/3`. Supply `Content-Length` as for `/api/evaluate`.
Units are separate query parameters; `2 km` is not calculator syntax.
Percent-encode IDs containing `/`, for example `from=km%2Fh`.

```json
{"ok":true,"result":"1000/3","unit":"m","symbol":"m","input_approximate":false,"factor_approximate":false}
```

| Parameter | Values / default |
| --- | --- |
| `from`, `to` | Required catalogue IDs. |
| `precision` | Working significant digits, 1–10000; default 34. |
| `places` | Display decimal places, 0–10000, or `full`; default 10. |
| `rounding` | `toward_zero`, `away_from_zero`, `floor`, `ceiling`, `half_up`, `half_even` (default). |
| `notation` | `auto` (default), `plain`, `scientific`, `math`, `fraction`. |
| `angle` | `rad` (default) or `deg`, for functions in the input expression. |
| `client` | Optional existing calculator session ID: 32 lowercase hexadecimal characters. |

Unknown, duplicate, empty or malformed query parameters are rejected. The
`precision` parameter here controls working precision; `/api/evaluate` has its
existing precision contract. Display settings do not increase working accuracy.
`angle` does not select conversion units: `from` and `to` always do that.

Integer and rational expression values reach exact-factor conversion directly,
without a decimal display round trip. Pi-based angle conversions use the guarded
approximate path. `input_approximate` reports a decimal approximation produced by
the evaluator; `factor_approximate` reports a conversion requiring pi. Neither
flag certifies exact displayed text or correctly rounded arbitrary input: display
rounding may occur even when both are false. `fraction` preserves exact rational
results; approximate inputs remain approximate decimal results.

## Sessions and errors

Without `client`, expressions are stateless. With an existing `client`, the
expression may read stored variables and confirmed `ans`. Conversion never
changes variables, `ans`, history, the random generator, pending preview or
session revision, including on failure. An expired/missing session is not created
or reset. Assignments and `rand()` are rejected. There is no `action` or
`revision` parameter; confirmation/history integration is a later feature.

Conversion failures normally return HTTP 400 with `ok: false`, `code`, `error`,
calculator `status` and one-based `column`. Codes include `invalid_options`,
`unknown_unit`, `incompatible_units`, `assignment_not_allowed`,
`random_not_allowed`, `session_expired`, `invalid_body`, `expression_error`,
`time_limit` and `value_too_large`. Allocation failure returns HTTP 500 with
`out_of_memory`. Unit/query errors use column 1; expression errors retain their
input position.

Existing HTTP framing errors keep their existing response schema: for example
missing Content-Length is 411, foreign Origin is 403 and a body exceeding 4096
bytes is 413. Embedded NUL bytes are rejected. Evaluation, conversion and result
formatting share the calculator's cooperative time/allocation budget. This is
not a public hosted-service isolation model; see [the roadmap](ROADMAP.md).
