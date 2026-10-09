# Session, history and function HTTP API

Complex values have value-level `schema_version:2`, with kind
`complex_rational` or `complex_decimal_approximation`. Full snapshots include
`components:{"real":"1/3","imaginary":"2/3"}` (or exact stored scientific
decimal texts); compact snapshots set components to null. The enclosing
response and real-value schemas stay at version one. Component texts and kind
are authoritative; display text is not a restoration format. `approximate`
is true only for decimal-complex values. `complex_form` records
`cartesian`, `trig` or `exp` display policy. Variables, ans, previews and history
own/copy both components; clearing history preserves complex ans.

Complex history entries also provide a bounded `copy` constructor, or null.
Unit conversions reject complex input with `complex_not_allowed`, including
values with a zero imaginary component. No arbitrary snapshot import is added.

These routes supplement [expression evaluation](HTTP_API.md) and the read-only
[unit converter](UNIT_HTTP_API.md). State belongs to the application layer,
never the numeric library or browser storage. There is no database, account,
authentication or public C session ABI. Client IDs isolate local browser tabs;
they are not credentials. The loopback server processes requests serially.

## Routes

| Method and route | Query / operation |
| --- | --- |
| `GET /api/functions` | Registry names, canonical aliases, implemented flags and min/max arities. No query. |
| `GET /api/session?client=ID` | Revision, counts/capacities, history sequences and ans availability. |
| `GET /api/session/variables?client=ID` | All stored variables through bounded pages. |
| `GET /api/session/history?client=ID` | Confirmed calculator entries, oldest first; optional `id=REV` gets one. |
| `GET /api/session/value?client=ID&name=x` | Full typed variable or `ans`, without expression evaluation. |
| `POST /api/session?client=ID&revision=N&action=clear-history` | Clear calculator history, preserve ans/variables/RNG and conversions. |
| `POST /api/session?client=ID&revision=N&action=reset` | Clear variables, ans, preview, RNG and both histories. |
| `POST /api/session?client=ID&revision=N&action=release` | Free session values; retain a bounded retry tombstone. |
| `GET /api/conversions?client=ID` | Saved conversion entries; optional `id=REV` restores one without reevaluation. |
| `POST /api/conversions?client=ID&revision=N&action=commit&from=km&to=m` | Confirm body expression into independent converter history. |
| `POST /api/conversions?client=ID&revision=N&action=clear` | Clear conversion history only. |

Start via the existing `POST /api/evaluate?...&client=ID&revision=1&action=start`
with an empty body. Starting a live ID preserves it. Released IDs remain expired
while their tombstone is retained; use a fresh ID to start another session.
Reads never create/revive sessions, consume revisions, change preview/cache,
draw random numbers or modify ans. Reset/release POST bodies must be empty.
POST framing/origin/body limits match the existing HTTP server.

## Versioned values

Success includes `ok:true` and `schema_version:1`. Revisions and history IDs
are **decimal strings** representing uint64 values, avoiding JSON number loss.
Capacities/counts/offsets are bounded JSON integers. Snapshot example:

```json
{
  "schema_version": 1,
  "kind": "rational", "full": true, "text": "25/3",
  "approximate": false, "quantity": true, "temperature_point": false,
  "dimensions": [2, 0, 0, 0, 0, 0], "unit": "",
  "context": {
    "precision": 34, "places": 10, "rounding": "half_even",
    "notation": "auto", "angle": "rad", "significant_division": true
  }
}
```

`kind` is `integer`, `rational` or `decimal_approximation`. Full integer/rational
text is lossless (a denominator of one may be omitted). Decimal text is a finite
scientific encoding of the stored approximation, with all its stored digits;
it is not an error bound or a claim that the original mathematical result is
exact. Never parse these strings as JavaScript doubles for later computation.

Dimensions are length, mass, time, temperature interval, information and angle.
`unit` identifies the coordinate unit; an empty unit for a Quantity denotes an
unnamed canonical composite. `temperature_point` distinguishes affine points
from intervals. Display text may be rounded independently of the snapshot.
`context` preserves the original working/display precision, rounding and angle
mode. These values can be reused as the contract for future tools; importing
arbitrary client snapshots into mathematical state is not supported.

