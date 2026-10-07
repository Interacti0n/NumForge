const { test, expect } = require('@playwright/test');
const fs = require('node:fs');
const path = require('node:path');

test('HTTP sessions: confirmation replay, isolation and expiration', async ({ request }) => {
    const client = 'b'.repeat(32);
    async function send(id, revision, action, input = '', precision = 10) {
        const response = await request.post('/api/evaluate?precision=' + precision +
            '&angle=rad&client=' + id + '&revision=' + revision + '&action=' + action, {data: input});
        return response.json();
    }
    expect((await send(client, 1, 'preview', 'ans')).status).toContain('session expired');
    expect((await send(client, 1, 'start')).ok).toBe(true);
    expect((await send(client, 1, 'preview', 'ans')).status).toBe('ans is undefined');
    expect((await send(client, 2, 'commit', '1/8', 2)).result).toBe('0.12');
    expect((await send(client, 3, 'preview', 'ans', 'full')).result).toBe('1/8');
    expect((await send(client, 4, 'commit', 'ans+1', 'full')).result).toBe('9/8');
    expect((await send(client, 4, 'commit', 'ans+1', 'full')).result).toBe('9/8');
    expect((await send(client, 5, 'preview', 'ans+1', 'full')).result).toBe('2.125');
    expect((await send(client, 3, 'commit', 'ans+1')).status).toBe('stale session request');
    expect((await send(client, 6, 'commit', '1/0')).ok).toBe(false);
    expect((await send(client, 7, 'preview', 'ans', 'full')).result).toBe('9/8');
    for (let index = 20; index < 28; index++) {
        const other = index.toString(16).padStart(32, '0');
        expect((await send(other, 1, 'start')).ok).toBe(true);
        expect((await send(other, 1, 'preview', 'ans')).status).toBe('ans is undefined');
    }
    expect((await send(client, 4, 'commit', 'ans+1', 'full')).status).toContain('session expired');
});

test('HTTP random previews remain stable and confirmation adopts them', async ({ request }) => {
    const client = 'c'.repeat(32);
    async function send(revision, action, input = '', precision = 'full') {
        const response = await request.post('/api/evaluate?precision=' + precision +
            '&angle=rad&client=' + client + '&revision=' + revision + '&action=' + action, {data: input});
        return response.json();
    }
    expect((await send(1, 'start')).ok).toBe(true);
    const preview = await send(1, 'preview', 'rand()+rand()');
    expect(preview.ok).toBe(true);
    expect((await send(2, 'preview', 'rand()+rand()')).result).toBe(preview.result);
    expect((await send(3, 'commit', 'rand()+rand()')).result).toBe(preview.result);
    expect((await send(3, 'commit', 'rand()+rand()')).result).toBe(preview.result);
    const ranged = await send(4, 'commit', 'rand(2;5)');
    expect(ranged.ok).toBe(true);
    expect(Number(ranged.result)).toBeGreaterThanOrEqual(2);
    expect(Number(ranged.result)).toBeLessThan(5);
    expect((await send(5, 'commit', 'rand(0)')).ok).toBe(false);
});

test('HTTP notation changes preserve the confirmed number and expose copy text', async ({ request }) => {
    const client = 'd'.repeat(32);
    async function send(revision, action, input, notation) {
        const response = await request.post('/api/evaluate?precision=2&angle=rad&notation=' + notation +
            '&client=' + client + '&revision=' + revision + '&action=' + action, {data: input});
        return response.json();
    }
    expect((await send(1, 'start', '', 'auto')).ok).toBe(true);
    expect((await send(1, 'commit', '123.456', 'plain')).result).toBe('123.46');
    const math = await send(2, 'preview', 'ans', 'math');
    expect(math.result).toBe('1.2346 × 10^2');
    expect(math.copy).toBe('1.2346E+2');
    expect((await send(3, 'preview', 'ans', 'scientific')).result).toBe('1.2346E+2');
    expect((await send(4, 'preview', 'ans', 'plain')).result).toBe('123.46');
    expect((await send(5, 'commit', '1E100000', 'plain')).ok).toBe(false);
    expect((await send(6, 'preview', 'ans', 'auto')).result).toBe('123.46');
});

test('HTTP cache validation, stale revisions and bounded eviction', async ({ request }) => {
    const first = 'a'.repeat(32);
    async function send(client, revision, input = '42', precision = 10) {
        return request.post('/api/evaluate?precision=' + precision +
            '&angle=rad&client=' + client + '&revision=' + revision, {data: input});
    }
    expect((await (await send(first, 1)).json()).cached).toBe(false);
    expect((await (await send(first, 3)).json()).cached).toBe(true);
    expect((await (await send(first, 2, '7')).json()).cached).toBe(false);
    expect((await (await send(first, 4)).json()).cached).toBe(true);
    for (const [client, revision] of [[first, '0'], [first, '-1'], [first, '1x'],
        [first, '999999999999999999'], ['z'.repeat(32), '1'], ['a', '1']]) {
        expect((await send(client, revision)).status()).toBe(400);
    }
    for (let n = 1; n <= 8; n++) {
        expect((await (await send(n.toString(16).padStart(32, '0'), 1)).json()).cached).toBe(false);
    }
    expect((await (await send(first, 5)).json()).cached).toBe(false);
    expect((await (await send(first, 6, '42', 10001)).json()).ok).toBe(false);
    expect((await (await send(first, 7)).json()).cached).toBe(true);
});


test('pages stay hidden until their stylesheet is available', async ({ page }) => {
    for (const path of ['/?lang=sk', '/api?lang=en']) {
        let releaseStyles;
        const stylesReleased = new Promise(resolve => { releaseStyles = resolve; });
        let requestStyles;
        const stylesRequested = new Promise(resolve => { requestStyles = resolve; });
        await page.route('**/assets/*.css', async route => {
            requestStyles();
            await stylesReleased;
            await route.continue();
        });
        try {
            await page.goto(path, {waitUntil: 'commit'});
            await stylesRequested;
            await page.locator('body').waitFor({state: 'attached'});
            await expect(page.locator('body')).toHaveCSS('visibility', 'hidden');
        } finally {
            releaseStyles();
        }
        await expect(page.locator('body')).toHaveCSS('visibility', 'visible');
        await page.unroute('**/assets/*.css');
    }
});

test('header logo and favicon use the same complete PNG', async ({ page, request }) => {
    const original = fs.readFileSync(path.join(__dirname, '../../web/logo.png'));
    const response = await request.get('/assets/logo.png');
    expect(response.ok()).toBe(true);
    expect(response.headers()['content-type']).toBe('image/png');
    expect(Number(response.headers()['content-length'])).toBe(original.length);
    expect((await response.body()).equals(original)).toBe(true);
    for (const route of ['/?lang=sk', '/?lang=en', '/api?lang=sk', '/api?lang=en']) {
        await page.goto(route);
        await expect(page.locator('link[rel="icon"]')).toHaveAttribute('href', '/assets/logo.png');
        await expect(page.locator('.brand-mark')).toHaveAttribute('src', '/assets/logo.png');
        expect(await page.locator('.brand-mark').evaluate(image => image.complete && image.naturalWidth > 0)).toBe(true);
    }
});

test('both guide titles use the complete NumForge wordmark', async ({ page, request }) => {
    const original = fs.readFileSync(path.join(__dirname, '../../web/wordmark.png'));
    const response = await request.get('/assets/wordmark.png');
    expect(response.ok()).toBe(true);
    expect(response.headers()['content-type']).toBe('image/png');
    expect(Number(response.headers()['content-length'])).toBe(original.length);
    expect((await response.body()).equals(original)).toBe(true);
    for (const [language, title] of [['sk', 'Použitie a API'], ['en', 'Usage and API']]) {
        await page.goto(`/api?lang=${language}`);
        const wordmark = page.locator('.guide-title .guide-wordmark');
        await expect(wordmark).toHaveAttribute('alt', 'NumForge');
        expect(await wordmark.evaluate(image => image.complete && image.naturalWidth > 0)).toBe(true);
        await expect(page.locator('.guide-title span')).toHaveText(title);
        const image = await wordmark.boundingBox();
        const content = await page.locator('.guide-content').boundingBox();
        expect(image.width).toBeLessThan(content.width);
    }
});

