const {test,expect}=require('@playwright/test');

for(const lang of ['sk','en']) {
    test(`mobile calculator reveals confirmed results without scrolling previews (${lang})`,async({page})=>{
        await page.setViewportSize({width:390,height:844});
        await page.goto('/?lang='+lang);
        await page.locator('#expression').fill('2+3');
        const before=await page.evaluate(()=>scrollY);
        await expect(page.locator('#result')).toHaveText('5');
        expect(Math.abs(await page.evaluate(()=>scrollY)-before)).toBeLessThan(2);
        await page.locator('.primary-button').click();
        await expect(page.locator('#history-list li')).toHaveCount(1);
        await expect.poll(()=>page.locator('.result-panel').evaluate(n=>Math.round(n.getBoundingClientRect().top))).toBe(88);
        await page.locator('#expression').fill('1/0');
        await page.locator('.primary-button').click();
        await expect(page.locator('#result')).toHaveClass('error');
        await expect.poll(()=>page.locator('.result-panel').evaluate(n=>Math.round(n.getBoundingClientRect().top))).toBe(88);
    });
    test(`mobile converter is compact with history below its revealed result (${lang})`,async({page})=>{
        await page.setViewportSize({width:390,height:844});
        await page.goto('/units?lang='+lang);
        await expect(page.locator('#unit-result')).toHaveText('1000');
        await expect(page.locator('.units-column > .intro')).toBeHidden();
        await expect(page.locator('#unit-input-help')).toBeHidden();
        await expect(page.locator('.units-column > .unit-history-card')).toBeVisible();
        expect(await page.evaluate(()=>document.querySelector('[aria-labelledby="unit-result-title"]').compareDocumentPosition(document.querySelector('.unit-history-card'))&Node.DOCUMENT_POSITION_FOLLOWING)).toBeTruthy();
        await page.locator('#unit-input').fill('2');
        const before=await page.evaluate(()=>scrollY);
        await expect(page.locator('#unit-result')).toHaveText('2000');
        expect(Math.abs(await page.evaluate(()=>scrollY)-before)).toBeLessThan(2);
        await page.locator('#unit-submit').click();
        await expect(page.locator('#unit-history-list li')).toHaveCount(1);
        const card=page.locator('.units-column > .card[aria-labelledby="unit-result-title"]');
        await expect.poll(()=>card.evaluate(n=>{
            const top=n.getBoundingClientRect().top;
            const limit=Math.max(88,top+scrollY-(document.documentElement.scrollHeight-innerHeight));
            return Math.abs(top-limit)<2;
        })).toBe(true);
        await expect(page.locator('#unit-result')).toBeInViewport({ratio:1});
        await page.setViewportSize({width:812,height:375});
        await expect(page.locator('.units-column > .unit-history-card')).toBeVisible();
        await expect(page.locator('.units-column > .intro')).toBeHidden();
        await page.locator('#unit-input').fill('3');
        await expect(page.locator('#unit-result')).toHaveText('3000');
        await page.locator('#unit-submit').click();
        await expect(page.locator('#unit-history-list li')).toHaveCount(2);
        await expect.poll(()=>card.evaluate(n=>{
            const top=n.getBoundingClientRect().top;
            const limit=Math.max(88,top+scrollY-(document.documentElement.scrollHeight-innerHeight));
            return Math.abs(top-limit)<2;
        })).toBe(true);
        await expect(page.locator('#unit-result')).toBeInViewport({ratio:1});
        await page.setViewportSize({width:1440,height:900});
        await expect(page.locator('.units-sidebar > .unit-history-card')).toBeVisible();
        await expect(page.locator('#unit-input-help')).toBeVisible();
    });
}