## Lists and bounded responses

Variable, history and conversion lists accept `offset=0`, `limit=8` by default,
and `full=0`. Limits are 1–32, offsets 0–32. They return `items`, `total`, `offset`
and `next_offset` (integer or `null`). All retained entries are accessible by
paging; nothing is silently skipped. `full=0` returns typed metadata with
`text:null` and `full:false`, plus the stored display. `full=1` adds authoritative
numeric text. The single-value endpoint defaults to `full=1`.

Read metadata first, then pass `at=REV` to pin pages to the session revision
(the **conversion revision** for converter pages). A changed revision returns
409; restart pagination instead of merging inconsistent pages. An omitted `at`
reads the current state. Individual `id` lookup cannot be combined with
offset/limit. Calculator entry IDs are confirmation revisions, not list indices.

Responses are bounded to 128 KiB including JSON framing; each full numeric
encoding is bounded to 65536 bytes. A large aggregate returns `value too large`;
retry with smaller pages, usually `limit=1`. Errors do not silently truncate or
round exact values. All serialization shares the 5-second/64-MiB cooperative
request budget and 512-KiB single-allocation limit. The 128-KiB response bound
is a separate serialization limit.

## Ordering, retries and expiration

Calculator mutations use the existing session revision, including previews and
variable deletion. Use a revision greater than the current one. Retrying the
latest lifecycle mutation with the same action/revision is idempotent while it
remains current; different or stale mutations return 409. A read consumes none.
Clearing history moves its newest value to owned ans storage; subsequent commits
replace it. This preserves exact values, units and approximation provenance.

Conversions use a separate `conversion_revision`. Confirmation accepts the same
`from`, `to`, `precision`, `places`, `rounding`, `angle` and `notation` parameters
as `/api/convert`; it rejects assignments, random calls and quantity-valued input.
The returned `entry` holds expression, source/target IDs, display, symbol,
approximation flags and typed value/context. Successful retries of the latest
identical input/settings return that entry even if calculator variables changed.
Changed input/settings or older revisions return 409. Numerical failures do not
advance the converter revision or add entries. An uncertain network/serialization
outcome must retry the identical request before starting another mutation.

Restore is a GET of the saved ID, never expression evaluation. Converter history
holds the last 16 confirmations and clearing it preserves calculator state.
Reset advances the conversion high-water mark so pre-reset confirmations cannot
be replayed; refresh metadata before the next converter mutation.

Eight FIFO sessions remain memory-only. Reload/navigation retains the tab's ID
and fetches fresh server lists; reset, release, server restart and FIFO eviction
can discard state. A release tombstone is lost when its FIFO slot is evicted.
Do not blindly restart an expired ID: use a fresh client and show the loss to
the user. There is no disk persistence or automatic TTL.

Malformed, unknown or duplicate query fields return 400. Missing sessions return
400 with `session expired; reload the page`; stale revisions return 409.
Errors include `ok:false`, `schema_version:1`, `status`, `error`, `code` and
one-based Unicode `column`. Conversion expression errors preserve their original
positions. Transport limits still use 403/408/411/413 as described in the HTTP guide.

## Minimal client example

After starting/confirming a session, substitute its ID and current revision:

```sh
curl "http://127.0.0.1:8765/api/session?client=CLIENT_ID"
curl "http://127.0.0.1:8765/api/session/variables?client=CLIENT_ID&limit=1&full=1"
curl -X POST --data-raw '' "http://127.0.0.1:8765/api/session?client=CLIENT_ID&revision=NEXT_REVISION&action=clear-history"
curl -X POST --data-raw '1/3' "http://127.0.0.1:8765/api/conversions?client=CLIENT_ID&revision=1&action=commit&from=km&to=m"
```

Future graphs/equations must add their application operation, versioned HTTP
contract and web integration together, preserving these value/context conventions,
client isolation, bounded output and explicit preview/confirmation semantics.

The function registry also reports `random` and zero-based `unit_arguments`
positions (`[1]` for qty, `[1,2]` for convert). Localized help and layout remain
presentation metadata; names, aliases, arities and implementation availability
come from the engine registry.
