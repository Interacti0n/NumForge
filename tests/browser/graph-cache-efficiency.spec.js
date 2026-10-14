const {test,expect}=require('@playwright/test');
const query=r=>new URL(r.url()).searchParams;
const plot=async page=>{await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');};
for(const width of [390,1280])test(`prepared resolution levels survive repeated zoom and reset at width ${width}`,async({page})=>{
 await page.setViewportSize({width,height:844});await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('x*x');const calls=[];
 page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))calls.push(r.url());});await plot(page);await page.waitForTimeout(500);
 for(let i=0;i<3;i++){await page.locator('#graph-plot').press('+');await page.waitForTimeout(500);await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');}
 await page.locator('#graph-reset').click();await page.waitForTimeout(500);const before=calls.length;
 for(let i=0;i<3;i++){await page.locator('#graph-plot').press('+');await page.waitForTimeout(500);await expect(page.locator('path.graph-line')).toHaveCount(1);}
 await page.locator('#graph-reset').click();await page.waitForTimeout(500);expect(calls.length).toBe(before);
});
test('expanded graph prepares only the central next view while keeping expansion cached',async({page})=>{
 await page.goto('/graph?lang=en');const calls=[];page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))calls.push(query(r));});await plot(page);await page.waitForTimeout(500);
 const before=calls.length;await page.locator('#graph-expand').click();await page.waitForTimeout(400);expect(calls.length).toBe(before);
 const detail=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&Number(query(r).get('min'))===-2.5&&Number(query(r).get('max'))===2.5&&Number(query(r).get('ymin'))===-2.5);
 await page.locator('#graph-plot').press('+');const response=await detail,q=query(response);expect(Number(q.get('width'))).toBe(await page.locator('#graph-plot').evaluate(svg=>Math.round(svg.viewBox.baseVal.width)));
 await page.locator('#graph-plot').press('+');await expect(page.locator('path.graph-line')).toBeVisible();await page.waitForTimeout(350);
 expect(calls.slice(before).some(q=>q.get('min')==='-2.5'&&q.get('max')==='2.5'&&q.get('ymin')==='-2.5'&&q.get('quality')==='coarse')).toBe(false);
});
test('unprepared region still paints a coarse result while refinement is held',async({page})=>{
 await page.goto('/graph?lang=en');await plot(page);await page.waitForTimeout(500);let release,waiting=false;const held=new Promise(resolve=>release=resolve),qualities=[];
 await page.route('**/api/graph/plot?**',async route=>{const q=query(route.request());qualities.push(q.get('quality'));const response=await route.fetch();if(q.get('quality')==='refined'){waiting=true;await held;}try{await route.fulfill({response});}catch(_) {}});
 await page.evaluate(()=>{const svg=document.querySelector('#graph-plot');for(let i=0;i<15;i++)svg.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowRight',bubbles:true,cancelable:true}));});
 await expect.poll(()=>waiting).toBe(true);expect(qualities.slice(0,2)).toEqual(['coarse','refined']);await expect(page.locator('path.graph-line')).toBeVisible();
 release();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
});
