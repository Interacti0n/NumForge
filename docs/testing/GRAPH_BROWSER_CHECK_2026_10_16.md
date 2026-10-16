# Graph browser check — 16 October 2026

Application snapshot: `6121409`, MSVC Release executable. Tests use a separate
loopback server; the public phone-testing instance uses the same executable.
The Firefox/WebKit configuration and portability fixes accompany this report.

## Scope

The graph/guide suite checks both languages, narrow/desktop viewports, camera
movement, axis labels, cached zoom, domain gaps, poles, extrema, dense bands,
subpixel geometry, multiple functions, colors, inspection, expanded view,
cancellation and malformed/delayed responses. Independent HTTP/reference tests
also run, but only UI tests exercise a browser engine.

The engine revisions are pinned by Playwright 1.63.0: Firefox 155.0 (1543) and
WebKit 26.6 (2359) on Windows. This is local optional coverage; the existing
Chromium CI configuration is unchanged.

Three native multi-touch tests require Chromium's DevTools protocol and skip
explicitly in Firefox/WebKit. Ordinary touch taps are tested there. Engine
emulation does not establish Safari/iOS compatibility or physical-phone
gesture performance.

## Test portability

Firefox delivers native mouse coordinates rounded to CSS pixels. The inspector
test previously assumed the requested fractional coordinate was delivered
exactly. It now observes the actual pointer event, independently projects it
back into graph coordinates and retains the five-decimal coordinate comparison
and analytic curve check. The delivered event must remain within one CSS pixel
of the requested position. Exact zero/localized label tests separately inject
an exact pointer coordinate. No production tolerance was loosened.

Firefox can also scroll a focused control while Playwright prepares its click,
especially in a short landscape viewport. The expanded-view test records the
page position at the actual opening click and checks exact restoration to that
position. A separate diagnostic confirmed the application restores that position;
the earlier measurement preceded the browser's own pre-click scrolling.

The axis-label test waits for the requested camera to be painted before reading
ticks. WebKit exposed a race where synthetic key events completed before the
scheduled animation frame. Expected camera bounds and label checks are retained.
The deep-zoom limit test likewise waits for the initial burst to be painted
before checking that another zoom cannot change the bounded camera.

## Results

The initial 316-case run completed in 12.1 minutes: 302 passed, six explicitly
skipped, and eight test-portability failures described above. All eight failures
were reproduced and corrected in the tests; no application code or numerical
tolerance changes were needed.

Corrected Firefox inspection/multiple-function/subpixel suites then passed
25 tests with two expected CDP skips. Corrected axis-label and deep-zoom checks
each passed in both engines. A Chromium compatibility run passed all 33 camera,
inspection, multiple-function and subpixel tests; the native pinch check and
updated deep-zoom check also passed separately.
The final combined regression run passed all 16 selected Firefox/WebKit cases
in 50.7 seconds on the finished test sources.

Across the broad run and corrected reruns, all **310 executable Firefox/WebKit
cases** have passed; six CDP-only cases remain explicitly skipped there. This
count combines runs, rather than claiming the initial full run was green.
The landscape expanded-view screenshot was also inspected.

The public tunnel was checked with HTTP 200 for the graph page and latest asset,
plus an actual graph POST with matching Origin. The mixed subpixel reproduction
returned 6673 points, 6673 slopes, `subpixel_hermite:true` and `limited:false`.
No push, release or version change is part of this check.

## Physical-phone checklist

Try `sin(x)`, `x*x`, `1/x` and `sin(x*x)+cos(1000x)`:

1. Pinch in and out repeatedly; pan horizontally and vertically. Check that
   curves remain visible and settle without stale loading indicators.
2. Verify that the parabola has no artificial gap and the reciprocal never
   connects across zero. Default reset should restore −10..10 on both axes.
3. Tap/drag near a curve to inspect it; use a second finger to resume pinch.
4. Add a second function, change colors and hide/show curves.
5. Expand the plot, rotate the phone, close it and reset. Check layout, scroll
   behavior and that controls remain reachable.

Record device, OS/browser, expression and gesture if a problem appears. A
successful automated run alone does not complete this physical-device check.
