# Screen-space connected curves, 2026-10-13

Compare the Release MSVC server at `f4b7f8b` with the horizontal screen-floor
change. Windows localhost HTTP, three serial requests per expression, median
elapsed time through receipt of the response bytes (JSON parsing excluded).
These are local smoke measurements, not portable timing guarantees or C-only
arithmetic benchmarks. Both servers use the same machine/build configuration.

Request: `POST /api/graph/plot?mode=fast&min=-30&max=30&ymin=-10&ymax=10&width=2048&height=440`,
with the expression in the plain-text body. Visible camera: x/y −10..10.

| Expression | Before points | After points | Before HTTP ms | After HTTP ms | Before limited | After limited |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| sin(x) | 277 | 277 | 1.822 | 1.981 | no | no |
| sin(100x) | 8192 | 6007 | 133.186 | 31.503 | yes | no |
| exp(x) | 8192 | 1805 | 128.039 | 8.408 | yes | no |
| x^10 | 8192 | 3973 | 127.993 | 24.713 | yes | no |
| sin(1000x) | 8192 | 8192 | 125.236 | 143.490 | yes | yes |
| 1/(x-0.1)^2 | 2215 | 2215 | 27.560 | 26.785 | yes | yes |

sin(100x) previously had 1928 disconnected real edges whose endpoint segments
intersected the visible camera; it now has none. All sampled edges in its entire
buffer, exp(x) and x^10 are connected. exp(x)/x^10 previously exhausted work
outside the displayed y range, even though their visible edge gaps were zero.
The browser no longer turns that global flag into an unconditional warning.

The change allows two half-pixel chords in recognized globally continuous trees,
with the existing C phase/domain checks. It is a horizontal display floor, not
a certified vertical error bound. Actual sin/cos peak samples remain in use.
The unchanged sin(1000x) gaps are intentional: the available point/pixel density
cannot resolve those periods. The pole case retains its checks and domain gaps;
`limited` remains meaningful for API clients even when those gaps are off-screen.

Regression coverage includes complete connection checks, pole crossings with
positive squared denominators, high-frequency rejection, original non-camera
pixel accuracy, visible-only warnings under x/y pan and atomic C failures.
