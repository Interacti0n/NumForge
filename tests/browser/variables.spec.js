const {test, expect} = require('@playwright/test');

for (const language of ['sk', 'en']) {
    test(`stored variable list: confirmation, capacity and navigation (${language})`, async ({page}) => {
        await page.setViewportSize({width: 1366, height: 768});
        await page.goto('/?lang=' + language);
        const input = page.locator('#expression');
        await page.locator('#session-variables-tab').click();
        const entries = page.locator('#variable-list li');
        async function assign(expression, result) {
            await input.fill(expression);
            await input.press('Enter');
            await expect(page.locator('#result')).toHaveText(result);
        }
        await input.fill('x=2/3');
        await expect(page.locator('#result')).toHaveText('2/3');
        await expect(entries).toHaveCount(0);
        await input.press('Enter');
        await expect(entries).toHaveCount(1);
        await expect(entries.first().locator('button').first()).toHaveText('x= 2/3');
        await assign('x=qty(5;"m")', '5 m');
        await expect(entries.first().locator('button').first()).toHaveText('x= 5 m');
        await input.fill('x=1/0');
        await input.press('Enter');
        await expect(page.locator('#result')).toHaveClass('error');
        await expect(entries.first().locator('button').first()).toHaveText('x= 5 m');
        for (let index = 0; index < 31; index++)
            await assign('v' + String.fromCharCode(65 + Math.floor(index / 26)) +
                String.fromCharCode(65 + index % 26) + '=' + index, String(index));
        await expect(entries).toHaveCount(32);
        await expect(page.locator('#variable-count')).toHaveText('32 / 32');
        await expect(page.locator('#history-list li')).toHaveCount(16);
        const dimensions = await page.locator('.sidebar').evaluate(list =>
            ({height: list.clientHeight, content: list.scrollHeight}));
        expect(dimensions.height).toBeGreaterThan(34);
        expect(dimensions.content).toBeGreaterThan(dimensions.height);
        await input.fill('');
        await page.locator('#variable-list button').filter({has: page.locator('.variable-name', {hasText: /^x$/})}).click();
        await expect(input).toHaveValue('x');
        await expect(page.locator('#result')).toHaveText('5 m');
        await page.locator('#session-history-tab').click();
        await page.locator('.header-guide').click();
        await page.locator('.primary-nav a[href="/?lang=' + language + '"]').click();
        await expect(entries).toHaveCount(32);
        await expect(page.locator('#session-history-tab')).toHaveAttribute('aria-selected', 'true');
        await page.locator('#session-variables-tab').click();
        await expect(entries.filter({hasText: 'x= 5 m'})).toHaveCount(1);
        await entries.filter({hasText: 'x= 5 m'}).locator('.variable-delete').click();
        await expect(entries).toHaveCount(31);
        await expect(page.locator('#variable-count')).toHaveText('31 / 32');
        await assign('fresh=2', '2');
        await expect(entries).toHaveCount(32);
        await input.fill('x');
        await expect(page.locator('#result')).toHaveClass('error');
        await page.setViewportSize({width: 375, height: 812});
        expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
        await page.locator('#reset-session').click();
        await page.locator('#session-variables-tab').click();
        await expect(entries).toHaveCount(0);
        await expect(page.locator('#variables-empty')).toBeVisible();
    });
}

test('HTTP delete-variable is isolated, idempotent and ordered with confirmations', async ({request}) => {
    const client = 'e'.repeat(32);
    async function send(revision, action, input = '') {
        return (await request.post('/api/evaluate?precision=10&angle=rad&client=' + client +
            '&revision=' + revision + '&action=' + action, {data: input})).json();
    }
    expect((await send(1, 'start')).ok).toBe(true);
    expect((await send(2, 'commit', 'x=qty(5;"m")')).result).toBe('5 m');
    expect((await send(3, 'delete-variable', 'x')).ok).toBe(true);
    expect((await send(3, 'delete-variable', 'x')).ok).toBe(true);
    expect((await send(2, 'commit', 'x=qty(5;"m")')).status).toBe('stale session request');
    expect((await send(4, 'preview', 'ans')).result).toBe('5 m');
    expect((await send(5, 'preview', 'x')).status).toBe('variable is undefined');
    expect((await send(6, 'delete-variable', 'x=2')).ok).toBe(false);
    expect((await send(7, 'delete-variable', 'missing')).ok).toBe(true);
    expect((await send(8, 'commit', 'x=3')).ok).toBe(true);
    expect((await send(3, 'delete-variable', 'x')).ok).toBe(false);
    expect((await send(9, 'preview', 'x')).result).toBe('3');
    const other = await request.post('/api/evaluate?precision=10&angle=rad&client=' + 'd'.repeat(32) +
        '&revision=1&action=delete-variable', {data: 'x'});
    expect((await other.json()).status).toContain('session expired');
});

test('uncertain deletion retries the same mutation before further calculations', async ({page}) => {
    await page.goto('/?lang=en');
    await page.locator('#expression').fill('x=7');
    await page.locator('#expression').press('Enter');
    await expect(page.locator('#variable-count')).toHaveText('1 / 32');
    await page.locator('#session-variables-tab').click();
    let revision;
    await page.route('**/*action=delete-variable', async route => {
        revision = new URL(route.request().url()).searchParams.get('revision');
        await route.fetch();
        await route.abort('failed');
    });
    await page.locator('.variable-delete').click();
    await expect(page.locator('#variable-status')).toContainText('Click delete again');
    await expect(page.locator('#variable-list li')).toHaveCount(1);
    await page.unroute('**/*action=delete-variable');
    const retry = page.waitForRequest(request => request.url().endsWith('action=delete-variable'));
    await page.locator('.variable-delete').click();
    expect(new URL((await retry).url()).searchParams.get('revision')).toBe(revision);
    await expect(page.locator('#variable-list li')).toHaveCount(0);
    await page.locator('#expression').fill('ans');
    await page.locator('#expression').press('Enter');
    await expect(page.locator('#result')).toHaveText('7');
});

test('tab keyboard controls and navigation preserve a completed deletion', async ({page}) => {
    await page.goto('/?lang=en');
    await page.locator('#expression').fill('x=7');
    await page.locator('#expression').press('Enter');
    await expect(page.locator('#variable-count')).toHaveText('1 / 32');
    await page.locator('#session-history-tab').focus();
    await page.keyboard.press('ArrowRight');
    await expect(page.locator('#session-variables-tab')).toBeFocused();
    await expect(page.locator('#session-variables-tab')).toHaveAttribute('aria-selected', 'true');
    let release;
    let deleted;
    const gate = new Promise(resolve => { release = resolve; });
    const ready = new Promise(resolve => { deleted = resolve; });
    await page.route('**/*action=delete-variable', async route => {
        const response = await route.fetch();
        deleted();
        await gate;
        await route.fulfill({response});
    });
    await page.locator('.variable-delete').click();
    await ready;
    await page.locator('.header-guide').click();
    release();
    await expect(page).toHaveURL(/\/api\?lang=en$/);
    await page.locator('.primary-nav a[href="/?lang=en"]').click();
    await expect(page.locator('#variable-count')).toHaveText('0 / 32');
    await expect(page.locator('#session-variables-tab')).toHaveAttribute('aria-selected', 'true');
    await expect(page.locator('#variables-empty')).toBeVisible();
});
