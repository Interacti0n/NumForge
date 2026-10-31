const {chromium, expect} = require('../browser/node_modules/@playwright/test');
const fs = require('node:fs');
(async () => {
    const baseURL = process.env.NF_UI_BASE_URL;
    if (!baseURL?.startsWith('https://localhost:')) throw Error('Disposable local HTTPS gateway required');
    const browser = await chromium.launch();
    let unblock, closing = false;
    const contexts = [];
    const fresh = async () => {
        const context = await browser.newContext({baseURL,ignoreHTTPSErrors:true}); contexts.push(context);
        return context.newPage();
    };
    const login = async (page, user) => {
        await page.goto('/login?lang=en');
        await expect(page.locator('#account-form')).toBeVisible();
        await page.getByLabel('Username',{exact:true}).fill(user);
        await page.getByLabel('Password',{exact:true}).fill('test-only');
        await page.getByRole('button',{name:'Sign in',exact:true}).click();
        await expect(page.locator('#preferences-form')).toBeVisible();
    };
    const field = (page,name) => page.locator(`#preferences-form [name="${name}"]`);
    const save = async page => {
        await page.getByRole('button',{name:'Save settings',exact:true}).click();
        await expect(page.locator('#preferences-status')).toHaveText('Settings saved.');
    };
    try {
        const first = await fresh();
        await first.goto('/?lang=en');
        await first.evaluate(() => window.numforgePreferences.refresh());
        await first.locator('#precision-mode').selectOption('custom');
        await first.locator('#precision').fill('3');
        await first.locator('#notation-mode').selectOption('plain');
        await first.locator('[data-angle="deg"]').click();
        await login(first,'container-check');
        await expect(field(first,'precision_mode')).toHaveValue('auto');
        await first.getByRole('button',{name:'Use guest settings',exact:true}).click();
        await expect(field(first,'precision')).toHaveValue('3');
        expect(await first.evaluate(async () => (await (await fetch('/api/account/preferences')).json()).revision)).toBe(0);
        await save(first);
        await field(first,'precision').fill('7');
        await field(first,'notation').selectOption('math');
        await field(first,'language').selectOption('en');
        await save(first);
        const second = await fresh();
        await login(second,'container-check');
        await expect(field(second,'precision')).toHaveValue('7');
        fs.mkdirSync('build/browser-artifacts/preferences',{recursive:true});
        await second.screenshot({path:'build/browser-artifacts/preferences/desktop.png',fullPage:true});
        await second.setViewportSize({width:390,height:844});
        expect(await second.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
        await second.screenshot({path:'build/browser-artifacts/preferences/mobile.png',fullPage:true});
        await second.setViewportSize({width:1280,height:720});
        const evaluations = [];
        second.on('request', r => {if (new URL(r.url()).pathname === '/api/evaluate') evaluations.push(r.postData());});
        await second.goto('/');
        await expect(second).toHaveURL(/lang=en/);
        await expect(second.locator('#precision')).toHaveValue('7');
        await expect(second.locator('#notation-mode')).toHaveValue('math');
        expect(evaluations.filter(body => body?.trim())).toEqual([]);
        await second.goto('/login?lang=en');
        await expect(field(second,'precision')).toHaveValue('7');
        await field(first,'precision').fill('11');
        await field(second,'precision').fill('13'); await save(second);
        await first.getByRole('button',{name:'Save settings',exact:true}).click();
        await expect(first.locator('#preferences-status')).toContainText('saved version changed');
        await expect(field(first,'precision')).toHaveValue('11');
        await first.getByRole('button',{name:'Load saved settings',exact:true}).click();
        await expect(field(first,'precision')).toHaveValue('13');
        await first.route('**/api/account/preferences', route => route.request().method() === 'POST' ? route.abort() : route.continue());
        await field(first,'precision').fill('17');
        await first.getByRole('button',{name:'Save settings',exact:true}).click();
        await expect(first.locator('#preferences-status')).toContainText('Could not connect');
        await expect(field(first,'precision')).toHaveValue('17');
        await first.unroute('**/api/account/preferences'); await save(first);
        // Hold an already committed response while another browser changes accounts.
        let committed = false;
        const held = new Promise(resolve => unblock = resolve);
        await first.route('**/api/account/preferences', async route => {
            if (route.request().method() !== 'POST') return route.continue();
            const response = await route.fetch(); committed = true; await held;
            try { if (!closing) await route.fulfill({response}); }
            catch (error) { if (!closing) throw error; }
        });
        await field(first,'precision').fill('19');
        await first.getByRole('button',{name:'Save settings',exact:true}).click();
        await expect.poll(() => committed).toBe(true);
        // A separate context cannot change first's cookies; switch first's session through its shared second tab.
        const tab = await first.context().newPage(); await tab.goto('/login?lang=en');
        await tab.getByRole('button',{name:'Sign out',exact:true}).click();
        await expect(tab.locator('#account-form')).toBeVisible();
        await tab.getByLabel('Username',{exact:true}).fill('container-other');
        await tab.getByLabel('Password',{exact:true}).fill('test-only');
        await tab.getByRole('button',{name:'Sign in',exact:true}).click();
        await expect(tab.locator('#account-name')).toHaveText('container-other');
        await expect(first.locator('#account-name')).toHaveText('container-other');
        unblock(); await first.unrouteAll({behavior:'wait'});
        await expect(field(first,'precision')).toHaveValue('10');
        await first.goto('/?lang=en');
        await expect(first.locator('#precision-mode')).toHaveValue('auto');
        await expect(first.locator('[data-angle="rad"]')).toHaveAttribute('aria-pressed','true');
        await tab.getByRole('button',{name:'Sign out',exact:true}).click();
        await expect(tab.locator('#account-form')).toBeVisible();
        await expect(first.locator('#precision-mode')).toHaveValue('custom');
        await expect(first.locator('#precision')).toHaveValue('3');
        await expect(first.locator('#notation-mode')).toHaveValue('plain');
        console.log('Preferences UI: explicit guest import, second browser, no restore calculation, CAS conflict, offline retry, account switch and delayed-response isolation passed.');
    } finally { closing = true; unblock?.(); for (const context of contexts) await context.close(); await browser.close(); }
})().catch(error => {console.error(error);process.exitCode=1;});
