# Local web source

Edit these assets, not the generated `web_page.h`. Keep Slovak and English
HTML structure, labels, guide part keys and section IDs aligned.

## Ownership

| Source | Responsibility |
| --- | --- |
| `calculator.*.html`, `calculator.js` | Expression input, previews, confirmation, function categories, keypad and session UI. |
| `calculator.css` | Shared workbench controls and responsive calculator layout; tool styles import it. |
| `chrome.css`, `navigation.js` | Shared header/footer, internal navigation, language changes and guide selection. |
| `api.*.html`, `api.css` | Guide content and its reading layout. |
| `function-library.js` | Localized descriptions/search aliases; `/api/functions` owns availability. |
| `graph.js`, `graph.css` | Up to four curve caches, camera, request queue, editor and expanded plot. |
| `graph-files.js` | Bounded workspace-file validation and standalone SVG/PNG capture; no sampling mathematics. |
| `equations.js`, `units.js`, `matrices.js` and their styles | C/HTTP adapters, result presentation and same-tab state for each tool. |

Mathematics and typed values belong to C. JavaScript validates/projects coordinates
and presents results; add application and HTTP contracts alongside new tools.
The purple palette originated from Melanie Brown's Deep Purple VS Code theme.

## Embedding and checks

`currency.js` owns the optional ECB/Frankfurter daily-rate table and tab-local
cache; `currency_web.c` applies supplied decimal factors through BigDecimal.
Market data is separate from fixed unit definitions and never fetched by C core.

CMake embeds the registered assets and root `LICENSE` in `numforge_web`.
The executable serves `/`, `/api`, `/graph`, `/solve`, `/matrices`, `/units`, `/login`,
`/register`, `/assets/` and `/LICENSE`; no asset directory, Node.js or Python is
needed at runtime. Account routes remain informational placeholders.

Adding an asset requires a CMake dependency, an embedding symbol in
`cmake/EmbedWeb.cmake` and a route in `apps/web/server/web_server.c`.
Rebuild with `cmake --build <build-directory> --target numforge_web`.
`web_server_smoke_tests`, `web_ui_tests` and the SK/EN Playwright suite cover
transport and UI behavior; see [Testing](../../../docs/TESTING.md).

`logo.png` supplies the header mark/favicon; `wordmark.png` remains available.
CMake substitutes `@NUMFORGE_VERSION@` from `PROJECT_VERSION`. The shared footer
opens the root MIT license in a local dialog and GitHub in a new tab.

## State and navigation

Internal transitions replace main content after its stylesheet loads, preserving
the header/footer. Each tool disposes listeners and aborts stale work before
replacement. Back/Forward and language changes retain same-tab input/settings;
a failed fetch falls back to normal navigation. Stylesheet failure reveals plain
content, and reduced-motion preferences suppress animations.

Session storage retains the client ID and UI state. C owns variables, `ans` and
confirmed histories; browser lists mirror the [session HTTP API](../../../docs/reference/SESSION_HTTP_API.md).
Clearing calculator history preserves variables and `ans`. New session resets
saved values; server restart or eviction still loses in-memory numeric state.

Desktop calculator columns scroll independently in short windows, preserving a
readable result. Long results use a modal on desktop and inline expansion on
narrow screens. Settings stay above the expression; mobile categories scroll
horizontally and history uses the sidebar's full width. Input grows to five
lines, then scrolls; Enter confirms and Shift+Enter inserts a line break.
Preview/confirmed labels are UI state; exact/approximate provenance comes from
the C numeric kind independently of display rounding. User instructions live in
the [calculator guide](../../../docs/guides/CALCULATOR_WEB.md).

The guide has six `data-guide-panel` articles. Its route accepts
`section=calculator|units|graphs|equations|matrices|api` and `lang=sk|en` in either order.
`navigation.js` handles active sections, legacy hashes and browser history;
without JavaScript, all guide content remains readable. Other routes retain
their existing query contracts.
