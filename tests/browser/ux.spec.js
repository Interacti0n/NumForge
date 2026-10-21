const {test,expect}=require('@playwright/test');

for(const lang of ['sk','en']) {
 test(`result status follows core provenance and confirmation ${lang}`,async({page})=>{
  await page.goto('/?lang='+lang);
  await page.locator('[data-example="2+2"]').click();
  await expect(page.locator('#result')).toHaveText('4');
  const status=page.locator('#result-status');
  await expect(status).toContainText(lang==='sk'?'Náhľad':'Preview');
  await expect(status).toContainText(lang==='sk'?'Exaktná hodnota':'Exact value');
  await expect(page.locator('#history-list li')).toHaveCount(0);
  await page.locator('#expression').press('Enter');
  await expect(status).toContainText(lang==='sk'?'Potvrdené':'Confirmed');
  await expect(page.locator('#history-list li')).toHaveCount(1);
  await page.locator('#expression').fill('sqrt(2)');
  await expect(page.locator('#result')).toContainText('1.4142');
  await expect(status).toContainText(lang==='sk'?'Približný výpočet':'Approximate calculation');
  await page.locator('.header-guide').click();
  await page.locator('.brand').click();
  await expect(status).toContainText(lang==='sk'?'Približný výpočet':'Approximate calculation');
  await page.locator('#expression').fill('1/0');
  await page.locator('#expression').press('Enter');
  await expect(page.locator('#result')).toHaveClass('error');
  await expect(status).toHaveText(lang==='sk'?'Skontroluj výraz':'Check the expression');
  await page.locator('.keypad-card [data-action=clear]').click();
  await expect(page.locator('#result-examples')).toBeVisible();
  await expect(status).toBeHidden();
 });
 test(`short desktop windows keep a full result line ${lang}`,async({page})=>{
  for(const height of [720,600,420]) {
   await page.setViewportSize({width:1280,height});
   await page.goto('/?lang='+lang);
   await page.locator('#expression').fill('2+2');
   await page.locator('#expression').press('Enter');
   await expect(page.locator('#result')).toHaveText('4');
   const visible=await page.locator('#result').evaluate(n=>({height:n.clientHeight,line:parseFloat(getComputedStyle(n).lineHeight),scroll:n.scrollHeight}));
   expect(visible.height).toBeGreaterThanOrEqual(Math.floor(visible.line+14));
   expect(visible.scroll).toBeLessThanOrEqual(visible.height+1);
   await expect(page.locator('#expand-result')).toBeHidden();
  }
 });
 test(`mobile settings and compact categories remain usable ${lang}`,async({page})=>{
  for(const [width,height] of [[390,844],[812,375]]) {
   await page.setViewportSize({width,height});await page.goto('/?lang='+lang);
   const [settings,input]=await page.evaluate(()=>['.precision','.expression-card'].map(s=>document.querySelector(s).getBoundingClientRect().toJSON()));
   expect(settings.bottom).toBeLessThan(input.top);
   await expect(page.locator('#settings-controls')).toBeVisible();
   expect(await page.locator('.function-tabs').evaluate(n=>n.clientHeight)).toBeLessThan(90);
   await page.locator('#function-tab-0').focus();await page.keyboard.press('End');
   await expect(page.locator('#function-tab-7')).toBeFocused();
   await expect(page.locator('.constants-library')).toBeVisible();
   expect(await page.evaluate(()=>document.documentElement.scrollWidth)).toBeLessThanOrEqual(width);
   await page.goto('/units?lang='+lang);
   await expect(page.locator('#unit-categories')).toBeHidden();
   await page.locator('#unit-category-toggle').click();
   await expect(page.locator('#unit-categories')).toBeVisible();
  }
 });
 test(`mobile plotting reveals the graph after multiple curves ${lang}`,async({page})=>{
  await page.setViewportSize({width:390,height:844});await page.goto('/graph?lang='+lang);
  for(const [i,value] of ['sin(x)','cos(x)','x/5'].entries()) {
   if(i) await page.locator('#graph-add').click();
   await page.locator('.graph-curve').nth(i).locator('input:not([type=color])').fill(value);
  }
  await page.locator('#graph-submit').click();
  await expect(page.locator('path.graph-line')).toHaveCount(3);
  await expect(page.locator('#graph-plot')).toBeInViewport({ratio:.9});
  await page.locator('.graph-edit').first().click();
  await page.locator('#graph-library-expression').fill('sin(2x)');
  await page.locator('#graph-library-plot').click();
  await expect(page.locator('#graph-plot')).toBeInViewport({ratio:.9});
  await expect(page.locator('.header-actions .register-link')).toHaveCount(0);
  await expect(page.locator('.footer-links .register-link')).toContainText(lang==='sk'?'pripravujeme':'planned');
 });
}

test('HTTP provenance survives display rounding, cache reuse and approximate variables',async({request})=>{
 const base='/api/evaluate?precision=0&angle=rad&client=10203040102030401020304010203040&revision=';
 const send=async(input,revision,action)=>(await request.post(base+revision+'&action='+action,{data:input})).json();
 await send('',1,'start');
 expect(await send('1/3',2,'preview')).toMatchObject({ok:true,approximate:false});
 expect(await send('a=sqrt(2)',3,'commit')).toMatchObject({ok:true,approximate:true});
 expect(await send('a-a',4,'preview')).toMatchObject({ok:true,approximate:true});
 expect(await send('a-a',5,'preview')).toMatchObject({ok:true,approximate:true,cached:true});
});
