const {test, expect} = require('@playwright/test');

for (const lang of ['sk', 'en']) {
    for (const units of [false, true]) {
        test(`${lang} ${units ? 'units' : 'calculator'} waits for idle editing and confirms immediately`, async ({page}) => {
            await page.emulateMedia({reducedMotion: 'reduce'});
            await page.goto((units ? '/units' : '/') + '?lang=' + lang);
            const input = page.locator(units ? '#unit-input' : '#expression');
            const result = page.locator(units ? '#unit-result' : '#result');
            if (units) await expect(result).toHaveText('1000');
            const requests = [];
            page.on('request', request => {
                const url = new URL(request.url());
                if (url.pathname === (units ? '/api/convert' : '/api/evaluate') ||
                    (units && url.pathname === '/api/conversions' && url.searchParams.get('action') === 'commit')) {
                    requests.push(request);
                }
            });
            await page.clock.install();
            await page.clock.pauseAt(new Date());
            await input.fill('2');
            await page.clock.runFor(1500);
            expect(requests).toHaveLength(0);
            await input.fill('2+3');
            await page.clock.runFor(1999);
            expect(requests).toHaveLength(0);
            await page.clock.runFor(1);
            await expect(result).toHaveText(units ? '5000' : '5');
            expect(requests).toHaveLength(1);
            expect(requests[0].postData()).toBe('2+3');

            await page.locator(units ? '#unit-notation' : '#notation-mode').dispatchEvent('change');
            await page.clock.runFor(1999);
            expect(requests).toHaveLength(1);
            await input.fill('4+5');
            await input.press('Enter');
            await expect(result).toHaveText(units ? '9000' : '9');
            expect(requests).toHaveLength(2);
            expect(new URL(requests[1].url()).searchParams.get('action')).toBe('commit');
            await page.clock.runFor(2500);
            expect(requests).toHaveLength(2);

            await input.fill('10');
            await input.fill('');
            await page.clock.runFor(2500);
            expect(requests).toHaveLength(2);
            await expect(result).toBeEmpty();

            await input.fill('20');
            await page.locator('.header-guide').click();
            await expect(page).toHaveURL(units ? `/api?section=units&lang=${lang}` : `/api?lang=${lang}`);
            await expect(page.locator('.guide-content')).toBeVisible();
            await page.clock.runFor(2500);
            expect(requests).toHaveLength(2);
        });
    }
}
