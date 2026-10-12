const {test,expect}=require('@playwright/test');

for(const lang of ['sk','en']) test(`guide tracks reading position and keeps mobile navigation visible (${lang})`,async({page})=>{
    await page.setViewportSize({width:390,height:844});
    await page.goto('/api?lang='+lang);
    await expect(page.locator('.guide-toc a[aria-current="location"]')).toHaveAttribute('href','#intro');
    const sectionIds = ['intro','syntax','variables','functions','complex','units','precision','cache','random','history','limits','http','session-http','graph-http','library','documentation'];
    expect(await page.locator('.guide-toc a').evaluateAll(links=>links.map(link=>link.hash.slice(1))))
        .toEqual(sectionIds);
    expect(await page.locator('.guide-content h1,.guide-content h2').evaluateAll(headings=>headings.map(heading=>heading.id))).toEqual(sectionIds);
    for(const id of sectionIds.slice(1)) {
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
        await expect(page.locator('.guide-position')).toHaveText(String(sectionIds.indexOf(id)+1).padStart(2,'0')+' / '+sectionIds.length);
    }
    await page.locator('.guide-toc a[href="#limits"]').click();
    await expect(page).toHaveURL(/#limits$/);
    await expect(page.locator('.guide-toc a[aria-current="location"]')).toHaveAttribute('href','#limits');
    await page.locator('.guide-content').evaluate(n=>{n.scrollTop=n.scrollHeight;});
    await expect(page.locator('.guide-toc a[aria-current="location"]')).toHaveAttribute('href','#documentation');
    await expect(page.locator('.guide-position')).toHaveText(`${sectionIds.length} / ${sectionIds.length}`);
    await expect.poll(()=>page.evaluate(()=>new Promise(resolve=>{
        const positions=[];
        function sample(){
            positions.push(document.querySelector('.guide-toc').getBoundingClientRect().top);
            if(positions.length===12)resolve(new Set(positions).size);
            else requestAnimationFrame(sample);
        }
        requestAnimationFrame(sample);
    }))).toBe(1);
    // Reveal the first item as a user would scroll the horizontal mobile menu.
    await page.locator('.guide-toc').evaluate(n=>n.scrollTo({left:0,behavior:'instant'}));
    await page.locator('.guide-toc a[href="#intro"]').click();
    await expect(page.locator('.guide-toc a[aria-current="location"]')).toHaveAttribute('href','#intro');
    await expect(page.locator('.guide-position')).toHaveText('01 / '+sectionIds.length);
    const docsLinks = page.locator('.guide-content a[href^="https://github.com/Interacti0n/NumForge/blob/main/docs/"]');
    expect(await docsLinks.count()).toBeGreaterThanOrEqual(10);
    for(const link of await docsLinks.all()) {
        await expect(link).toHaveAttribute('target','_blank');
        await expect(link).toHaveAttribute('rel','noopener noreferrer');
    }
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
