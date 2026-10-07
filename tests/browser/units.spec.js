const {test,expect}=require('@playwright/test');

for(const lang of ['sk','en']) test.describe(lang,()=>{
    test('categories, exact preview, confirmation, swap, copy and approximation',async({page})=>{
        await page.goto('/units?lang='+lang);
        await expect(page.locator('#unit-result')).toHaveText('1000');
        await expect(page.locator('[data-quantity]')).toHaveCount(10);
        await page.locator('#unit-input').fill('1/3');
        await expect(page.locator('#unit-result')).toHaveText('1000/3');
        await page.locator('#unit-input').press('Enter');
        await expect(page.locator('#unit-status')).toHaveText(lang==='sk'?'Prevod potvrdený':'Conversion confirmed');
        await page.locator('#unit-copy').click();
        expect(await page.evaluate(()=>navigator.clipboard.readText())).toBe('1000/3');
        await page.locator('#unit-swap').click();
        await expect(page.locator('#unit-result')).toHaveText('1/3000');
        await page.locator('[data-quantity="temperature"]').click();
        await page.locator('#unit-input').fill('100');
        await expect(page.locator('#unit-result')).toHaveText('212');
        await page.locator('[data-quantity="temperature_interval"]').click();
        await page.locator('#unit-input').fill('10');
        await expect(page.locator('#unit-result')).toHaveText('18');
        await page.locator('[data-quantity="angle"]').click();
        await page.locator('#unit-input').fill('180');
        await expect(page.locator('#unit-result')).toHaveText('3.1415926536');
        await expect(page.locator('#unit-result-meta')).toContainText('π');
        for(const quantity of ['length','area','volume','mass','time','speed','temperature','temperature_interval','information','angle']) {
            await page.locator(`[data-quantity="${quantity}"]`).click();
            expect(await page.locator('#unit-from option').evaluateAll(options=>options.map(o=>o.value)))
                .toEqual(await page.locator('#unit-to option').evaluateAll(options=>options.map(o=>o.value)));
            await expect(page.locator('#unit-from')).not.toHaveValue('');
            await expect(page.locator('#unit-to')).not.toHaveValue('');
        }
    });
    test('settings, invalid inputs, source links and responsive keyboard controls',async({page})=>{
        await page.goto('/units?lang='+lang);
        await expect(page.locator('#unit-result')).toHaveText('1000');
        await page.locator('[data-quantity="speed"]').click();
        await page.locator('#unit-input').fill('1');
        await page.locator('.unit-settings summary').click();
        await page.locator('#unit-places-mode').selectOption('custom');
        await page.locator('#unit-places').fill('2');await page.locator('#unit-places').dispatchEvent('change');
        await page.locator('#unit-notation').selectOption('plain');
        await page.locator('#unit-rounding').selectOption('floor');
        await expect(page.locator('#unit-result')).toHaveText('0.27');
        await page.locator('#unit-rounding').selectOption('ceiling');
        await expect(page.locator('#unit-result')).toHaveText('0.28');
        await page.locator('#unit-input').fill('rand()');
        await expect(page.locator('#unit-status')).toContainText(lang==='sk'?'Náhodné':'Random');
        await expect(page.locator('#unit-copy')).toBeDisabled();
        await page.locator('#unit-input').fill('x=5');
        await expect(page.locator('#unit-status')).toContainText(lang==='sk'?'Priradenia':'Assignments');
        await page.locator('#unit-input').fill('');
        await expect(page.locator('#unit-result')).toBeEmpty();
        await expect(page.locator('#unit-copy')).toBeDisabled();
        await page.locator('.unit-info summary').click();
        await expect(page.locator('#unit-source-links a')).toHaveCount(2);
        for(const width of [1440,1024,768,375,320]) {
            await page.setViewportSize({width,height:800});
            expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBe(true);
            const controls=page.locator('#unit-from,#unit-to,#unit-swap,#unit-submit');
            for(const control of await controls.all()) expect((await control.boundingBox()).height).toBeGreaterThanOrEqual(40);
        }
        await page.locator('#unit-swap').focus();
        await expect(page.locator('#unit-swap')).toBeFocused();
        await page.keyboard.press('Enter');
        await expect(page.locator('#unit-from')).toHaveValue('m/s');
        await expect(page.locator('#unit-categories')).toBeHidden();
        await page.locator('#unit-category-toggle').click();
        await page.locator('[data-quantity="information"]').click();
        await expect(page.locator('#unit-from')).toHaveValue('MiB');
        await expect(page.locator('#unit-category-toggle')).toBeFocused();
        await expect(page.locator('#unit-categories')).toBeHidden();
    });
});

