const {test,expect}=require('@playwright/test');
for (const lang of ['sk','en']) for (const units of [false,true]) {
 test(`server calculation timing ${lang} ${units?'units':'calculator'}`,async({page})=>{
  await page.setViewportSize({width:lang==='sk'?390:1280,height:844});
  let elapsed=501;
  await page.route(units?'**/api/convert?**':'**/api/evaluate?**',async route=>{
   const response=await route.fetch(); const body=await response.json();
   if(body.ok && (units || !new URL(route.request().url()).searchParams.get('action')?.includes('start')))body.elapsed_ms=elapsed;
   await route.fulfill({response,json:body});
  });
  await page.goto((units?'/units':'/')+'?lang='+lang);
  const input=page.locator(units?'#unit-input':'#expression');
  const timing=page.locator(units?'#unit-result-time':'#result-time');
  await input.fill('12');
  await expect(timing).toHaveText(lang==='en'?'Calculation time: 0.501 s':'Čas výpočtu: 0,501 s');
  elapsed=500;await input.fill('13');await expect(timing).toBeHidden();
  await expect(page.locator(units?'#unit-result':'#result')).not.toBeEmpty();
  elapsed=1234;await input.fill('14');
  await expect(timing).toHaveText(lang==='en'?'Calculation time: 1.234 s':'Čas výpočtu: 1,234 s');
  await input.fill('');await expect(timing).toBeHidden();
 });
}
