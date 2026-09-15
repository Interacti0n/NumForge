const { test, expect } = require('@playwright/test');

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
            await page.locator('#precision').fill('2');
            await calculate(page, '1/8', '0.12');
            await page.locator('#full-precision').check();
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
            await page.locator('#precision').fill('2');
            await expect(page.locator('#result')).toHaveText('0.12');
            await page.locator('#full-precision').check();
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
        test('function groups, aliases and pending calls', async ({ page }, testInfo) => {
            await expect(page.locator('details.function-group')).toHaveCount(4);
            await expect(page.locator('[data-function]')).toHaveCount(24);
            await expect(page.locator('[data-function]:disabled')).toHaveCount(14);
            const powers = page.locator('details').filter({ has: page.locator('[data-function="pow"]') });
            await powers.locator('summary').focus();
            await page.keyboard.press('Enter');
            await page.locator('[data-function="pow"]').click();
            await expect(page.locator('#expression')).toHaveValue('pow(');
            await page.locator('[data-insert="2"]').click();
            await page.locator('[data-insert=";"]').click();
            await page.locator('[data-insert="3"]').click();
            await page.locator('[data-insert=")"]').click();
            await page.locator('[data-action=evaluate]').click();
            await expect(page.locator('#result')).toHaveText('8');
            await calculate(page, 'factorial(5)', '120');
            await page.locator('#expression').fill('exp(1)');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toContainText(lang === 'sk' ? 'funkcia nie je implementovaná' : 'not implemented');
            await page.locator('#expression').fill('atan(1;2)');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toContainText(lang === 'sk' ? 'nesprávny počet argumentov' : 'wrong number of arguments');
            await calculate(page, '1e3-1*e*3', '0');
            await page.screenshot({ path: testInfo.outputPath('functions-desktop.png'), fullPage: true });
            await page.setViewportSize({ width: 375, height: 812 });
            for (const group of await page.locator('details.function-group').all()) {
                if (await group.getAttribute('open') === null) await group.locator('summary').click();
            }
            await expect.poll(() => page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(true);
            await page.screenshot({ path: testInfo.outputPath('functions-mobile.png'), fullPage: true });
            await page.locator('.guide-link').click();
            await expect(page.locator('body')).toContainText('log(x;b)');
        });
        test('integer buttons execute through C and reject invalid domains', async ({ page }) => {
            const group = page.locator('details').filter({has: page.locator('[data-function="gcd"]')});
            await group.locator('summary').click();
            for (const [name, args, expected] of [
                ['gcd', '-48;18)', '6'], ['lcm', '-4;6)', '12'],
                ['mod', '-7;3)', '-1'], ['isqrt', '18446744073709551616)', '4294967296']
            ]) {
                await page.locator('[data-action=clear]').click();
                await page.locator(`[data-function="${name}"]`).click();
                await expect(page.locator('#expression')).toHaveValue(`${name}(`);
                await page.locator('#expression').press('End');
                await page.locator('#expression').pressSequentially(args);
                await page.locator('#expression').press('Enter');
                await expect(page.locator('#result')).toHaveText(expected);
            }
            await calculate(page, 'min(abs(-3);max(1;sign(-2)))', '1');
            await page.locator('#expression').fill('isqrt(-1)');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toContainText(lang === 'sk' ? 'neplatný argument' : 'invalid argument');
        });
        test('result keeps five lines and expansion resets', async ({ page }) => {
            const result = page.locator('#result');
            const lineHeight = await result.evaluate(el => parseFloat(getComputedStyle(el).lineHeight));
            expect((await result.boundingBox()).height).toBeCloseTo(5 * lineHeight, 0);
            await page.locator('#full-precision').check();
            await calculate(page, '2^2000', (2n ** 2000n).toString()[0] + '.' + (2n ** 2000n).toString().slice(1) + 'E+602');
            await expect(page.locator('#expand-result')).toBeVisible();
            await page.locator('#expand-result').click();
            expect((await result.boundingBox()).height).toBeGreaterThan(5 * lineHeight);
            await calculate(page, '2^2000', (2n ** 2000n).toString()[0] + '.' + (2n ** 2000n).toString().slice(1) + 'E+602');
            await expect(page.locator('#expand-result')).toHaveText(lang === 'sk' ? 'Zobraziť všetko' : 'Show all');
        });
        test('non-JSON failures recover without stale results', async ({ page }) => {
            await page.route('**/api/evaluate*', route => route.fulfill({ status: 503,
                contentType: 'text/plain', body: 'Unavailable' }));
            await page.locator('#expression').fill('2+2');
            await page.locator('#expression').press('Enter');
            await expect(page.locator('#result')).toContainText(lang === 'sk' ? 'Chyba' : 'Error');
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
