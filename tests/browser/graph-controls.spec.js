const {test,expect}=require('@playwright/test');
async function color(page,value){
 await page.locator('#graph-color').evaluate((input,value)=>{input.value=value;input.dispatchEvent(new Event('input',{bubbles:true}));},value);
}
for(const [lang,width] of [['sk',390],['en',1280]])test(`simple graph controls, color and radians ${lang}`,async({page})=>{
 await page.setViewportSize({width,height:844});await page.goto('/graph?lang='+lang);
 await expect(page.locator('#graph-form input')).toHaveCount(2);await expect(page.locator('#graph-form select,#graph-form details')).toHaveCount(0);
 await expect(page.getByLabel(lang==='sk'?'Farba krivky':'Curve color')).toHaveAttribute('type','color');
 const calls=[];page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))calls.push(r);});
 await page.locator('#graph-expression').press('Enter');await expect(page.locator('path.graph-line')).toBeVisible();
 const before=calls.length;await color(page,'#20c997');
 await expect(page.locator('path.graph-line')).toHaveCSS('stroke','rgb(32, 201, 151)');
 await page.waitForTimeout(350);expect(calls.length).toBe(before);
 await page.locator('#graph-zoom-in').click();await expect(page.locator('path.graph-line')).toHaveCSS('stroke','rgb(32, 201, 151)');
 for(const call of calls){const q=new URL(call.url()).searchParams;expect(q.get('angle')).toBe('rad');
  for(const key of ['precision','client','at','color'])expect(q.has(key)).toBe(false);
 }
 await expect(page.locator('#graph-status')).not.toContainText('real samples');await expect(page.locator('#graph-status')).not.toContainText('reálne body');
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBe(true);
});

test('expression and color persist through language navigation and reload',async({page})=>{
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('cos(t)');await color(page,'#ff8c42');
 await page.evaluate(()=>window.originalHeader=document.querySelector('.page-header'));
 await page.locator('.language-switch a[lang=sk]').click();
 await expect(page.locator('#graph-expression')).toHaveValue('cos(t)');await expect(page.locator('#graph-color')).toHaveValue('#ff8c42');
 expect(await page.evaluate(()=>window.originalHeader===document.querySelector('.page-header'))).toBe(true);
 await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toHaveCSS('stroke','rgb(255, 140, 66)');
 await page.reload();await expect(page.locator('#graph-color')).toHaveValue('#ff8c42');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');
});

test('legacy precision, angle and calculator state do not affect a new graph',async({page})=>{
 await page.addInitScript(()=>{
  sessionStorage.setItem('numforge-graph-state',JSON.stringify({expression:'sin(t)',color:'invalid',precision:'10000',angle:'deg',session:true,min:'100',max:'200',ymin:'100',ymax:'200'}));
  sessionStorage.setItem('numforge-client-id','aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa');
 });
 await page.goto('/graph?lang=en');await expect(page.locator('#graph-color')).toHaveValue('#dfafff');
 const ready=page.waitForResponse(r=>r.url().includes('/api/graph/plot?'));
 await page.locator('#graph-submit').click();const response=await ready,q=new URL(response.url()).searchParams,data=await response.json();
 expect(q.get('angle')).toBe('rad');expect(q.has('precision')).toBe(false);expect(q.has('client')).toBe(false);
 expect(data.schema_version).toBe(3);expect(q.get('min')).toBe('-30');
 for(const p of data.points)expect(p[1]).toBeCloseTo(Math.sin(p[0]),9);
});

test('changing color during sampling preserves the in-flight result',async({page})=>{
 let release;const held=new Promise(resolve=>release=resolve);let calls=0;
 await page.route('**/api/graph/plot?**',async route=>{++calls;const response=await route.fetch();await held;await route.fulfill({response});});
 await page.goto('/graph?lang=en');await page.locator('#graph-submit').click();
 await expect(page.locator('.graph-workspace')).toHaveAttribute('aria-busy','true');await color(page,'#4da6ff');
 release();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 await expect(page.locator('path.graph-line')).toHaveCSS('stroke','rgb(77, 166, 255)');expect(calls).toBe(1);
});
