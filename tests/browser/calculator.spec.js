const { test, expect } = require('@playwright/test');

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


for (const lang of ['sk', 'en']) {
    test.describe(lang, () => {
        test.beforeEach(async ({ page }) => { await page.goto(`/?lang=${lang}`); });
        async function calculate(page, input, expected) {
            await page.locator('#expression').fill(input);
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toHaveText(expected);
        }
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
            await calculate(page, '1/8', '0.125');
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
            await page.locator(`a[href="/?lang=${other}"]`).click();
            await calculate(page, '2(2+2)', '8');
        });
        test('automatic calculation and precision above result', async ({ page }, testInfo) => {
            const settings = await page.locator('.precision').boundingBox();
            const panel = await page.locator('.result-panel').boundingBox();
            expect(settings.y + settings.height).toBeLessThan(panel.y);
            await page.locator('#expression').fill('1/8');
            await expect(page.locator('#result')).toHaveText('0.125');
            if (await page.locator('#precision-mode').inputValue() !== 'custom')
                await page.locator('#precision-mode').selectOption('custom');
            await page.locator('#precision').fill('2');
            await expect(page.locator('#result')).toHaveText('0.12');
            await page.locator('#precision-mode').selectOption('full');
            await expect(page.locator('#result')).toHaveText('0.125');
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
            await expect(page.locator('details.function-group')).toHaveCount(5);
            await expect(page.locator('[data-function]')).toHaveCount(39);
            await expect(page.locator('[data-function]:disabled')).toHaveCount(0);
            await page.locator('#function-tab-0').click();
            await page.locator('[data-function="round"]').click();
            await expect(page.locator('#expression')).toHaveValue('round()');
            await page.locator('#expression').fill('round(12.345;2)');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toHaveText('12.34');
            await calculate(page, 'mean(1;2;2)', '1.6666666667');
            await page.locator('[data-action="clear"]').click();
            await page.locator('#function-tab-3').click();
            await page.locator('[data-function="median"]').click();
            await expect(page.locator('#expression')).toHaveValue('median()');
            await calculate(page, 'harmean(1;2;4)', '1.7142857143');
            await page.locator('[data-action="clear"]').click();
            await page.locator('[data-function="variance"]').click();
            await expect(page.locator('#expression')).toHaveValue('variance()');
            await calculate(page, 'stdev(1;2;3)', '1');
            await page.locator('[data-action="clear"]').click();
            const powers = page.locator('details').filter({ has: page.locator('[data-function="pow"]') });
            await page.locator('#function-tab-2').focus();
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
            await page.locator('#function-tab-2').click();
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
            await calculate(page, 'degrees(π)', '180');
            await page.locator('#expression').fill('atan(1;2)');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toContainText(lang === 'sk' ? 'nesprávny počet argumentov' : 'wrong number of arguments');
            await calculate(page, '1e3-1*e*3', '0');
            await page.screenshot({ path: testInfo.outputPath('functions-desktop.png'), fullPage: true });
            await page.setViewportSize({ width: 375, height: 812 });
            await expect.poll(() => page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(true);
            await page.screenshot({ path: testInfo.outputPath('functions-mobile.png'), fullPage: true });
            await page.locator('.guide-link').click();
            await expect(page.locator('body')).toContainText('log(x;b)');
        });
        test('integer buttons execute through C and reject invalid domains', async ({ page }) => {
            const group = page.locator('details').filter({has: page.locator('[data-function="gcd"]')});
            await page.locator('#function-tab-1').click();
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
            await page.locator('#function-tab-2').click();
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
        test('result keeps five lines and expansion resets', async ({ page }) => {
            const result = page.locator('#result');
            const lineHeight = await result.evaluate(el => parseFloat(getComputedStyle(el).lineHeight) *
                Number(getComputedStyle(el.closest('.calculator-shell')).zoom));
            expect((await result.boundingBox()).height).toBeCloseTo(5 * lineHeight, 0);
            await page.locator('#precision-mode').selectOption('full');
            await calculate(page, '2^2000', (2n ** 2000n).toString()[0] + '.' + (2n ** 2000n).toString().slice(1) + 'E+602');
            await expect(page.locator('#expand-result')).toBeVisible();
            await page.locator('#expand-result').click();
            expect((await result.boundingBox()).height).toBeGreaterThan(5 * lineHeight);
            await calculate(page, '2^2000', (2n ** 2000n).toString()[0] + '.' + (2n ** 2000n).toString().slice(1) + 'E+602');
            await expect(page.locator('#expand-result')).toHaveText(lang === 'sk' ? 'Zobraziť všetko' : 'Show all');
        });
        test('collapsed calculator fits the viewport in every category', async ({ page }) => {
            for (const [width, height] of [[1280, 720], [1024, 600], [375, 667], [320, 568], [812, 375]]) {
                await page.setViewportSize({width, height});
                for (let index = 0; index < 4; index++) {
                    await page.locator(`#function-tab-${index}`).click();
                    await expect.poll(() => page.evaluate(() =>
                        document.documentElement.scrollHeight <= window.innerHeight + 1)).toBe(true);
                    await expect.poll(() => page.evaluate(() =>
                        document.documentElement.scrollWidth <= window.innerWidth + 1)).toBe(true);
                }
            }
            await page.setViewportSize({width: 375, height: 667});
            await page.locator('#precision-mode').selectOption('full');
            await page.locator('#expression').fill('2^2000');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#expand-result')).toBeVisible();
            await expect.poll(() => page.evaluate(() =>
                document.documentElement.scrollHeight <= window.innerHeight + 1)).toBe(true);
            await page.locator('#expand-result').click();
            await expect.poll(() => page.evaluate(() =>
                document.documentElement.scrollHeight > window.innerHeight)).toBe(true);
            await page.locator('#expand-result').click();
            await expect.poll(() => page.evaluate(() =>
                document.documentElement.scrollHeight <= window.innerHeight + 1)).toBe(true);
        });
        test('cache reformats values, recomputes precision and isolates pages', async ({ page, context }) => {
            async function answer(expression, scale) {
                await page.locator('#expression').fill(expression);
                const response = page.waitForResponse(r => r.url().includes('/api/evaluate'));
                if (await page.locator('#precision-mode').inputValue() !== 'custom')
                    await page.locator('#precision-mode').selectOption('custom');
                await page.locator('#precision').fill(String(scale));
                await page.locator('#expression').press('Enter');
                return (await response).json();
            }
            expect((await answer('100!', 10)).cached).toBe(false);
            expect((await answer('100!', 80)).cached).toBe(true);
            expect((await answer('1/3', 10)).cached).toBe(false);
            expect((await answer('1/3', 20)).cached).toBe(true);
            expect((await answer('1/3', 80)).cached).toBe(false);
            expect((await answer('1/3', 10)).cached).toBe(false);
            const other = await context.newPage();
            await other.goto(`/?lang=${lang}`);
            const response = other.waitForResponse(r => r.url().includes('/api/evaluate'));
            await other.locator('#expression').fill('1/3');
            await other.locator('#expression').press('Enter');
            expect((await (await response).json()).cached).toBe(false);
            await other.close();
        });
        test('angle selector placement, contrast and language state', async ({ page }) => {
            const settings = await page.locator('.precision-controls').boundingBox();
            const selector = await page.locator('.angle-switch').boundingBox();
            expect(selector.x).toBeGreaterThanOrEqual(settings.x + settings.width);
            expect(Math.abs(selector.y + selector.height / 2 - settings.y - settings.height / 2)).toBeLessThan(2);
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
            await page.locator('#function-tab-2').click();
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
            await page.locator('#expression').press('Enter');
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
