# Local web source

The calculator, API guide and upcoming-tools page have separate Slovak and English HTML files.
`calculator.css` and `api.css` style their respective pages, while `chrome.css`
styles the shared header. `calculator.js` implements calculator behavior, and
`navigation.js` handles
transitions between the calculator, guide, future-tool pages and language versions.
It also fills the shared upcoming page with graph, equation,
sign-in or registration text based on the route. Change these source
files directly; do not edit the generated `web_page.h` under the build tree.

CMake embeds the web assets and the root `LICENSE` when building
`numforge_web`. The C server serves the HTML routes (`/`, `/api`, `/graph`,
`/solve`, `/units`, `/login`, and `/register`), the registered
`/assets/` routes and the license text at `/LICENSE` from the executable.
The same `logo.png` is displayed in all page headers and linked as their
PNG favicon. `wordmark.png` appears in the SK/EN guide introduction. The
server sends both images with their exact binary lengths.
The installed executable needs no asset directory, Node.js, or working-directory
setup. Adding a new asset requires a CMake dependency and a symbol/route in
`cmake/EmbedWeb.cmake` and `src/web/web_server.c`.

Run `cmake --build <build-directory> --target numforge_web` after an edit.
`web_server_smoke_tests`, `web_ui_tests`, and the SK/EN Playwright suite check
the HTTP assets and calculator behavior.

The calculator HTML supplies the accessible structure and translated labels;
`calculator.css` controls the responsive layout, and `calculator.js` owns live
previews, session state, function categories/search, recent shortcuts and keypad insertion. The guide
pages use `api.css` and their own section links. Keep both language pages in
sync when changing controls or navigation.
Internal navigation fetches the target HTML and replaces only the main content.
The header and footer stay mounted; the target page stylesheet loads before the
content is swapped, and the calculator initializes again when returning from
another page. Browser Back and Forward use the same path. A failed fetch falls
back to normal navigation. Section links keep native scrolling, external links
are unaffected, and reduced-motion preferences disable the content animation.
The shared header uses a spacious single-row horizontal menu with equally sized,
subtle control surfaces. Their positions and widths remain stable when switching
between Slovak and English. Narrow screens use an accessible
drop-down menu on narrow screens. Escape or clicking outside closes it.
The HTML keeps the content hidden until its stylesheet loads, so navigation does
not briefly reveal unstyled text. A failed stylesheet request reveals the plain
page instead of leaving it blank.
Language changes and navigation among these pages in the same tab preserve the calculator
session ID, revision, history, input, settings and recent tools in session
storage. Reload and New session start fresh. Server restart or session eviction
still loses the in-memory numeric state.

On desktop, the calculator shell uses the viewport height and keeps the main
controls in view. The result fills the available space above the keypad before
offering full-result expansion. The function list and visible history can
scroll inside their cards; very short windows may also scroll the calculator
column. Long results open in a native modal dialog with its own scroll area,
while narrow screens retain inline expansion. Clicking outside the result dialog
closes it, and its copy button uses the same result text as the main panel. The guide header
and section menu stay in place; only the guide text scrolls. On narrow screens,
the menu is a horizontally scrollable row.
Short results sit vertically centered with a subtle left accent; long values
start at the top of their clipped display, and errors use the error accent.

The expression starts as a one-line textarea and grows to five lines as text
wraps. Beyond five lines, an adjacent control enlarges that same field over the
result, ending before the result text so at least its first line stays visible.
The keypad and function library remain usable. Enter confirms and collapses the
field; Esc only collapses it. Clipped results show a visible `...` marker and a
`Show all...` control.

The full-width settings bar sits above the expression and stays compact in
height. The calculator and guide use colors based on Melanie Brown's Deep Purple
VS Code theme. Each calculator card has a title and icon. On desktop, the
function library and session take natural heights inside one scrollable right
column, keeping session/history reachable in short windows. The left calculator
column scrolls when needed. Function categories and function buttons have
separate labelled panels, divided from the search field. Categories form a
vertical list beside their controls on wide screens, and search matches Slovak
descriptions with or without diacritics. The recent row remembers up to twelve
distinct constants, operations or functions and shows as many as fit in one row;
visible buttons share the available width without extending past the card. The function
help appears after the active function group within the same scrollable column.
Inverse trigonometric and hyperbolic functions can also be found by their
`arc` and `arcus` names, including prefix-only searches.

The calculator and guide share a bilingual footer with the MIT license and
GitHub links. CMake replaces `@NUMFORGE_VERSION@` from `PROJECT_VERSION` when
embedding the pages, so the displayed version follows the build configuration.
The footer shows the exact English MIT text from the root license in a local
dialog; GitHub opens in a new tab. Header and footer dimensions match between
the calculator and guide.
