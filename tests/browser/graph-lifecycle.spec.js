const {test,expect}=require('@playwright/test');
for(const width of [390,1280])test(`leaving four queued curves cancels work and restores only inputs at width ${width}`,async({page})=>{
 await page.setViewportSize({width,height:844});await page.emulateMedia({reducedMotion:'reduce'});await page.goto('/graph?lang=en');
 const functions=['sin(33x)','cos(17x)','x*x','x/5'];
 for(let i=0;i<4;i++){if(i)await page.locator('#graph-add').click();await page.locator('.graph-curve').nth(i).locator('input:not([type=color])').fill(functions[i]);}
 const calls=[],errors=[];page.on('pageerror',e=>errors.push(String(e)));page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))calls.push(r.postData());});
 const navigate=async target=>{if(await page.locator('.nav-toggle').isVisible())await page.locator('.nav-toggle').click();await page.locator(`.primary-nav a[href="/${target}?lang=en"]`).click();};
 let release,waiting=false,hold=true;const held=new Promise(r=>release=r);
 await page.route('**/api/graph/plot?**',async route=>{const response=await route.fetch();if(hold){hold=false;waiting=true;await held;}try{await route.fulfill({response});}catch(_) {}});
 try{
  await page.locator('#graph-submit').click();await expect.poll(()=>waiting).toBe(true);expect(calls).toEqual([functions[0]]);
  await navigate('units');await expect(page.locator('#unit-converter')).toBeVisible();release();
  await page.setViewportSize({width,height:900});await page.waitForTimeout(300);expect(calls).toEqual([functions[0]]);
  await expect(page.locator('body')).not.toHaveClass(/graph-expanded-open/);
  await navigate('graph');await expect(page.locator('.graph-curve')).toHaveCount(4);
  for(let i=0;i<4;i++)await expect(page.locator('.graph-curve').nth(i).locator('input:not([type=color])')).toHaveValue(functions[i]);
  await expect(page.locator('path.graph-line')).toHaveCount(0);
  await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
  expect(calls.slice(1,5)).toEqual(functions);await expect(page.locator('#graph-status')).toContainText('4 functions plotted');expect(errors).toEqual([]);
 }finally{release();}
});