test('header controls keep their size and position when changing language', async ({ page }) => {
    for (const width of [1280, 1024]) {
        await page.setViewportSize({width, height: 768});
        await page.goto('/?lang=sk');
        const geometry = () => page.evaluate(() => {
            const controls = [...document.querySelectorAll('.brand, .primary-nav a, .language-switch, .header-actions > a')];
            return {
                boxes: controls.map(control => {
                    const box = control.getBoundingClientRect();
                    return {x: box.x, width: box.width, height: box.height,
                        fits: control.scrollWidth <= control.clientWidth + 1};
                }),
                pageFits: document.documentElement.scrollWidth <= innerWidth + 1
            };
        });
        const slovak = await geometry();
        await page.locator('.language-switch a[lang="en"]').click();
        await expect(page.locator('html')).toHaveAttribute('lang', 'en');
        const english = await geometry();
        expect(slovak.pageFits && english.pageFits).toBe(true);
        for (let index = 0; index < slovak.boxes.length; index++) {
            expect(slovak.boxes[index].fits && english.boxes[index].fits,
                `width ${width}, control ${index}: ${JSON.stringify([slovak.boxes[index], english.boxes[index]])}`).toBe(true);
            for (const dimension of ['x', 'width', 'height'])
                expect(Math.abs(slovak.boxes[index][dimension] - english.boxes[index][dimension])).toBeLessThan(1);
        }
    }
});

