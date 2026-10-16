const {test, expect} = require('@playwright/test');

const factorial = x => {
    if (!Number.isInteger(x) || x < 0) return NaN;
    let value = 1;
    for (let i = 2; i <= x; i++) value *= i;
    return value;
};
const scenes = [
    {name: 'oscillations and corners', expressions: ['sin(33x)', 'cos(17x)', 'abs(x*sin(x))', 'x*x/5'],
        references: [x => Math.sin(33*x), x => Math.cos(17*x), x => Math.abs(x*Math.sin(x)), x => x*x/5]},
    {name: 'poles and real domain edges', expressions: ['1/(x-0.37)', 'sqrt(x-0.2)', 'ln(x+2)', '1/ln(ln(sqrt(x)))^2'],
        references: [x => 1/(x-.37), x => Math.sqrt(x-.2), x => Math.log(x+2), x => x > 1 ? 1/Math.log(Math.log(Math.sqrt(x)))**2 : NaN]},
    {name: 'narrow peaks and tangent contacts', expressions: ['exp(-((x-0.1234)/0.04)^2)', 'abs(x-0.1234)', '(x-0.1234)^2', '1/(1+((x+0.43127)/0.04)^2)'],
        references: [x => Math.exp(-(((x-.1234)/.04)**2)), x => Math.abs(x-.1234), x => (x-.1234)**2, x => 1/(1+((x+.43127)/.04)**2)]},
    {name: 'integer points beside continuous and step curves', expressions: ['x!', 'ncr(x;3)', 'x', 'floor(x)'],
        references: [factorial, x => Number.isInteger(x) && x >= 3 ? x*(x-1)*(x-2)/6 : NaN, x => x, Math.floor]},
];

for (const width of [390, 1280]) for (const scene of scenes) {
    test(`release camera cycle: ${scene.name} at ${width}`, async ({page}) => {
        test.setTimeout(45000);
        const errors = [], requests = [], responses = [], active = new Set();
        let lastActivity = Date.now();
        page.on('pageerror', error => errors.push(error.message));
        page.on('request', request => {
            if (request.url().includes('/api/graph/')) {
                requests.push(request.url()); active.add(request); lastActivity = Date.now();
            }
        });
        const finished = request => { if (active.delete(request)) lastActivity = Date.now(); };
        page.on('requestfinished', finished);
        page.on('requestfailed', finished);
        page.on('response', response => {
            if (response.url().includes('/api/graph/')) responses.push(response.status());
        });
        await page.setViewportSize({width, height: 900});
        await page.goto('/graph?lang=en');
        for (let i = 0; i < scene.expressions.length; i++) {
            if (i) await page.locator('#graph-add').click();
            await page.locator('.graph-curve').nth(i).locator('input:not([type=color])').fill(scene.expressions[i]);
        }
        const svg = page.locator('#graph-plot');
        const observations = [];
        async function check(label) {
            // Camera sampling is debounced by 90 ms and marker verification by
            // another 120 ms. Do not mistake the previous frame for settled work.
            lastActivity = Date.now();
            await expect.poll(() => !active.size && Date.now()-lastActivity > 300, {timeout: 15000}).toBe(true);
            await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy', 'true');
            await expect(svg).toHaveAttribute('data-features-state', /ready|limited/);
            await expect(page.locator('.graph-curve-status.error:not([hidden])')).toHaveCount(0);
            await expect(page.locator('#graph-status')).not.toHaveClass(/error/);
            const markers = await page.locator('.graph-feature').evaluateAll(nodes => nodes.map(node => ({
                x: Number(node.dataset.x), y: Number(node.dataset.y), kind: node.dataset.kind,
                curve: Number(node.dataset.curve)-1, other: Number(node.dataset.other)-1,
                verified: node.dataset.verified,
            })));
            expect(markers.length).toBeLessThanOrEqual(128);
            for (const marker of markers) {
                expect(marker.verified).toBe('true');
                expect(Number.isFinite(marker.x) && Number.isFinite(marker.y)).toBe(true);
                const actual = scene.references[marker.curve](marker.x);
                expect(Number.isFinite(actual), JSON.stringify(marker)).toBe(true);
                const tolerance = 1e-7 * Math.max(1, Math.abs(actual), Math.abs(marker.y));
                expect(Math.abs(actual-marker.y), JSON.stringify(marker)).toBeLessThan(tolerance);
                if (marker.kind === 'x-axis') expect(marker.y).toBe(0);
                if (marker.kind === 'y-axis') expect(marker.x).toBe(0);
                if (marker.kind === 'intersection') {
                    const other = scene.references[marker.other](marker.x);
                    expect(Number.isFinite(other), JSON.stringify(marker)).toBe(true);
                    expect(Math.abs(actual-other), JSON.stringify(marker)).toBeLessThan(tolerance);
                }
            }
            const dots = await page.locator('.graph-discrete-point').evaluateAll(nodes => nodes.map(node => ({
                x: Number(node.dataset.x), y: Number(node.dataset.y), curve: Number(node.dataset.curve)-1,
            })));
            for (const dot of dots) {
                expect(Number.isSafeInteger(dot.x)).toBe(true);
                expect(dot.y).toBeCloseTo(scene.references[dot.curve](dot.x), 7);
            }
            const drawing = await svg.evaluate(node => ({
                paths: node.querySelectorAll('.graph-line, .graph-band').length,
                invalid: [...node.querySelectorAll('path')].some(path => /NaN|Infinity/.test(path.getAttribute('d') || '')),
            }));
            expect(drawing.invalid).toBe(false);
            expect(drawing.paths + dots.length).toBeGreaterThan(0);
            observations.push({label, markers: markers.length, dots: dots.length, requests: requests.length});
        }
        await page.locator('#graph-submit').click();
        await check('initial');
        for (const [label, keys] of [
            ['zoom and pan', ['+', '+', 'ArrowRight', 'ArrowUp']],
            ['wide and opposite pan', ['-', '-', '-', 'ArrowLeft', 'ArrowDown']],
        ]) {
            await svg.evaluate((node, keys) => {
                for (const key of keys) node.dispatchEvent(new KeyboardEvent('keydown', {key, bubbles: true, cancelable: true}));
            }, keys);
            await check(label);
        }
        await page.locator('#graph-reset').click();
        await check('reset');
        for (const axis of ['x', 'y']) {
            await expect(svg).toHaveAttribute(`data-${axis}min`, '-10');
            await expect(svg).toHaveAttribute(`data-${axis}max`, '10');
            expect(await page.locator(`.graph-tick[data-axis="${axis}"]`).allTextContents()).toContain('0');
        }
        expect(errors).toEqual([]);
        expect(responses.every(status => status === 200)).toBe(true);
        await test.info().attach('camera-cycle.json', {body: JSON.stringify(observations), contentType: 'application/json'});
        const screenshot = test.info().outputPath('reset-view.png');
        await page.locator('.graph-result').screenshot({path: screenshot});
        await test.info().attach('reset-view', {path: screenshot, contentType: 'image/png'});
    });
}
