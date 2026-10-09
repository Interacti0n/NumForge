const {test,expect}=require('@playwright/test');

for(const lang of ['sk','en']) test(`guide tracks reading position and keeps mobile navigation visible (${lang})`,async({page})=>{
    await page.setViewportSize({width:390,height:844});
    await page.goto('/api?lang='+lang);
    await expect(page.locator('.guide-toc a[aria-current="location"]')).toHaveAttribute('href','#intro');
    expect(await page.locator('.guide-toc a').evaluateAll(links=>links.map(link=>link.hash.slice(1))))
        .toEqual(await page.locator('.guide-content h1,.guide-content h2').evaluateAll(headings=>headings.map(heading=>heading.id)));
    for(const id of ['precision','functions','http','history','syntax']) {
        await page.evaluate(id=>{
            const content=document.querySelector('.guide-content'), heading=document.getElementById(id);
            content.scrollTop+=heading.getBoundingClientRect().top-content.getBoundingClientRect().top-30;
        },id);
        await expect(page.locator('.guide-toc a[aria-current="location"]')).toHaveAttribute('href','#'+id);
        await expect.poll(()=>page.locator('.guide-toc a[aria-current="location"]').evaluate(n=>{
            const item=n.getBoundingClientRect(),bar=n.parentElement.getBoundingClientRect();
            return item.left>=bar.left-1 && item.right<=bar.right+1;
        })).toBe(true);
        expect(await page.locator('.guide-toc').evaluate(n=>n.getBoundingClientRect().top)).toBeGreaterThanOrEqual(0);
        expect(new URL(page.url()).hash).toBe('');
        if(id==='precision')await expect(page.locator('.guide-position')).toHaveText(lang==='sk'?'03 / 10':'07 / 10');
    }
    await page.locator('.guide-toc a[href="#http"]').click();
    await expect(page).toHaveURL(/#http$/);
    await expect(page.locator('.guide-toc a[aria-current="location"]')).toHaveAttribute('href','#http');
    await page.locator('.guide-content').evaluate(n=>{n.scrollTop=n.scrollHeight;});
    await expect(page.locator('.guide-toc a[aria-current="location"]')).toHaveAttribute('href','#session-http');
    await expect(page.locator('.guide-position')).toHaveText('10 / 10');
    await page.locator('.guide-toc a[href="#intro"]').click();
    await expect(page.locator('.guide-toc a[aria-current="location"]')).toHaveAttribute('href','#intro');
    await expect(page.locator('.guide-position')).toHaveText('01 / 10');
    await page.locator('.guide-toc a[href="#session-http"]').click();
    await expect(page).toHaveURL(/#session-http$/);
    await expect(page.locator('.guide-toc a[aria-current="location"]')).toHaveAttribute('href','#session-http');
    for(const [width,height] of [[320,568],[812,375],[1280,720],[390,844]]) {
        await page.setViewportSize({width,height});
        expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBe(true);
        await expect(page.locator('.guide-toc a[aria-current="location"]')).toHaveCount(1);
    }
    await page.locator('.nav-toggle').click();
    await page.locator('.primary-nav a[href="/?lang='+lang+'"]').click();
    await expect(page.locator('#expression')).toBeVisible();
    await page.locator('.guide-link').click();
    await expect(page.locator('.guide-nav-heading')).toHaveCount(1);
    await expect(page.locator('.guide-toc a[aria-current="location"]')).toHaveAttribute('href','#intro');
});