for (const lang of ['sk', 'en']) {
    test.describe(lang, () => {
        test.beforeEach(async ({ page }) => { await page.goto(`/?lang=${lang}`); });
        async function calculate(page, input, expected) {
            await page.locator('#expression').fill(input);
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toHaveText(expected);
        }
        test('text and mathematical values use their respective font families', async ({ page }) => {
            await calculate(page, '1/3', '1/3');
            const fonts = await page.evaluate(() => {
                const font = selector => getComputedStyle(document.querySelector(selector)).fontFamily;
                return {
                    ui: font('.primary-nav'), settings: font('.precision'),
                    category: font('#function-tab-0'), help: font('#function-help'),
                    input: font('#expression'), result: font('#result'),
                    historyText: font('#history-list button'), historyValue: font('#history-list .history-value'),
                    numericKey: font('.number-keypad [data-insert="7"]'),
                    actionKey: font('.edit-keypad [data-action="clear"]'),
                    functionKey: font('.keypad.functions button')
                };
            });
            for (const name of ['settings', 'category', 'help', 'historyText', 'actionKey'])
                expect(fonts[name]).toBe(fonts.ui);
            for (const name of ['result', 'historyValue', 'numericKey', 'functionKey'])
                expect(fonts[name]).toBe(fonts.input);
            expect(fonts.ui).not.toBe(fonts.input);
        });
        test('notation selector changes display and mathematical copy', async ({ page }) => {
            await page.locator('#precision-mode').selectOption('custom');
            await page.locator('#precision').fill('2');
            await page.locator('#notation-mode').selectOption('math');
            await calculate(page, '123.456', '1.2346 × 10^2');
            await page.locator('#copy-result').click();
            expect(await page.evaluate(() => navigator.clipboard.readText())).toBe('1.2346E+2');
            await page.locator('#history-list .history-copy').click();
            expect(await page.evaluate(() => navigator.clipboard.readText())).toBe('1.2346E+2');
            await page.locator('#notation-mode').selectOption('plain');
            await expect(page.locator('#result')).toHaveText('123.46');
            await page.locator('#notation-mode').selectOption('scientific');
            await expect(page.locator('#result')).toHaveText('1.2346E+2');
            await expect(page.locator('#history-list li')).toHaveCount(1);
        });
        test('fraction notation changes only the display', async ({ page }) => {
            await calculate(page, '1/3', '1/3');
            await expect(page.locator('#result-approx')).toHaveText('≈ 0.3333333333');
            await page.locator('#notation-mode').selectOption('plain');
            await expect(page.locator('#result')).toHaveText('0.3333333333');
            await expect(page.locator('#result-approx')).toBeHidden();
            await page.locator('#notation-mode').selectOption('fraction');
            await expect(page.locator('#result')).toHaveText('1/3');
            await expect(page.locator('#result-approx')).toHaveText('≈ 0.3333333333');
            await expect(page.locator('#history-list li')).toHaveCount(1);
            await calculate(page, 'sqrt(2)', '1.4142135624');
            await expect(page.locator('#result-approx')).toBeHidden();
            await calculate(page, '123456789012345/7', '1.7636684145E+13');
        });
        test('ans changes only on confirmation; history and reload', async ({ page, context }) => {
            await calculate(page, '5', '5');
            await page.locator('#expression').fill('ans+1');
            await expect(page.locator('#result')).toHaveText('6');
            await expect(page.locator('#history-list li')).toHaveCount(1);
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#history-list li')).toHaveCount(2);
            await expect(page.locator('#result')).toHaveText('6');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toHaveText('7');
            await expect(page.locator('#history-list li')).toHaveCount(3);
            await page.locator('#expression').fill('2+3');
            await page.locator('#expression').evaluate(input => input.setSelectionRange(2, 3));
            await page.locator('#history-list .history-entry').first().click();
            await expect(page.locator('#expression')).toHaveValue('2+5');
            await page.locator('#history-list .history-copy').first().click();
            expect(await page.evaluate(() => navigator.clipboard.readText())).toBe('5');
            await page.locator('#expression').fill('1+');
            await page.locator('#session-title').click();
            await page.locator('#history-list .history-entry').first().click();
            await expect(page.locator('#expression')).toHaveValue('1+5');
            await expect(page.locator('#history-list li')).toHaveCount(3);
            await calculate(page, 'ans', '7');
            const other = await context.newPage();
            await other.goto('/?lang=' + lang);
            await other.locator('#expression').fill('ans');
            await other.locator('#expression').press('Enter');
            await expect(other.locator('#result')).toContainText(lang === 'sk' ? 'nemá potvrdenú' : 'undefined');
            await other.close();
            await page.locator('#reset-session').click();
            await expect(page.locator('#history-list li')).toHaveCount(0);
            await page.locator('#expression').fill('ans');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toContainText(lang === 'sk' ? 'nemá potvrdenú' : 'undefined');
            await calculate(page, '1', '1');
            await expect(page.locator('#history-list .history-index')).toHaveText(['#1']);
        });
        test('long history stays on one line and marks the hidden end', async ({ page }) => {
            const input = '1' + '+1'.repeat(80);
            await calculate(page, input, '81');
            const line = page.locator('#history-list .history-line');
            await expect(line).toHaveClass(/is-truncated/);
            const height = await line.evaluate(element => element.getBoundingClientRect().height);
            const lineHeight = await line.evaluate(element => parseFloat(getComputedStyle(element).lineHeight));
            expect(height).toBeLessThanOrEqual(lineHeight + 1);
            await page.locator('#history-list .history-copy').click();
            expect(await page.evaluate(() => navigator.clipboard.readText())).toBe('81');
        });
        test('real C calculation, precision, buttons and clipboard', async ({ page }) => {
            await calculate(page, '0,1+0.2', '0.3');
            await page.locator('#copy-result').click();
            await expect.poll(() => page.evaluate(() => navigator.clipboard.readText())).toBe('0.3');
            if (await page.locator('#precision-mode').inputValue() !== 'custom')
                await page.locator('#precision-mode').selectOption('custom');
            await page.locator('#precision').fill('2');
            await calculate(page, '1/8', '0.12');
            await page.locator('#precision-mode').selectOption('full');
            await expect(page.locator('#precision')).toBeDisabled();
            await calculate(page, '1/8', '1/8');
            await calculate(page, '((1E80+1)/8)*8-1E80', '1');
            await page.locator('[data-action=clear]').click();
            await expect(page.locator('#expression')).toHaveValue('');
            await expect(page.locator('#copy-result')).toBeDisabled();
            for (const value of ['2', '+', '3']) await page.locator(`[data-insert="${value}"]`).click();
            await page.locator('[data-action=evaluate]').click();
            await expect(page.locator('#result')).toHaveText('5');
            await page.locator('[data-action=backspace]').click();
            await expect(page.locator('#expression')).toHaveValue('2+');
            await page.locator('[data-insert="π"]').click();
            await expect(page.locator('#expression')).toHaveValue('2+π');
        });
        test('lost confirmation response retries the original committed value', async ({ page }) => {
            await calculate(page, '5', '5');
            await page.route('**/api/evaluate*', async route => {
                if (!route.request().url().includes('&action=commit')) return route.continue();
                await route.fetch(); // The C server commits, but the browser never receives its reply.
                await route.abort();
            });
            await page.locator('#expression').fill('ans+1');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toContainText(lang === 'sk' ? 'neisté' : 'uncertain');
            await page.unroute('**/api/evaluate*');
            await page.locator('#expression').fill('999');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#expression')).toHaveValue('ans+1');
            await expect(page.locator('#result')).toHaveText('6');
            await expect(page.locator('#history-list li')).toHaveCount(2);
            await calculate(page, 'ans', '6');
        });
        test('errors and language navigation on both pages', async ({ page }) => {
            await page.locator('#expression').fill('1/0');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toContainText(lang === 'sk' ? 'delenie nulou' : 'division by zero');
            await expect(page.locator('#copy-result')).toBeDisabled();
            await page.locator('.guide-link').click();
            await expect(page.locator('html')).toHaveAttribute('lang', lang);
            await expect(page.locator('body')).toContainText('bigdecimal_mean');
            const other = lang === 'sk' ? 'en' : 'sk';
            await page.locator(`a[href="/api?lang=${other}"]`).click();
            await expect(page.locator('html')).toHaveAttribute('lang', other);
            await page.locator(`.back-link[href="/?lang=${other}"]`).click();
            await calculate(page, '2(2+2)', '8');
        });
        test('page navigation keeps the header and footer mounted', async ({ page }) => {
            await page.evaluate(() => {
                window.originalHeader = document.querySelector('.page-header');
                window.originalFooter = document.querySelector('.page-footer');
                window.originalMenu = document.querySelector('.primary-nav');
            });
            await page.locator('.guide-link').click();
            await expect(page).toHaveURL(`/api?lang=${lang}`);
            expect(await page.evaluate(() => document.querySelector('.page-header') === window.originalHeader &&
                document.querySelector('.page-footer') === window.originalFooter &&
                document.querySelector('.primary-nav') === window.originalMenu)).toBe(true);
            await page.locator('.guide-toc a[href="#http"]').click();
            await expect(page).toHaveURL(/#http$/);
            const other = lang === 'sk' ? 'en' : 'sk';
            await page.locator(`.language-switch a[lang="${other}"]`).click();
            await expect(page).toHaveURL(`/api?lang=${other}`);
            expect(await page.evaluate(() => document.querySelector('.page-header') === window.originalHeader &&
                document.querySelector('.page-footer') === window.originalFooter &&
                document.querySelector('.primary-nav') === window.originalMenu)).toBe(true);
            await page.emulateMedia({reducedMotion: 'reduce'});
            await page.locator('.back-link').click();
            await expect(page).toHaveURL(`/?lang=${other}`);
            expect(await page.evaluate(() => document.querySelector('.page-header') === window.originalHeader &&
                document.querySelector('.page-footer') === window.originalFooter &&
                document.querySelector('.primary-nav') === window.originalMenu)).toBe(true);
            await page.goBack();
            await expect(page).toHaveURL(`/api?lang=${other}`);
            await expect(page.locator('.guide-content')).toBeVisible();
            await page.locator(`.primary-nav a[href="/graph?lang=${other}"]`).click();
            await expect(page).toHaveURL(`/graph?lang=${other}`);
            await expect(page.locator('#upcoming-title')).toBeVisible();
            await page.locator('.register-link').click();
            await expect(page).toHaveURL(`/register?lang=${other}`);
            expect(await page.evaluate(() => document.querySelector('.page-header') === window.originalHeader &&
                document.querySelector('.page-footer') === window.originalFooter &&
                document.querySelector('.primary-nav') === window.originalMenu)).toBe(true);
        });
        test('automatic calculation and precision above result', async ({ page }, testInfo) => {
            await expect(page.locator('.page-footer')).toBeVisible();
            await expect(page.locator('.page-footer')).toContainText('v2.0.0');
            await expect(page.locator('.page-footer a[href$="/LICENSE"]')).toBeVisible();
            await expect(page.locator('.page-footer a[href="https://github.com/Interacti0n/NumForge"]')).toBeVisible();
            const settings = await page.locator('.precision').boundingBox();
            const input = await page.locator('.expression-card').boundingBox();
            const panel = await page.locator('.result-panel').boundingBox();
            expect(settings.y + settings.height).toBeLessThan(input.y);
            expect(settings.y + settings.height).toBeLessThan(panel.y);
            expect(settings.width).toBeGreaterThanOrEqual(input.width - 2);
            await expect(page.locator('#settings-title')).toBeVisible();
            await page.locator('#expression').fill('1/8');
            await expect(page.locator('#result')).toHaveText('1/8');
            if (await page.locator('#precision-mode').inputValue() !== 'custom')
                await page.locator('#precision-mode').selectOption('custom');
            await page.locator('#precision').fill('2');
            await expect(page.locator('#result')).toHaveText('0.12');
            await page.locator('#precision-mode').selectOption('full');
            await expect(page.locator('#result')).toHaveText('1/8');
            await page.locator('#expression').fill('2+');
            await page.locator('#expression').press('End');
            await page.locator('[data-insert="3"]').click();
            await expect(page.locator('#result')).toHaveText('5');
            await page.locator('[data-action=backspace]').click();
            await expect(page.locator('#result')).toContainText(lang === 'sk' ? 'Chyba' : 'Error');
            await page.locator('[data-insert="4"]').click();
            await expect(page.locator('#result')).toHaveText('6');
            await page.screenshot({path: testInfo.outputPath('automatic-desktop.png'), fullPage: true});
            await page.setViewportSize({width: 375, height: 812});
            await page.screenshot({path: testInfo.outputPath('automatic-mobile.png'), fullPage: true});
            await page.locator('[data-action=clear]').click();
            await expect(page.locator('#result')).toBeEmpty();
        });
        test('function groups, aliases and trigonometry', async ({ page }, testInfo) => {
            test.setTimeout(30000);
            await expect(page.locator('details.function-group')).toHaveCount(6);
            await expect(page.locator('[data-function]')).toHaveCount(46);
            await expect(page.locator('[data-function]:disabled')).toHaveCount(0);
            await page.locator('#function-tab-0').click();
            await page.locator('[data-function="rand"]').click();
            await expect(page.locator('#expression')).toHaveValue('rand()');
            await expect(page.locator('#expression')).toHaveJSProperty('selectionStart', 6);
            await page.locator('[data-action="clear"]').click();
            await page.locator('[data-function="round"]').click();
            await expect(page.locator('#expression')).toHaveValue('round()');
            await page.locator('#expression').fill('round(12.345;2)');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toHaveText('12.34');
            await calculate(page, 'mean(1;2;2)', '5/3');
            await page.locator('[data-action="clear"]').click();
            await page.locator('#function-tab-1').click();
            await page.locator('[data-function="median"]').click();
            await expect(page.locator('#expression')).toHaveValue('median()');
            await calculate(page, 'harmean(1;2;4)', '12/7');
            await page.locator('[data-action="clear"]').click();
            await page.locator('[data-function="variance"]').click();
            await expect(page.locator('#expression')).toHaveValue('variance()');
            await calculate(page, 'stdev(1;2;3)', '1');
            await page.locator('[data-action="clear"]').click();
            const powers = page.locator('details').filter({ has: page.locator('[data-function="pow"]') });
            await page.locator('#function-tab-3').focus();
            await page.keyboard.press('Enter');
            await page.locator('[data-function="pow"]').click();
            await expect(page.locator('#expression')).toHaveValue('pow()');
            expect(await page.locator('#expression').evaluate(el => el.selectionStart)).toBe(4);
            await page.locator('[data-insert="2"]').click();
            await page.locator('[data-insert=";"]').click();
            await page.locator('[data-insert="3"]').click();
            await page.locator('[data-action=evaluate]').click();
            await expect(page.locator('#result')).toHaveText('8');
            await page.locator('#expression').press('Enter');
            expect(await page.locator('#expression').evaluate(el => el.selectionStart)).toBe(8);
            await calculate(page, '2^-3', '1/8');
            await calculate(page, 'factorial(5)', '120');
            await page.locator('[data-action="clear"]').click();
            await page.locator('[data-function="exp"]').click();
            await expect(page.locator('#expression')).toHaveValue('exp()');
            await page.locator('#expression').fill('exp(0)');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toHaveText('1');
            await calculate(page, 'ln(1)', '0');
            await calculate(page, 'log(100)', '2');
            await calculate(page, 'log(8;2)', '3');
            const angles = page.locator('details').filter({ has: page.locator('[data-function="sin"]') });
            await page.locator('#function-tab-4').click();
            await expect(page.locator('details.function-group:visible')).toHaveCount(1);
            await expect(page.locator('[data-angle="rad"]')).toHaveClass(/active/);
            await page.locator('[data-action="clear"]').click();
            await page.locator('[data-function="sin"]').click();
            await page.locator('#function-tab-3').click();
            await page.locator('[data-function="sqrt"]').click();
            await expect(page.locator('#expression')).toHaveValue('sin(sqrt())');
            expect(await page.locator('#expression').evaluate(el => el.selectionStart)).toBe(9);
            await page.locator('#function-tab-4').click();
            await calculate(page, 'sin(π/2)', '1');
            await page.locator('[data-angle="deg"]').click();
            await expect(page.locator('[data-angle="deg"]')).toHaveClass(/active/);
            await calculate(page, 'sin(90)', '1');
            await calculate(page, 'cos(180)', '-1');
            await calculate(page, 'tan(45)', '1');
            await calculate(page, 'asin(1)', '90');
            await calculate(page, 'arcsin(1)', '90');
            await calculate(page, 'arctan(1)', '45');
            await calculate(page, 'arcustan(1)', '45');
            await calculate(page, 'degrees(π)', '180');
            await page.locator('#function-tab-5').click();
            await page.locator('[data-action="clear"]').click();
            await page.locator('[data-function="sinh"]').click();
            await expect(page.locator('#expression')).toHaveValue('sinh()');
            await calculate(page, 'tanh(1)', '0.761594156');
            await calculate(page, 'acosh(2)', '1.3169578969');
            await calculate(page, 'arcuscosh(1)', '0');
            await page.locator('#expression').fill('atan(1;2)');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toContainText(lang === 'sk' ? 'nesprávny počet argumentov' : 'wrong number of arguments');
            await calculate(page, '1e3-1*e*3', '0');
            await page.screenshot({ path: testInfo.outputPath('functions-desktop.png'), fullPage: true });
            await page.setViewportSize({ width: 375, height: 812 });
            await expect.poll(() => page.evaluate(() =>
                document.documentElement.scrollWidth - window.innerWidth)).toBeLessThanOrEqual(1);
            await page.screenshot({ path: testInfo.outputPath('functions-mobile.png'), fullPage: true });
            await page.locator('.nav-toggle').click();
            await page.locator('.guide-link').click();
            await expect(page.locator('body')).toContainText('log(x;b)');
        });
        test('integer buttons execute through C and reject invalid domains', async ({ page }) => {
            const group = page.locator('details').filter({has: page.locator('[data-function="gcd"]')});
            await page.locator('#function-tab-2').click();
            for (const [name, args, expected] of [
                ['gcd', '-48;18)', '6'], ['lcm', '-4;6)', '12'],
                ['mod', '-7;3)', '-1'], ['npr', '5;2)', '20'], ['ncr', '5;2)', '10'],
                ['isqrt', '18446744073709551616)', '4294967296']
            ]) {
                await page.locator('[data-action=clear]').click();
                await page.locator(`[data-function="${name}"]`).click();
                await expect(page.locator('#expression')).toHaveValue(`${name}()`);
                await page.locator('#expression').pressSequentially(args.slice(0, -1));
                await page.locator('#expression').press('Enter');
                await expect(page.locator('#result')).toHaveText(expected);
            }
            await calculate(page, 'min(abs(-3);max(1;sign(-2)))', '1');
            await page.locator('#expression').fill('isqrt(-1)');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toContainText(lang === 'sk' ? 'neplatný argument' : 'invalid argument');
        });
        test('real roots, precision and domain errors', async ({ page }) => {
            const group = page.locator('details').filter({has: page.locator('[data-function="sqrt"]')});
            await page.locator('#function-tab-3').click();
            for (const [name, args, expected] of [
                ['sqrt', '2)', '1.4142135624'], ['cbrt', '-8)', '-2'], ['root', '-32;5)', '-2']
            ]) {
                await page.locator('[data-action=clear]').click();
                await page.locator(`[data-function="${name}"]`).click();
                await expect(page.locator('#expression')).toHaveValue(`${name}()`);
                await page.locator('#expression').pressSequentially(args.slice(0, -1));
                await page.locator('#expression').press('Enter');
                await expect(page.locator('#result')).toHaveText(expected);
            }
            await calculate(page, '√(0,25)', '0.5');
            if (await page.locator('#precision-mode').inputValue() !== 'custom')
                await page.locator('#precision-mode').selectOption('custom');
            await page.locator('#precision').fill('3');
            await calculate(page, 'sqrt(2)', '1.414');
            await page.locator('#precision-mode').selectOption('full');
            await expect(page.locator('#result')).toHaveText('1.414213562373095048801688724209698');
            await page.locator('#expression').fill('root(-16;4)');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toContainText(lang === 'sk' ? 'neplatný argument' : 'invalid argument');
            await page.locator('.guide-link').click();
            await expect(page.locator('body')).toContainText('root(x;n)');
        });
        test('result uses available panel height and expansion resets', async ({ page }) => {
            const result = page.locator('#result');
            await calculate(page, '42', '42');
            const shortResult = await result.boundingBox();
            const available = await page.locator('.result-body').boundingBox();
            expect(Math.abs(shortResult.y - available.y)).toBeLessThan(3);
            expect(shortResult.height).toBeLessThan(available.height);
            expect(await result.evaluate(el => getComputedStyle(el).borderLeftColor)).not.toBe('rgba(0, 0, 0, 0)');
            await page.locator('#precision-mode').selectOption('full');
            await calculate(page, '2^2000', (2n ** 2000n).toString()[0] + '.' + (2n ** 2000n).toString().slice(1) + 'E+602');
            expect((await result.boundingBox()).height).toBeGreaterThanOrEqual(
                (await page.locator('.result-body').boundingBox()).height - 8);
            await expect(page.locator('#expand-result')).toBeVisible();
            await expect(page.locator('#result-more-marker')).toHaveText('...');
            await expect(page.locator('#result-more-marker')).toBeVisible();
            await page.locator('#expand-result').click();
            await expect(page.locator('#result-dialog')).toBeVisible();
            await expect(page.locator('#result-full')).toHaveText((2n ** 2000n).toString()[0] + '.' + (2n ** 2000n).toString().slice(1) + 'E+602');
            expect((await result.boundingBox()).height).toBeGreaterThanOrEqual(
                (await page.locator('.result-body').boundingBox()).height - 8);
            await page.locator('#copy-result-dialog').click();
            await expect.poll(() => page.evaluate(() => navigator.clipboard.readText())).toBe((2n ** 2000n).toString()[0] + '.' + (2n ** 2000n).toString().slice(1) + 'E+602');
            await page.mouse.click(5, 5);
            await expect(page.locator('#result-dialog')).toBeHidden();
            await page.locator('#expand-result').click();
            await expect(page.locator('#result-dialog')).toBeVisible();
            await page.locator('#close-result-dialog').click();
            await expect(page.locator('#result-dialog')).toBeHidden();
            await calculate(page, '2^2000', (2n ** 2000n).toString()[0] + '.' + (2n ** 2000n).toString().slice(1) + 'E+602');
            await expect(page.locator('#expand-result')).toHaveText(lang === 'sk' ? 'Zobraziť všetko...' : 'Show all...');
        });
        test('long expression expands above a visible result while controls stay available', async ({ page }) => {
            const oneLine = (await page.locator('#expression').boundingBox()).height;
            expect(oneLine).toBeLessThan(50);
            await page.locator('#expression').fill('1+'.repeat(80) + '1');
            expect((await page.locator('#expression').boundingBox()).height).toBeGreaterThan(oneLine);
            await expect(page.locator('#expand-expression')).toBeHidden();
            const longExpression = '1+'.repeat(180) + '1';
            await page.locator('#expression').fill(longExpression);
            await expect(page.locator('#expand-expression')).toBeVisible();
            const heightLimit = await page.locator('#expression').evaluate(el => {
                const style = getComputedStyle(el);
                return 5 * parseFloat(style.lineHeight) + parseFloat(style.paddingTop) +
                    parseFloat(style.paddingBottom) + parseFloat(style.borderTopWidth) +
                    parseFloat(style.borderBottomWidth);
            });
            expect((await page.locator('#expression').boundingBox()).height).toBeLessThanOrEqual(heightLimit + 2);
            expect(await page.locator('#expression').evaluate(el => el.scrollHeight > el.clientHeight)).toBe(true);
            await page.locator('#expand-expression').click();
            await expect(page.locator('.expression-card')).toHaveClass(/is-expanded/);
            expect((await page.locator('#expression').boundingBox()).height).toBeGreaterThan(64);
            await page.locator('#expression').fill(longExpression + '/');
            await expect(page.locator('#result')).toContainText(lang === 'sk' ? 'Chyba' : 'Error');
            const inputBox = await page.locator('#expression').boundingBox();
            const resultText = await page.locator('#result').boundingBox();
            expect(inputBox.y + inputBox.height).toBeLessThan(resultText.y);
            expect(resultText.y + Math.min(resultText.height, 18)).toBeLessThanOrEqual(720);
            await page.locator('#expression').fill(longExpression);
            await page.locator('.number-keypad [data-insert="7"]').click();
            await expect(page.locator('#expression')).toHaveValue(longExpression + '7');
            await page.locator('#expression').fill('1+2');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('.expression-card')).not.toHaveClass(/is-expanded/);
            await expect(page.locator('#expression')).toHaveValue('1+2');
            await expect(page.locator('#result')).toHaveText('3');
            await expect(page.locator('#expand-expression')).toBeHidden();
            expect(await page.evaluate(() => document.documentElement.scrollHeight)).toBe(720);
            await page.setViewportSize({width: 375, height: 667});
            await page.locator('#expression').fill(longExpression);
            await expect(page.locator('#expand-expression')).toBeVisible();
            await page.locator('#expand-expression').click();
            await expect(page.locator('.expression-card')).toHaveClass(/is-expanded/);
            await page.locator('#expression').press('Escape');
            await expect(page.locator('.expression-card')).not.toHaveClass(/is-expanded/);
            expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1)).toBe(true);
        });
        test('session survives language and guide navigation in the same tab', async ({ page }) => {
            await calculate(page, '1/3', '1/3');
            await page.locator('#function-tab-4').click();
            await page.locator('.constants [data-insert="π"]').click();
            await page.locator('#expression').fill('ans+1');
            await expect(page.locator('#result')).toHaveText('4/3');
            await page.locator('#precision-mode').selectOption('full');
            await page.locator('[data-angle="deg"]').click();
            const other = lang === 'sk' ? 'en' : 'sk';
            await page.locator(`.language-switch a[lang="${other}"]`).click();
            await expect(page.locator('html')).toHaveAttribute('lang', other);
            await expect(page.locator('#expression')).toHaveValue('ans+1');
            await expect(page.locator('#precision-mode')).toHaveValue('full');
            await expect(page.locator('[data-angle="deg"]')).toHaveAttribute('aria-pressed', 'true');
            await expect(page.locator('#history-list li')).toHaveCount(1);
            await expect(page.locator('#history-list .history-index')).toHaveText(['#1']);
            await expect(page.locator('#recent-buttons button')).toHaveCount(1);
            await expect(page.locator('#function-tab-4')).toHaveAttribute('aria-selected', 'true');
            await page.locator('.guide-link').click();
            await page.locator('.back-link').click();
            await expect(page.locator('#history-list li')).toHaveCount(1);
            await expect(page.locator('#history-list .history-index')).toHaveText(['#1']);
            await expect(page.locator('#expression')).toHaveValue('ans+1');
            await expect(page.locator('#result')).toHaveText('4/3');
            await expect(page.locator('html')).toHaveAttribute('lang', other);
            await expect(page.locator('.primary-nav a[href^="/graph"]'))
                .toHaveAttribute('href', `/graph?lang=${other}`);
            await page.locator('.primary-nav a[href^="/graph"]').click();
            await expect(page.locator('#upcoming-title')).toHaveText(other === 'sk' ? 'Grafická kalkulačka' : 'Graphing calculator');
            await page.locator('.primary-nav a[href^="/solve"]').click();
            await expect(page.locator('#upcoming-title')).toHaveText(other === 'sk' ? 'Riešenie rovníc' : 'Equation solver');
            await page.locator('.primary-nav a[href^="/units"]').click();
            await expect(page.locator('#unit-converter')).toBeVisible();
            await expect(page.locator('#unit-result')).toHaveText('1000');
            await page.locator('.primary-nav a[href^="/?lang="]').click();
            await expect(page.locator('#history-list .history-index')).toHaveText(['#1']);
            await expect(page.locator('#expression')).toHaveValue('ans+1');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#history-list li')).toHaveCount(2);
            await expect(page.locator('#history-list .history-index')).toHaveText(['#1', '#2']);
            await calculate(page, 'ans', '4/3');
        });
        test('future tools have localized pages and the menu remains usable on mobile', async ({ page, request }) => {
            for (const tool of ['graph', 'solve', 'login', 'register']) {
                const response = await request.get(`/${tool}?lang=${lang}`);
                expect(response.ok()).toBe(true);
                await page.goto(`/${tool}?lang=${lang}`);
                await expect(page.locator(`[data-nav="${tool}"]`)).toHaveAttribute('aria-current', 'page');
                await expect(page.locator('#upcoming-title')).not.toBeEmpty();
                const other = lang === 'sk' ? 'en' : 'sk';
                await expect(page.locator(`.language-switch a[lang="${other}"]`))
                    .toHaveAttribute('href', `/${tool}?lang=${other}`);
            }
            expect((await request.get(`/account?lang=${lang}`)).status()).toBe(404);
            expect((await request.get(`/premium?lang=${lang}`)).status()).toBe(404);
            await expect(page.locator('a[href^="/premium"]')).toHaveCount(0);
            await page.setViewportSize({width: 1024, height: 768});
            await expect(page.locator('.nav-toggle')).toBeHidden();
            expect(await page.evaluate(() => {
                const menu = document.querySelector('.primary-nav');
                const links = [...menu.querySelectorAll('a')];
                const firstLine = links[0].getBoundingClientRect().top;
                return links.every(link => Math.abs(link.getBoundingClientRect().top - firstLine) < 1) &&
                    menu.scrollWidth <= menu.clientWidth + 1 &&
                    document.documentElement.scrollWidth <= innerWidth + 1;
            })).toBe(true);
            await page.setViewportSize({width: 375, height: 667});
            expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1)).toBe(true);
            await expect(page.locator('.nav-toggle')).toHaveAttribute('aria-expanded', 'false');
            await page.locator('.nav-toggle').click();
            await expect(page.locator('.nav-toggle')).toHaveAttribute('aria-expanded', 'true');
            await expect(page.locator('[data-nav="units"]')).toBeVisible();
            await expect(page.locator('[data-nav="login"]')).toBeVisible();
            await expect(page.locator('[data-nav="register"]')).toBeVisible();
            await page.keyboard.press('Escape');
            await expect(page.locator('.nav-toggle')).toHaveAttribute('aria-expanded', 'false');
            await page.locator('.nav-toggle').click();
            await page.locator('[data-nav="graph"]').click();
            await expect(page.locator('#upcoming-title')).toHaveText(lang === 'sk' ? 'Grafická kalkulačka' : 'Graphing calculator');
            await page.setViewportSize({width: 320, height: 667});
            expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1)).toBe(true);
            await page.locator('.nav-toggle').click();
            await expect(page.locator('[data-nav="register"]')).toBeVisible();
        });
        test('header, footer and license match the guide', async ({ page }) => {
            const calculatorHeader = await page.locator('.page-header').boundingBox();
            const calculatorFooter = await page.locator('.page-footer').boundingBox();
            await expect(page.locator('.page-footer a[href="https://github.com/Interacti0n/NumForge"]')).toHaveAttribute('target', '_blank');
            await page.locator('[data-license]').click();
            await expect(page.locator('.license-dialog')).toBeVisible();
            await expect(page.locator('.license-dialog pre')).toContainText('Copyright (c) 2026 Interacti0n');
            await page.locator('.license-heading button').click();
            await page.locator('.guide-link').click();
            const guideHeader = await page.locator('.page-header').boundingBox();
            const guideFooter = await page.locator('.page-footer').boundingBox();
            for (const key of ['x', 'width', 'height'])
            {
                expect(Math.abs(calculatorHeader[key] - guideHeader[key])).toBeLessThan(2);
                expect(Math.abs(calculatorFooter[key] - guideFooter[key])).toBeLessThan(2);
            }
            await page.locator('[data-license]').click();
            await expect(page.locator('.license-dialog')).toBeVisible();
            await expect(page.locator('.license-dialog pre')).toContainText('Permission is hereby granted');
            await expect(page.locator('.page-footer a[href="https://github.com/Interacti0n/NumForge"]')).toHaveAttribute('target', '_blank');
            await page.locator('.license-heading button').click();
            await page.setViewportSize({width: 375, height: 667});
            const mobileGuideHeader = await page.locator('.page-header').boundingBox();
            const mobileGuideFooter = await page.locator('.page-footer').boundingBox();
            await page.locator('.nav-toggle').click();
            await page.locator('.back-link').click();
            const mobileCalculatorHeader = await page.locator('.page-header').boundingBox();
            const mobileCalculatorFooter = await page.locator('.page-footer').boundingBox();
            for (const key of ['x', 'width', 'height'])
            {
                expect(Math.abs(mobileCalculatorHeader[key] - mobileGuideHeader[key])).toBeLessThan(2);
                expect(Math.abs(mobileCalculatorFooter[key] - mobileGuideFooter[key])).toBeLessThan(2);
            }
        });
        test('responsive layout scrolls naturally without shrinking controls', async ({ page }) => {
            for (const [width, height] of [[1280, 720], [1024, 600], [375, 667], [320, 568], [812, 375]]) {
                await page.setViewportSize({width, height});
                for (let index = 0; index < 6; index++) {
                    await page.locator(`#function-tab-${index}`).click();
                    await expect.poll(() => page.evaluate(() =>
                        document.documentElement.scrollWidth <= window.innerWidth + 1)).toBe(true);
                    expect(await page.locator('.calculator-shell').evaluate(el =>
                        getComputedStyle(el).zoom)).toBe('1');
                }
            }
            for (const [width, height] of [[1024, 768], [1280, 720], [1440, 900]]) {
                await page.setViewportSize({width, height});
                expect(await page.evaluate(() => document.documentElement.scrollHeight)).toBe(height);
                expect(await page.locator('.calculator-column').evaluate(el => el.scrollHeight <= el.clientHeight + 1)).toBe(true);
            }
            await page.setViewportSize({width: 1024, height: 768});
            await page.locator('#precision-mode').selectOption('custom');
            await expect(page.locator('#custom-precision')).toBeVisible();
            expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1)).toBe(true);
            expect(await page.locator('.calculator-column').evaluate(el => el.scrollHeight <= el.clientHeight + 1)).toBe(true);
            await page.locator('#precision-mode').selectOption('auto');
            await page.setViewportSize({width: 375, height: 667});
            expect(await page.locator('.number-keypad button').first().evaluate(el =>
                el.getBoundingClientRect().height)).toBeGreaterThanOrEqual(45);
            await page.locator('#precision-mode').selectOption('full');
            await page.locator('#expression').fill('2^2000');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#expand-result')).toBeVisible();
            await expect(page.locator('#result-more-marker')).toBeVisible();
            await expect.poll(() => page.evaluate(() =>
                document.documentElement.scrollHeight > window.innerHeight)).toBe(true);
            await page.locator('#expand-result').click();
            await expect(page.locator('#result')).toHaveClass(/expanded/);
            await expect(page.locator('#result-more-marker')).toBeHidden();
            await page.locator('#expand-result').click();
            await expect(page.locator('#result')).not.toHaveClass(/expanded/);
            await expect(page.locator('#result-more-marker')).toBeVisible();
        });
        test('function search and category controls are easy to reach', async ({ page }) => {
            await expect(page.locator('#example-options')).toHaveCount(0);
            const categoryLabels = lang === 'sk' ? ['Mocniny a logy', 'Goniometria']
                : ['Powers & logs', 'Trigonometry'];
            await expect(page.locator('#function-tab-3')).toHaveText(categoryLabels[0]);
            await expect(page.locator('#function-tab-4')).toHaveText(categoryLabels[1]);
            await page.setViewportSize({width: 1024, height: 768});
            const categoryFits = await page.locator('.function-tabs button').evaluateAll(buttons =>
                buttons.every(button => button.scrollWidth <= button.clientWidth + 1 &&
                    button.scrollHeight <= button.clientHeight + 1));
            expect(categoryFits).toBe(true);
            await page.locator('#function-search').fill('sqrt');
            await expect(page.locator('[data-function="sqrt"]')).toBeVisible();
            await expect(page.locator('[data-function="sin"]')).toBeHidden();
            await page.locator('[data-function="sqrt"]').click();
            await expect(page.locator('#expression')).toHaveValue('sqrt()');
            await page.locator('#function-search').fill(lang === 'sk' ? 'odmocnina' : 'square root');
            await expect(page.locator('[data-function="sqrt"]')).toBeVisible();
            await page.locator('#function-search').fill('unlikely-function-name');
            await expect(page.locator('#search-status')).toContainText(lang === 'sk' ? 'Žiadna' : 'No matching');
            await page.locator('#function-search').press('Escape');
            await expect(page.locator('#function-tab-0')).toBeVisible();
            for (const prefix of ['arc', 'arcus']) {
                await page.locator('#function-search').fill(prefix);
                for (const name of ['asin', 'acos', 'atan', 'asinh', 'acosh', 'atanh'])
                    await expect(page.locator(`[data-function="${name}"]`)).toBeVisible();
            }
            await page.locator('#function-search').fill('arcuscosh');
            await expect(page.locator('[data-function="acosh"]')).toBeVisible();
            await expect(page.locator('[data-function="asin"]')).toBeHidden();
            await page.locator('#function-search').press('Escape');
            if (lang === 'sk') {
                await page.locator('#function-search').fill('sucet');
                await expect(page.locator('[data-function="sum"]')).toBeVisible();
                await page.locator('#function-search').fill('nahodna');
                await expect(page.locator('[data-function="rand"]')).toBeVisible();
                await page.locator('#function-search').press('Escape');
            }
            const first = await page.locator('#function-tab-0').boundingBox();
            const second = await page.locator('#function-tab-1').boundingBox();
            expect(second.y).toBeGreaterThan(first.y);
            expect(Math.abs(second.x - first.x)).toBeLessThan(2);
            const search = await page.locator('#function-search').boundingBox();
            const categories = await page.locator('.function-tabs').boundingBox();
            const functions = await page.locator('.function-group:not([hidden])').boundingBox();
            expect(categories.y).toBeGreaterThan(search.y + search.height + 10);
            expect(functions.x).toBeGreaterThan(categories.x + categories.width);
            expect(await page.locator('.keypad.functions button:visible').first().evaluate(el =>
                parseFloat(getComputedStyle(el).fontSize))).toBeGreaterThanOrEqual(14);
            const helpPosition = await page.locator('#function-help').evaluate(el => ({
                top: el.getBoundingClientRect().top,
                bottom: el.getBoundingClientRect().bottom,
                groupBottom: document.querySelector('.function-groups').getBoundingClientRect().bottom,
                cardBottom: document.querySelector('.function-library').getBoundingClientRect().bottom
            }));
            expect(helpPosition.top).toBeGreaterThanOrEqual(helpPosition.groupBottom - 1);
            expect(helpPosition.bottom).toBeLessThanOrEqual(helpPosition.cardBottom + 1);
            const session = await page.locator('.session-panel').boundingBox();
            const sidebar = await page.locator('.sidebar').boundingBox();
            expect(Math.abs(session.y + session.height - (sidebar.y + sidebar.height))).toBeLessThan(2);
        });
        test('recent tools remain available below the keypad', async ({ page }) => {
            await page.locator('.constants [data-insert="π"]').click();
            await expect(page.locator('#recent-buttons button')).toHaveCount(1);
            await expect(page.locator('#recent-buttons button').first()).toHaveText('π');
            await page.locator('.operations [data-insert="^"]').click();
            await expect(page.locator('#recent-buttons button').first()).toHaveText('xʸ');
            await page.locator('#function-tab-3').click();
            await page.locator('[data-function="sqrt"]').click();
            await expect(page.locator('#recent-buttons button').first()).toHaveText('sqrt');
            await page.locator('[data-action="clear"]').click();
            await page.locator('#recent-buttons button').first().click();
            await expect(page.locator('#expression')).toHaveValue('sqrt()');
            for (const value of ['e', 'φ']) await page.locator(`.constants [data-insert="${value}"]`).click();
            for (const value of ['²', '³']) await page.locator(`.operations [data-insert="${value}"]`).click();
            await expect(page.locator('#recent-buttons button')).toHaveCount(7);
            await page.locator('.constants [data-insert="π"]').click();
            await expect(page.locator('#recent-buttons button').first()).toHaveText('π');
            await expect(page.locator('#recent-buttons button')).toHaveCount(7);
            await expect(page.locator('#recent-buttons button:visible')).toHaveCount(7);
            expect(await page.locator('.calculator-column').evaluate(el => el.scrollHeight <= el.clientHeight + 1)).toBe(true);
            const desktopRows = await page.locator('#recent-buttons button:visible').evaluateAll(buttons =>
                buttons.map(button => button.getBoundingClientRect().y));
            expect(Math.max(...desktopRows) - Math.min(...desktopRows)).toBeLessThan(2);
            const desktopFit = await page.locator('#recent-buttons').evaluate(row => ({
                right: row.getBoundingClientRect().right,
                last: [...row.children].filter(button => !button.hidden).at(-1).getBoundingClientRect().right
            }));
            expect(desktopFit.right - desktopFit.last).toBeLessThan(2);
            await page.setViewportSize({width: 375, height: 667});
            await expect.poll(() => page.locator('#recent-buttons button:visible').count()).toBeLessThan(7);
            expect(await page.locator('#recent-buttons button:visible').count()).toBeGreaterThan(0);
            const mobileRows = await page.locator('#recent-buttons button:visible').evaluateAll(buttons =>
                buttons.map(button => button.getBoundingClientRect().y));
            expect(Math.max(...mobileRows) - Math.min(...mobileRows)).toBeLessThan(2);
            const mobileFit = await page.locator('#recent-buttons').evaluate(row => ({
                right: row.getBoundingClientRect().right,
                last: [...row.children].filter(button => !button.hidden).at(-1).getBoundingClientRect().right
            }));
            expect(mobileFit.last).toBeLessThanOrEqual(mobileFit.right + 1);
            expect(mobileFit.right - mobileFit.last).toBeLessThan(2);
            expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1)).toBe(true);
        });
        test('function library gives unused sidebar height to the session', async ({ page }) => {
            await page.setViewportSize({width: 1280, height: 720});
            const before = {
                library: await page.locator('.function-library').boundingBox(),
                session: await page.locator('.session-panel').boundingBox()
            };
            await page.locator('#function-search').fill('no-such-function');
            const after = {
                library: await page.locator('.function-library').boundingBox(),
                session: await page.locator('.session-panel').boundingBox()
            };
            expect(after.library.height).toBeLessThan(before.library.height);
            expect(after.session.height).toBeGreaterThan(before.session.height);
            expect(Math.abs(after.session.y + after.session.height -
                (before.session.y + before.session.height))).toBeLessThan(2);
        });
        test('guide sections stay navigable on narrow screens', async ({ page }) => {
            await page.locator('.guide-link').click();
            await page.setViewportSize({width: 375, height: 812});
            const headerY = (await page.locator('.page-header').boundingBox()).y;
            const menuY = (await page.locator('.guide-toc').boundingBox()).y;
            await page.locator('.guide-toc a[href="#http"]').click();
            await expect(page).toHaveURL(/#http$/);
            expect((await page.locator('.page-header').boundingBox()).y).toBe(headerY);
            expect((await page.locator('.guide-toc').boundingBox()).y).toBe(menuY);
            expect(await page.locator('.guide-content').evaluate(el => el.scrollTop)).toBeGreaterThan(0);
            await expect(page.locator('#http')).toBeVisible();
            await expect.poll(() => page.evaluate(() =>
                document.documentElement.scrollWidth <= window.innerWidth + 1)).toBe(true);
        });
        test('guide navigation remains visible while reading on desktop', async ({ page }) => {
            await page.setViewportSize({width: 1280, height: 720});
            await page.locator('.guide-link').click();
            await expect(page.locator('.page-footer')).toContainText('v2.0.0');
            await expect(page.locator('.page-footer a[href$="/LICENSE"]')).toBeVisible();
            const headerY = (await page.locator('.page-header').boundingBox()).y;
            const menuY = (await page.locator('.guide-toc').boundingBox()).y;
            await page.locator('.guide-toc a[href="#http"]').click();
            await expect(page).toHaveURL(/#http$/);
            expect((await page.locator('.page-header').boundingBox()).y).toBe(headerY);
            expect((await page.locator('.guide-toc').boundingBox()).y).toBe(menuY);
            expect(await page.locator('.guide-content').evaluate(el => el.scrollTop)).toBeGreaterThan(0);
            await page.locator('.guide-toc a[href="#library"]').click();
            await expect(page).toHaveURL(/#library$/);
            await expect(page.locator('.guide-toc a[href="#syntax"]')).toBeVisible();
        });
        test('cache reformats values, recomputes precision and isolates pages', async ({ page, context }) => {
            async function answer(expression, scale) {
                const response = page.waitForResponse(r => r.url().includes('&action=commit'));
                await page.evaluate(({expression, scale}) => {
                    document.querySelector('#expression').value = expression;
                    document.querySelector('#precision-mode').value = 'custom';
                    document.querySelector('#precision').value = String(scale);
                    document.querySelector('#calculator').requestSubmit();
                }, {expression, scale});
                return (await response).json();
            }
            expect((await answer('100!', 10)).cached).toBe(false);
            expect((await answer('100!', 80)).cached).toBe(true);
            expect((await answer('1/3', 10)).cached).toBe(false);
            expect((await answer('1/3', 20)).cached).toBe(true);
            expect((await answer('1/3', 80)).cached).toBe(true);
            expect((await answer('1/3', 10)).cached).toBe(true);
            const other = await context.newPage();
            await other.goto(`/?lang=${lang}`);
            const response = other.waitForResponse(r => r.url().includes('&action=commit'));
            await other.locator('#expression').fill('1/3');
            await other.locator('#expression').press('Enter');
            expect((await (await response).json()).cached).toBe(false);
            await other.close();
        });
        test('angle selector placement, contrast and language state', async ({ page }) => {
            const settings = await page.locator('.precision-controls').boundingBox();
            const selector = await page.locator('.angle-switch').boundingBox();
            expect(selector.x).toBeGreaterThanOrEqual(settings.x);
            expect(selector.x + selector.width).toBeLessThanOrEqual(settings.x + settings.width + 1);
            expect(selector.y).toBeGreaterThanOrEqual(settings.y);
            expect(selector.y + selector.height).toBeLessThanOrEqual(settings.y + settings.height + 1);
            await expect(page.locator('#angle-indicator')).toHaveCount(0);
            await page.locator('[data-angle=deg]').click();
            const colors = await page.locator('[data-angle]').evaluateAll(buttons =>
                buttons.map(button => getComputedStyle(button).backgroundColor));
            expect(colors[0]).not.toBe(colors[1]);
            await expect(page.locator('[data-angle=deg]')).toHaveAttribute('aria-pressed', 'true');
            await page.locator('#expression').fill('π+sqrt(2)');
            if (await page.locator('#precision-mode').inputValue() !== 'custom')
                await page.locator('#precision-mode').selectOption('custom');
            await page.locator('#precision').fill('20');
            const other = lang === 'sk' ? 'en' : 'sk';
            await page.locator(`.language-switch a[lang=${other}]`).click();
            await expect(page.locator('html')).toHaveAttribute('lang', other);
            await expect(page.locator('#expression')).toHaveValue('π+sqrt(2)');
            await expect(page.locator('#precision')).toHaveValue('20');
            await expect(page.locator('[data-angle=deg]')).toHaveAttribute('aria-pressed', 'true');
            await page.locator('#precision-mode').selectOption('full');
            await page.locator(`.language-switch a[lang=${lang}]`).click();
            await expect(page.locator('#expression')).toHaveValue('π+sqrt(2)');
            await expect(page.locator('#precision-mode')).toHaveValue('full');
            await expect(page.locator('#precision')).toBeDisabled();
            await expect(page.locator('body')).not.toContainText('C parser and exact BigDecimal');
            await expect(page.locator('body')).not.toContainText('cez C parser a presný BigDecimal');
        });
        test('function help, domains and shared angle selector', async ({ page }) => {
            await expect(page.locator('[data-angle=rad]')).toHaveAttribute('aria-pressed', 'true');
            await page.locator('[data-function=rand]').focus();
            await expect(page.locator('#function-help')).toContainText('rand(x;y)');
            await page.locator('#function-tab-4').click();
            await page.locator('[data-angle=deg]').press('Enter');
            await expect(page.locator('[data-angle=deg]')).toHaveAttribute('aria-pressed', 'true');
            await expect(page.locator('[data-angle=deg]')).toHaveAttribute('aria-pressed', 'true');
            await page.reload();
            await expect(page.locator('[data-angle=deg]')).toHaveAttribute('aria-pressed', 'true');
            await page.locator('#function-tab-4').click();
            await page.locator('[data-function=asin]').focus();
            await expect(page.locator('#function-help')).toContainText('-1 ≤ x ≤ 1');
            await page.locator('#expression').fill('asin(2)');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toContainText('-1 ≤ x ≤ 1');
            await expect(page.locator('#result')).toContainText('⟦asin⟧');
            await page.locator('#function-tab-3').click();
            await page.locator('[data-action=clear]').click();
            await page.locator('[data-function=log]').dispatchEvent('click');
            await expect(page.locator('#function-help')).toContainText('y ≠ 1');
            await expect(page.locator('#expression')).toHaveValue('log()');
            await page.locator('[data-function=sqrt]').hover();
            await expect(page.locator('#function-help')).toContainText('x ≥ 0');
            await expect(page.locator('[data-function=sqrt]')).toHaveText('√x');
            await page.locator('#expression').fill('log(2;1)');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toContainText('y ≠ 1');
        });
        test('non-JSON failures recover without stale results', async ({ page }) => {
            await page.route('**/api/evaluate*', route => route.fulfill({ status: 503,
                contentType: 'text/plain', body: 'Unavailable' }));
            await page.locator('#expression').fill('2+2');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toContainText(lang === 'sk' ? 'Chyba' : 'Error');
            await expect(page.locator('#result')).toContainText(lang === 'sk' ? 'Neočakávaná odpoveď servera' : 'Unexpected server response');
            await expect(page.locator('#copy-result')).toBeDisabled();
            await page.unroute('**/api/evaluate*');
            await calculate(page, '2+2', '4');
        });
        test('Clear cancels an older response and network failure recovers', async ({ page }) => {
            let release, arrived, finished;
            const held = new Promise(resolve => { release = resolve; });
            const seen = new Promise(resolve => { arrived = resolve; });
            const done = new Promise(resolve => { finished = resolve; });
            await page.route('**/api/evaluate*', async route => {
                if (route.request().postData() !== '1+1') return route.continue();
                const response = await route.fetch();
                arrived();
                await held;
                try { await route.fulfill({ response }); } catch { /* client deliberately aborted */ }
                finished();
            });
            await page.locator('#expression').fill('1+1');
            await seen;
            await page.locator('[data-action=clear]').click();
            await expect(page.locator('#copy-result')).toBeDisabled();
            await calculate(page, '2+2', '4');
            release();
            await done;
            await expect(page.locator('#result')).toHaveText('4');
            await page.unroute('**/api/evaluate*');
            await page.route('**/api/evaluate*', route => route.abort());
            await page.locator('#expression').fill('3+3');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toContainText(lang === 'sk' ? 'Chyba' : 'Error');
            await page.unroute('**/api/evaluate*');
            await calculate(page, '3+3', '6');
        });
    });
}


