const {test,expect}=require('@playwright/test');

for(const route of ['/', '/units', '/api']) test(`mobile header follows scroll direction on ${route}`,async({page})=>{
    await page.setViewportSize({width:390,height:844});
    await page.goto(route+'?lang=sk');
    const header=page.locator('.page-header');
    const scroll=async y=>page.evaluate(y=>{
        const pane=document.querySelector('.guide-content') || document.scrollingElement;
        pane.scrollTop=y;
    },y);
    await scroll(220);
    await expect(header).toHaveClass(/scroll-hidden/);
    await expect.poll(()=>header.evaluate(n=>n.getBoundingClientRect().bottom)).toBeLessThanOrEqual(1);
    await scroll(160);
    await expect(header).not.toHaveClass(/scroll-hidden/);
    await page.locator('.nav-toggle').click();
    await scroll(300);
    await expect(header).not.toHaveClass(/scroll-hidden/);
    await page.keyboard.press('Escape');
    await page.locator('.nav-toggle').evaluate(n=>n.blur());
    await scroll(380);
    await expect(header).toHaveClass(/scroll-hidden/);
    await page.locator('.header-guide').focus();
    await expect(header).not.toHaveClass(/scroll-hidden/);
    await page.locator('.header-guide').evaluate(n=>n.blur());
    await scroll(440);
    await expect(header).toHaveClass(/scroll-hidden/);
    await page.setViewportSize({width:1280,height:720});
    await expect(header).not.toHaveClass(/scroll-hidden/);
    await page.setViewportSize({width:390,height:844});
    await scroll(0);
    await expect(header).not.toHaveClass(/scroll-hidden/);
});
