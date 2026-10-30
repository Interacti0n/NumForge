const {chromium, expect} = require('../browser/node_modules/@playwright/test');
const fs = require('node:fs');
const path = require('node:path');
(async () => {
    const baseURL = process.env.NF_UI_BASE_URL;
    if (!baseURL?.startsWith('https://localhost:')) throw Error('Disposable local HTTPS gateway required');
    const browser = await chromium.launch();
    try {
        // Python's gateway check already verified the actual CA chain.
        const context = await browser.newContext({baseURL, ignoreHTTPSErrors:true, viewport:{width:1366,height:900}});
        const page = await context.newPage();
        for (const width of [1280,1024,981]) {
            await page.setViewportSize({width,height:800});
            const boxes = [];
            for (const language of ['sk','en']) {
                await page.goto('/?lang='+language);
                await page.evaluate(() => document.fonts.ready);
                boxes.push(await page.locator('.account-link').boundingBox());
                expect(boxes.at(-1).y).toBeGreaterThanOrEqual(0);
                expect(boxes.at(-1).y+boxes.at(-1).height).toBeLessThanOrEqual((await page.locator('.page-header').boundingBox()).height+1);
                expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
            }
            expect(Math.abs(boxes[0].width-boxes[1].width)).toBeLessThan(1);
            expect(Math.abs(boxes[0].x-boxes[1].x)).toBeLessThan(1);
        }
        await page.setViewportSize({width:1366,height:900});
        await page.goto('/login?lang=sk');
        await expect(page.locator('#account-form')).toBeVisible();
        await page.getByLabel('Používateľské meno', {exact:true}).fill('container-check');
        await page.getByLabel('Heslo', {exact:true}).fill('wrong');
        await page.getByRole('button', {name:'Prihlásiť sa',exact:true}).click();
        await expect(page.locator('#account-status')).toContainText('nie je správne');
        await page.getByLabel('Heslo', {exact:true}).fill('test-only');
        await page.getByRole('button', {name:'Prihlásiť sa',exact:true}).click();
        await expect(page.locator('#account-name')).toHaveText('container-check');
        await page.reload();
        await expect(page.locator('#account-name')).toHaveText('container-check');
        const second = await context.newPage();
        await second.goto('/login?lang=en');
        await expect(second.locator('#account-name')).toHaveText('container-check');
        await page.getByRole('button', {name:'Odhlásiť sa',exact:true}).click();
        await expect(second.locator('#account-form')).toBeVisible();
        const output = path.resolve('build/browser-artifacts/accounts');
        fs.mkdirSync(output,{recursive:true});
        await page.screenshot({path:path.join(output,'desktop.png'),fullPage:true});
        await page.setViewportSize({width:390,height:844});
        await page.goto('/login?lang=en');
        await expect(page.getByRole('button',{name:'Sign in',exact:true})).toBeVisible();
        expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
        await page.screenshot({path:path.join(output,'mobile.png'),fullPage:true});
        await page.goto('/graph?lang=en');
        if (!await page.locator('.account-link').isVisible()) await page.locator('.nav-toggle').click();
        await expect(page.locator('.account-link')).toBeVisible();
        await page.locator('.account-link').click();
        await expect(page.locator('#account-form')).toBeVisible();
        console.log('Account UI: SK/EN, wrong password, login/reload, cross-tab logout, mobile width and tool navigation passed.');
        await context.close();
    } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode=1; });
