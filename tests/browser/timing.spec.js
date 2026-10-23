const {test,expect}=require('@playwright/test');
for (const lang of ['sk','en']) for (const units of [false,true]) {
 test(`server calculation timing ${lang} ${units?'units':'calculator'}`,async({page})=>{
  await page.setViewportSize({width:lang==='sk'?390:1280,height:844});
  let elapsed=501;
  await page.route(units?'**/api/convert?**':'**/api/evaluate?**',async route=>{
   const response=await route.fetch(); const body=await response.json();
   if(body.ok && (units || !new URL(route.request().url()).searchParams.get('action')?.includes('start'))) {
    body.elapsed_ms=elapsed;
    body.timings_us={parse:1000,evaluate:elapsed*1000-8000,convert:1000,normalize:1000,round:1000,compose:1000,format:1000,serialize:1000,other:1000};
   }
   await route.fulfill({response,json:body});
  });
  await page.goto((units?'/units':'/')+'?lang='+lang);
  const input=page.locator(units?'#unit-input':'#expression');
  const timing=page.locator(units?'#unit-result-time':'#result-time');
  await input.fill('12');
  await expect(timing.locator('summary')).toHaveText(lang==='en'?'Processing time: 0.501 s':'Čas spracovania: 0,501 s');
  await timing.locator('summary').focus();
  await expect(timing.locator('.timing-breakdown')).toBeVisible();
  await expect(timing.locator('dt')).toHaveCount(9);
  await expect(timing.locator('dd')).toContainText(['1 ms','493 ms']);
  await timing.locator('summary').click();
  await expect(timing).toHaveAttribute('open','');
  await expect(timing.locator('.timing-breakdown')).toContainText(lang==='en'?'Network transfer':'Prenos cez sieť');
  elapsed=500;await input.fill('13');await expect(timing).toBeHidden();
  await expect(page.locator(units?'#unit-result':'#result')).not.toBeEmpty();
  elapsed=1234;await input.fill('14');
  await expect(timing.locator('summary')).toHaveText(lang==='en'?'Processing time: 1.234 s':'Čas spracovania: 1,234 s');
  await input.fill('');await expect(timing).toBeHidden();
 });
}

test('HTTP phase totals cover processing and cached results omit stale timing', async ({request}) => {
 const client='fa'.repeat(16);
 const base='/api/evaluate?precision=10&angle=rad&client='+client;
 const send=async(expression,revision)=>(await request.post(base+'&revision='+revision,{data:expression})).json();
 const body=await send('200000!',1);
 expect(body.ok).toBe(true);
 expect(body.elapsed_ms).toBeGreaterThan(500);
 for(const value of Object.values(body.timings_us))expect(Number.isSafeInteger(value)&&value>=0).toBe(true);
 const total=Object.values(body.timings_us).reduce((sum,value)=>sum+value,0)/1000;
 expect(Math.abs(total-body.elapsed_ms)).toBeLessThan(5);
 for(const phase of ['evaluate','normalize','convert'])expect(body.timings_us[phase]).toBeGreaterThan(0);
 const cached=await send('200000!',2);
 expect(cached.result).toBe(body.result);expect(cached.cached).toBe(true);
 expect(cached.elapsed_ms).toBeUndefined();expect(cached.timings_us).toBeUndefined();
});

test('timing details open and close on a touchscreen',async({browser})=>{
 const context=await browser.newContext({viewport:{width:390,height:844},isMobile:true,hasTouch:true});
 const page=await context.newPage();
 try {
  await page.route('**/api/evaluate?**',async route=>{
   const response=await route.fetch(),body=await response.json();
   if(body.ok&&body.result)body.elapsed_ms=801;
   await route.fulfill({response,json:body});
  });
  await page.goto(test.info().project.use.baseURL+'/?lang=sk');
  await page.locator('#expression').fill('2+2');
  const details=page.locator('#result-time');
  await details.locator('summary').tap();await expect(details).toHaveAttribute('open','');
  await expect(details.locator('.timing-breakdown')).toBeVisible();
  await details.locator('summary').tap();await expect(details).not.toHaveAttribute('open','');
 } finally {await context.close();}
});
