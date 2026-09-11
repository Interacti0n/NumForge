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