test('session variables: preview, exact values, replay and client isolation', async ({request}) => {
    async function send(client, revision, action, input='') {
        const response=await request.post('/api/evaluate?precision=full&angle=rad&client='+client+'&revision='+revision+'&action='+action,{data:input});
        return response.json();
    }
    const client='e'.repeat(32),other='f'.repeat(32);
    expect((await send(client,1,'start')).ok).toBe(true);
    expect((await send(client,1,'preview','x=2/3')).result).toBe('2/3');
    expect((await send(client,2,'preview','x')).status).toBe('variable is undefined');
    expect((await send(client,3,'commit','x=2/3')).result).toBe('2/3');
    expect((await send(client,4,'commit','x=x+1')).result).toBe('5/3');
    expect((await send(client,4,'commit','x=x+1')).result).toBe('5/3');
    expect((await send(client,5,'preview','x*3')).result).toBe('5');
    expect((await send(client,6,'commit','x=1/0')).ok).toBe(false);
    expect((await send(client,7,'preview','x*3')).result).toBe('5');
    expect((await send(other,1,'start')).ok).toBe(true);
    expect((await send(other,1,'preview','x')).status).toBe('variable is undefined');
});

test('variable assignments can be typed and confirmed in both languages', async ({page}) => {
    for(const language of ['en','sk']) {
        await page.goto('/?lang='+language);
        await page.locator('#expression').fill('x=2/3');
        await page.locator('#expression').press('Enter');
        await expect(page.locator('#result')).toHaveText('2/3');
        await page.locator('#expression').fill('x*3');
        await page.locator('#expression').press('Enter');
        await expect(page.locator('#result')).toHaveText('2');
        await page.locator('#reset-session').click();
        await page.locator('#expression').fill('x');
        await page.locator('#expression').press('Enter');
        await expect(page.locator('#result')).toContainText(language==='en'?'variable is undefined':'premenná nie je definovaná');
    }
});