test('navigation preserves calculator variables, history and converter selections',async({page})=>{
    await page.goto('/?lang=en');
    await page.locator('#expression').fill('x=2/3');await page.locator('#expression').press('Enter');
    await expect(page.locator('#history-list li')).toHaveCount(1);
    await page.locator('.primary-nav a[href="/units?lang=en"]').click();
    await expect(page.locator('#unit-result')).toHaveText('1000');
    await page.locator('#unit-input').fill('x+ans');
    await expect(page.locator('#unit-result')).toHaveText('4000/3');
    await page.locator('#unit-input').press('Enter');
    await page.locator('.language-switch a[lang="sk"]').click();
    await expect(page.locator('#unit-input')).toHaveValue('x+ans');
    await expect(page.locator('#unit-result')).toHaveText('4000/3');
    await page.locator('.primary-nav a[href="/?lang=sk"]').click();
    await expect(page.locator('#history-list li')).toHaveCount(1);
    await page.locator('#expression').fill('x+ans');await page.locator('#expression').press('Enter');
    await expect(page.locator('#result')).toHaveText('4/3');
});

test('expanded desktop workbench fits the viewport while smaller screens scroll naturally',async({page})=>{
    await page.goto('/units?lang=sk');
    await expect(page.locator('#unit-result')).toHaveText('1000');
    await page.locator('.unit-settings summary').click();
    await page.locator('#unit-places-mode').selectOption('custom');
    await page.locator('.unit-info summary').click();
    await page.locator('[data-quantity="temperature"]').click();
    await page.locator('#unit-input').press('Enter');
    await expect(page.locator('#unit-history-list li')).toHaveCount(1);
    await page.locator('.unit-example-card summary').click();
    for(const [width,height] of [[1920,1080],[1440,900],[1366,768],[1280,768]]) {
        await page.setViewportSize({width,height});
        expect(await page.evaluate(()=>{
            const shell=document.querySelector('.units-shell');
            const converter=document.querySelector('.units-column > .card');
            const sidebar=document.querySelector('.units-sidebar');
            return {page:document.documentElement.scrollHeight<=innerHeight+1,
                shell:shell.getBoundingClientRect().bottom<=innerHeight+1,
                converter:converter.scrollHeight<=converter.clientHeight+1,
                sidebar:sidebar.scrollHeight<=sidebar.clientHeight+1};
        }),`${width} × ${height}`).toEqual({page:true,shell:true,converter:true,sidebar:true});
        await expect(page.locator('#unit-copy')).toBeInViewport();
        await expect(page.locator('.page-footer')).toBeInViewport();
        await expect(page.locator('#unit-history-clear')).toBeInViewport();
    }
    for(const [width,height] of [[1100,700],[1024,768],[1440,600],[768,800],[375,667]]) {
        await page.setViewportSize({width,height});
        expect(await page.evaluate(()=>document.documentElement.scrollHeight>innerHeight && getComputedStyle(document.body).overflowY!=='hidden')).toBe(true);
        await page.locator('.page-footer').scrollIntoViewIfNeeded();
        await expect(page.locator('.page-footer')).toBeInViewport();
    }
});

test('catalogue and conversion failures recover, and stale previews cannot overwrite a new result',async({page})=>{
    let failed=true;
    await page.route('**/api/units',route=>failed?route.fulfill({status:503,body:'Unavailable'}):route.continue());
    await page.goto('/units?lang=en');
    await expect(page.locator('#unit-retry')).toBeVisible();
    failed=false;await page.locator('#unit-retry').click();
    await expect(page.locator('#unit-result')).toHaveText('1000');
    let release;
    await page.route('**/api/convert?**',async route=>{
        if(route.request().postData()==='2') {
            await new Promise(resolve=>release=resolve);
            try { await route.fulfill({json:{ok:true,result:'2000',unit:'m',symbol:'m'}}); } catch (_) {}
        } else if(route.request().postData()==='3') await route.fulfill({status:500,body:'Oops'});
        else await route.continue();
    });
    await page.locator('#unit-input').fill('2');await expect.poll(()=>!!release).toBe(true);
    await page.locator('#unit-input').fill('4');
    await expect(page.locator('#unit-result')).toHaveText('4000');release();
    await expect(page.locator('#unit-result')).toHaveText('4000');
    await page.locator('#unit-input').fill('3');
    await expect(page.locator('#unit-retry')).toBeVisible();
    await expect(page.locator('#unit-copy')).toBeDisabled();
    await page.locator('#unit-input').fill('5');
    await expect(page.locator('#unit-result')).toHaveText('5000');
});

