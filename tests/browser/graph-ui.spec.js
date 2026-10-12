const {test,expect}=require('@playwright/test');
async function sample(page,expression,count=5) {
 await page.locator('#graph-expression').fill(expression);
 await page.locator('#graph-min').fill('-1');await page.locator('#graph-max').fill('1');
 await page.locator('#graph-count').fill(String(count));
 const response=page.waitForResponse(r=>r.url().includes('/api/graph/sample?'));
 await page.locator('#graph-submit').click();await response;
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
}
for(const [lang,width] of [['sk',390],['en',1280]])test(`graph UI axes, gaps and typed sample table ${lang}`,async({page})=>{
 await page.setViewportSize({width,height:844});await page.goto('/graph?lang='+lang);
 await expect(page.locator('.primary-nav a[aria-current=page]')).toHaveAttribute('href','/graph?lang='+lang);
 await sample(page,'x*x');await expect(page.locator('.graph-point')).toHaveCount(5);
 await expect(page.locator('#graph-plot')).toBeVisible();
 await expect(page.locator('.graph-line')).toHaveCount(0);
 await page.screenshot({path:`C:/Incoming/Projekty/NumForge/build/graph-ui-${lang}.png`,fullPage:true});
 await page.locator('#graph-connect').check();await expect(page.locator('.graph-line')).toHaveCount(4);
 await sample(page,'1/x');await expect(page.locator('.graph-point')).toHaveCount(4);
 await expect(page.locator('.graph-line')).toHaveCount(2);
 await page.locator('#graph-values summary').click();await expect(page.locator('#graph-table-body tr')).toHaveCount(5);
 await expect(page.locator('#graph-table-body tr').nth(2)).toContainText(lang==='en'?'Domain error':'Chyba definičného oboru');
 await sample(page,'sqrt(x)');await expect(page.locator('.graph-point')).toHaveCount(3);
 await expect(page.locator('#graph-table-body tr').first()).toContainText(lang==='en'?'Complex':'Komplexné');
 await sample(page,'qty(x;"m")');await expect(page.locator('#graph-plot')).toBeHidden();
 await expect(page.locator('#graph-table-body tr').first()).toContainText(lang==='en'?'Quantity':'Veličina');
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBe(true);
 await sample(page,'rand()');await expect(page.locator('#graph-status')).toHaveClass(/error/);
 await expect(page.locator('#graph-values')).toBeHidden();
});
test('graph browser numeric boundaries remain finite and do not misplot underflow',async({page})=>{
 await page.goto('/graph?lang=en');
 await sample(page,'10^400');await expect(page.locator('#graph-plot')).toBeHidden();
 await page.locator('#graph-values summary').click();await expect(page.locator('#graph-table-body tr').first()).toContainText('Outside plotting range');
 await sample(page,'10^(-400)');await expect(page.locator('#graph-plot')).toBeHidden();
 await sample(page,'10^308*x');await expect(page.locator('.graph-point')).toHaveCount(5);
 expect(await page.locator('#graph-plot').evaluate(svg=>!/NaN|Infinity/.test(svg.innerHTML))).toBe(true);
 await sample(page,'0');await expect(page.locator('.graph-point')).toHaveCount(5);
});
test('graphs use a read-only calculator snapshot and survive in-place navigation',async({page,request})=>{
 await page.goto('/?lang=en');await page.locator('#expression').fill('a=2');await page.locator('#expression').press('Enter');
 await expect(page.locator('#result')).toHaveText('2');
 const client=await page.evaluate(()=>sessionStorage.getItem('numforge-client-id'));
 const read=async()=>Promise.all(['/api/session','/api/session/variables','/api/session/history'].map(async p=>(await request.get(p+'?client='+client)).json()));
 const before=await read();
 await page.evaluate(()=>window.graphHeader=document.querySelector('.page-header'));
 await page.locator('.primary-nav a[href^="/graph"]').click();
 await expect(page.locator('#graph-form')).toBeVisible();await page.locator('#graph-session').check();
 await sample(page,'a*x');await expect(page.locator('.graph-point')).toHaveCount(5);expect(await read()).toEqual(before);
 await page.locator('.language-switch a[lang=sk]').click();await expect(page.locator('#graph-expression')).toHaveValue('a*x');
 await page.locator('#graph-submit').click();await expect(page.locator('.graph-point')).toHaveCount(5);
 expect(await page.evaluate(()=>window.graphHeader===document.querySelector('.page-header'))).toBe(true);
 await page.locator('.primary-nav a[href^="/?"]').click();await expect(page.locator('#expression')).toHaveValue('a=2');
 await page.goBack();await expect(page.locator('#graph-form')).toBeVisible();
 await page.locator('#graph-submit').click();await expect(page.locator('.graph-point')).toHaveCount(5);
});
test('editing cancels a pending graph and a stale response cannot replace the input',async({page})=>{
 let release;const paused=new Promise(resolve=>release=resolve);
 await page.route('**/api/graph/sample?**',async route=>{const response=await route.fetch();await paused;try{await route.fulfill({response});}catch(_) {}});
 await page.goto('/graph?lang=en');await page.locator('#graph-submit').click();
 await expect(page.locator('.graph-workspace')).toHaveAttribute('aria-busy','true');
 await page.locator('#graph-expression').fill('x*x');release();
 await expect(page.locator('#graph-status')).toHaveText('Enter a function and plot it.');
 await expect(page.locator('#graph-plot')).toBeHidden();
});

test('default sine graph and custom degree parameter use the C sampler',async({page})=>{
 await page.goto('/graph?lang=en');await page.locator('#graph-submit').click();
 await expect(page.locator('.graph-point')).toHaveCount(257);
 await page.locator('#graph-expression').fill('sin(t)');await page.locator('#graph-parameter').fill('t');
 await page.locator('#graph-min').fill('0');await page.locator('#graph-max').fill('90');
 await page.locator('#graph-count').fill('3');await page.locator('#graph-angle').selectOption('deg');
 await page.locator('#graph-submit').click();await expect(page.locator('.graph-point')).toHaveCount(3);
 await page.locator('#graph-values summary').click();
 await expect(page.locator('#graph-table-body tr').last()).toContainText('1E+0');
 await page.setViewportSize({width:390,height:844});await expect(page.locator('#graph-plot')).toBeVisible();
 expect(await page.locator('#graph-plot').evaluate(svg=>Number(svg.getAttribute('viewBox').split(' ')[2]))).toBeLessThan(600);
});
