const {test,expect}=require('@playwright/test');
async function sample(page,expression,min='-10',max='10') {
 await page.locator('#graph-expression').fill(expression);
 if((await page.locator('.graph-settings').getAttribute('open'))===null)await page.locator('.graph-settings summary').click();
 await page.locator('#graph-min').fill(min);await page.locator('#graph-max').fill(max);
 const ready=page.waitForResponse(r=>r.url().includes('/api/graph/plot?'));
 await page.locator('#graph-submit').click();const response=await ready;
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');return response.json();
}
for(const [lang,width] of [['sk',390],['en',1280]])test(`adaptive graph draws lines and typed gaps ${lang}`,async({page})=>{
 await page.setViewportSize({width,height:844});await page.goto('/graph?lang='+lang);
 await expect(page.locator('#graph-count,#graph-parameter,#graph-connect')).toHaveCount(0);
 const curve=await sample(page,'x*x');await expect(page.locator('#graph-plot')).toBeVisible();
 await expect(page.locator('circle.graph-point')).toHaveCount(0);await expect(page.locator('path.graph-line')).toHaveCount(1);
 expect(curve.points.length).toBeLessThan(1024);
 const reciprocal=await sample(page,'1/x');await expect(page.locator('path.graph-line')).toBeVisible();
 reciprocal.points.forEach((p,i)=>{if(p.connect){expect(p.kind).toBe('real');expect(reciprocal.points[i-1].kind).toBe('real');}});
 expect(reciprocal.points.some(p=>p.kind==='domain_error')).toBe(true);
 await page.locator('#graph-values summary').click();await expect(page.locator('#graph-table-body tr')).toHaveCount(reciprocal.count);
 await sample(page,'sqrt(t)');await expect(page.locator('#graph-status')).toContainText('f(t)');
 await expect(page.locator('#graph-table-body tr').first()).toContainText(lang==='en'?'Complex':'Komplexné');
 await sample(page,'qty(t;"m")');await expect(page.locator('#graph-plot')).toBeHidden();
 await expect(page.locator('#graph-table-body tr').first()).toContainText(lang==='en'?'Quantity':'Veličina');
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBe(true);
 await sample(page,'rand()');await expect(page.locator('#graph-status')).toHaveClass(/error/);
 await expect(page.locator('#graph-values')).toBeHidden();
});
test('adaptive browser numeric boundaries remain finite',async({page})=>{
 await page.goto('/graph?lang=en');await sample(page,'10^400');await expect(page.locator('#graph-plot')).toBeHidden();
 await page.locator('#graph-values summary').click();await expect(page.locator('#graph-table-body tr').first()).toContainText('Outside plotting range');
 await sample(page,'10^(-400)');await expect(page.locator('#graph-plot')).toBeHidden();
 await sample(page,'10^308*x','-1','1');await expect(page.locator('path.graph-line')).toBeVisible();
 expect(await page.locator('#graph-plot').evaluate(svg=>!/NaN|Infinity/.test(svg.innerHTML))).toBe(true);
 await sample(page,'0');await expect(page.locator('#graph-plot')).toBeVisible();await expect(page.locator('path.graph-line')).toHaveCount(1);
});
test('automatic parameter uses read-only session values and detects ambiguity',async({page,request})=>{
 await page.goto('/?lang=en');await page.locator('#expression').fill('a=2');await page.locator('#expression').press('Enter');await expect(page.locator('#result')).toHaveText('2');
 const client=await page.evaluate(()=>sessionStorage.getItem('numforge-client-id'));
 const read=async()=>Promise.all(['/api/session','/api/session/variables','/api/session/history'].map(async p=>(await request.get(p+'?client='+client)).json()));
 const before=await read();await page.evaluate(()=>window.graphHeader=document.querySelector('.page-header'));
 await page.locator('.primary-nav a[href^="/graph"]').click();await expect(page.locator('#graph-form')).toBeVisible();
 await page.locator('#graph-session').check();const result=await sample(page,'a*t');expect(result.parameter).toBe('t');expect(await read()).toEqual(before);
 await page.locator('.language-switch a[lang=sk]').click();await expect(page.locator('#graph-expression')).toHaveValue('a*t');
 await sample(page,'a*t');await expect(page.locator('path.graph-line')).toBeVisible();expect(await page.evaluate(()=>window.graphHeader===document.querySelector('.page-header'))).toBe(true);
 await page.locator('#graph-session').uncheck();await sample(page,'x+y');await expect(page.locator('#graph-status')).toContainText('jednu neznámu');
});
test('editing cancels a pending adaptive plot',async({page})=>{
 let release;const paused=new Promise(resolve=>release=resolve);
 await page.route('**/api/graph/plot?**',async route=>{const response=await route.fetch();await paused;try{await route.fulfill({response});}catch(_) {}});
 await page.goto('/graph?lang=en');await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).toHaveAttribute('aria-busy','true');
 await page.locator('#graph-expression').fill('x*x');release();await expect(page.locator('#graph-status')).toHaveText('Enter a function and plot it.');await expect(page.locator('#graph-plot')).toBeHidden();
});
test('default view, automatic t, degree angles and resize',async({page})=>{
 await page.goto('/graph?lang=en');await expect(page.locator('.graph-settings')).not.toHaveAttribute('open','');
 await expect(page.locator('#graph-min')).toHaveValue('-10');await expect(page.locator('#graph-max')).toHaveValue('10');
 await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toBeVisible();
 await sample(page,'sin(t)','0','90');await page.locator('#graph-angle').selectOption('deg');await page.locator('#graph-submit').click();
 await expect(page.locator('#graph-status')).toContainText('f(t)');await page.locator('#graph-values summary').click();await expect(page.locator('#graph-table-body tr').last()).toContainText('1E+0');
 await page.locator('.graph-settings summary').click();await page.locator('#graph-values summary').click();
 await page.screenshot({path:'C:/Incoming/Projekty/NumForge/build/adaptive-graph-desktop.png',fullPage:true});
 await page.setViewportSize({width:390,height:844});await expect(page.locator('#graph-plot')).toBeVisible();
 await page.screenshot({path:'C:/Incoming/Projekty/NumForge/build/adaptive-graph-mobile.png',fullPage:true});
});