test('history keeps exact values, restores snapshots and ignores previews/errors',async({page})=>{
    await page.goto('/units?lang=en');
    await expect(page.locator('#unit-result')).toHaveText('1000');
    await expect(page.locator('#unit-history-list li')).toHaveCount(0);
    await page.locator('#unit-input').fill('1/3');
    await page.locator('.unit-settings summary').click();
    await page.locator('#unit-notation').selectOption('plain');
    await page.locator('#unit-places-mode').selectOption('custom');
    await page.locator('#unit-places').fill('2');await page.locator('#unit-places').dispatchEvent('change');
    await expect(page.locator('#unit-result')).toHaveText('333.33');
    await page.locator('#unit-input').press('Enter');
    await expect(page.locator('#unit-history-list li')).toHaveCount(1);
    expect(await page.evaluate(()=>window.numforgeUnitHistory.entries[0].body.value))
        .toMatchObject({kind:'rational',text:'1000/3',unit:'m',precision:34});
    await page.locator('#unit-history-list .unit-history-copy').click();
    expect(await page.evaluate(()=>navigator.clipboard.readText())).toBe('1000/3');
    await page.locator('#unit-input').fill('1/0');
    await expect(page.locator('#unit-status')).toContainText('Division by zero');
    await page.locator('#unit-input').press('Enter');
    await expect(page.locator('#unit-history-list li')).toHaveCount(1);
    await page.locator('#unit-history-list .unit-history-restore').click();
    await expect(page.locator('#unit-result')).toHaveText('333.33');
    await expect(page.locator('#unit-status')).toContainText('Stored conversion');
    await page.locator('.language-switch a[lang="sk"]').click();
    await expect(page.locator('#unit-history-list li')).toHaveCount(1);
    await page.reload();
    await expect(page.locator('#unit-history-list li')).toHaveCount(1);
    await page.locator('#unit-history-clear').click();
    await expect(page.locator('#unit-history-list li')).toHaveCount(0);
});

test('history freezes calculator-dependent values and bounds confirmations without duplicates',async({page})=>{
    await page.goto('/?lang=en');
    await page.locator('#expression').fill('x=2');await page.locator('#expression').press('Enter');
    await expect(page.locator('#history-list li')).toHaveCount(1);
    await page.locator('.primary-nav a[href^="/units"]').click();
    await expect(page.locator('#unit-result')).toHaveText('1000');
    await page.locator('#unit-input').fill('x+ans');await page.locator('#unit-input').press('Enter');
    await expect(page.locator('#unit-history-list li')).toHaveCount(1);
    await page.locator('.primary-nav a[href="/?lang=en"]').click();
    await expect(page.locator('#history-list li')).toHaveCount(1);
    await page.locator('#expression').fill('x=3');await page.locator('#expression').press('Enter');
    await expect(page.locator('#history-list li')).toHaveCount(2);
    await page.locator('.primary-nav a[href^="/units"]').click();
    await expect(page.locator('#unit-result')).toHaveText('6000');
    await page.locator('#unit-history-list .unit-history-restore').click();
    await expect(page.locator('#unit-result')).toHaveText('4000');
    await expect(page.locator('#unit-history-list li')).toHaveCount(1);
    let release;
    await page.route('**/api/convert?**snapshot=1**',async route=>{
        await new Promise(resolve=>release=resolve);await route.continue();
    });
    await page.locator('#unit-input').fill('1');await page.locator('#unit-input').press('Enter');
    await expect.poll(()=>!!release).toBe(true);
    await page.locator('#unit-input').press('Enter');release();
    await expect(page.locator('#unit-history-list li')).toHaveCount(2);
    await page.unroute('**/api/convert?**snapshot=1**');
    for(let i=0;i<16;i++) {
        await page.locator('#unit-input').fill(String(i+2));await page.locator('#unit-input').press('Enter');
        await expect.poll(()=>page.evaluate(()=>window.numforgeUnitHistory.sequence)).toBe(i+3);
    }
    await expect(page.locator('#unit-history-list li')).toHaveCount(16);
    expect(await page.evaluate(()=>window.numforgeUnitHistory.entries[0].number)).toBe(3);
});

test('a lost confirmation response can be retried without duplicate history',async({page})=>{
    await page.goto('/units?lang=en');await expect(page.locator('#unit-result')).toHaveText('1000');
    let fail=true;
    await page.route('**/api/convert?**snapshot=1**',async route=>{
        if(fail){fail=false;await route.fetch();await route.abort('failed');}
        else await route.continue();
    });
    await page.locator('#unit-input').press('Enter');
    await expect(page.locator('#unit-retry')).toBeVisible();
    await expect(page.locator('#unit-history-list li')).toHaveCount(0);
    await page.locator('#unit-retry').click();
    await expect(page.locator('#unit-history-list li')).toHaveCount(1);
});
