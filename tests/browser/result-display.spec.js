const {test,expect}=require('@playwright/test');

for(const width of [390,1280]) test(`readable results preserve copied values ${width}`,async({page})=>{
 await page.setViewportSize({width,height:844});
 await page.route('**/api/evaluate?**',async route=>{
  if(route.request().postData()==='400000!') await route.fulfill({json:{ok:true,result:'2.5344860463E+2067109',approximate:false}});
  else await route.continue();
 });
 await page.goto('/?lang=sk');
 const input=page.locator('#expression'), result=page.locator('#result');
 await input.fill('400000!');
 await expect(result.locator('sup')).toHaveText('2067109');
 await expect(result).toHaveText('2.5344860463 × 102067109');
 await expect(result).toHaveAccessibleName('2.5344860463 × 10^+2067109');
 await page.locator('#copy-result').click();
 expect(await page.evaluate(()=>navigator.clipboard.readText())).toBe('2.5344860463E+2067109');
 await page.locator('#notation-mode').selectOption('scientific');
 await expect(result).toHaveText('2.5344860463E+2067109');
 await expect(result.locator('sup')).toHaveCount(0);
 await page.locator('#notation-mode').selectOption('plain');
 await input.fill('-1234567.125');
 await expect(result).toHaveText('-1\u202f234\u202f567.125');
 await page.locator('#copy-result').click();
 expect(await page.evaluate(()=>navigator.clipboard.readText())).toBe('-1234567.125');
 await page.locator('#notation-mode').selectOption('auto');
 await input.fill('10^-100');
 await expect(result.locator('sup')).toHaveText('-100');
 expect(await page.evaluate(()=>document.documentElement.scrollWidth)).toBeLessThanOrEqual(width);
 await input.fill('1/0');await expect(result).toHaveClass('error');
 await expect(result).not.toHaveAttribute('aria-label');
 await expect(result.locator('sup')).toHaveCount(0);
});

test('unit results and mathematical notation use superscripts',async({page})=>{
 await page.goto('/units?lang=en');
 await page.locator('#unit-input').fill('10^100');
 await expect(page.locator('#unit-result sup')).toHaveText(/\d+/);
 await page.goto('/?lang=en');
 await page.locator('#notation-mode').selectOption('math');
 await page.locator('#expression').fill('123.456');
 await expect(page.locator('#result sup')).toHaveText('2');
 await page.locator('#copy-result').click();
 expect(await page.evaluate(()=>navigator.clipboard.readText())).toBe('1.23456E+2');
});

test('real factorial result and phase tooltip fit a mobile screen',async({page})=>{
 await page.setViewportSize({width:390,height:844});
 await page.goto('/?lang=sk');
 await page.locator('#expression').fill('200000!');
 await page.locator('#expression').press('Enter');
 await expect(page.locator('#result sup')).toHaveText('973350',{timeout:12000});
 await page.screenshot({path:'../../build/browser-artifacts/result-math-mobile.png',fullPage:true});
 await page.locator('#result-time summary').click();
 await expect(page.locator('#result-time dt')).toHaveCount(9);
 const box=await page.locator('.timing-breakdown').boundingBox();
 expect(box.x).toBeGreaterThanOrEqual(0);expect(box.x+box.width).toBeLessThanOrEqual(390);
 await page.screenshot({path:'../../build/browser-artifacts/result-timing-mobile.png',fullPage:true});
});
