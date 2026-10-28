const {test,expect}=require('@playwright/test');

for(const lang of ['sk','en'])test(`equation graph keeps existing functions and verifies the root ${lang}`,async({page})=>{
 await page.goto('/graph?lang='+lang);
 await page.locator('#graph-expression').fill('sin(x)');
 await page.locator('.primary-nav a[href="/solve?lang='+lang+'"]').click();
 await page.locator('#equation-expression').fill('x+2=7');await page.locator('#equation-submit').click();
 await page.locator('.equation-graph-link').click();
 await expect(page).toHaveURL('/graph?lang='+lang);
 const expressions=page.locator('.graph-curve input:not([type=color])');
 await expect(expressions).toHaveCount(2);await expect(expressions.nth(0)).toHaveValue('sin(x)');
 await expect(expressions.nth(1)).toHaveValue('(x+2)-(7)');
 await expect(page.locator('.graph-curve.active input:not([type=color])')).toHaveValue('(x+2)-(7)');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-5');
 await expect(page.locator('.graph-feature[data-kind="x-axis"][data-curve="2"][data-verified="true"]')).toHaveCount(1);
 expect(Number(await page.locator('.graph-feature[data-kind="x-axis"][data-curve="2"]').getAttribute('data-x'))).toBeCloseTo(5,8);
 await page.locator('.primary-nav a[href="/solve?lang='+lang+'"]').click();
 await page.locator('.equation-graph-link').click();await expect(expressions).toHaveCount(2);
 await page.reload();await expect(expressions).toHaveCount(2);
 await expect(expressions.nth(1)).toHaveValue('(x+2)-(7)');
});

test('coefficient graph preserves grouping and complex roots have no graph action',async({page})=>{
 await page.goto('/solve?lang=en');await page.locator('#equation-mode').selectOption('coefficients');
 for(const [name,value] of Object.entries({a:'0',b:'2+1',c:'-15'}))await page.locator('#equation-'+name).fill(value);
 await page.locator('#equation-submit').click();await page.locator('.equation-graph-link').click();
 await expect(page.locator('#graph-expression')).toHaveValue('(0)*x^2+(2+1)*x+(-15)');
 await expect(page.locator('.graph-feature[data-kind="x-axis"]')).toHaveCount(1);
 await page.locator('.primary-nav a[href="/solve?lang=en"]').click();
 for(const [name,value] of Object.entries({a:'1',b:'0',c:'1'}))await page.locator('#equation-'+name).fill(value);
 await page.locator('#equation-submit').click();await expect(page.locator('.equation-root')).toHaveCount(2);
 await expect(page.locator('.equation-graph-link')).toHaveCount(0);
 await page.locator('#equation-c').fill('-1');await expect(page.locator('.equation-graph-link')).toHaveCount(0);
});

test('numeric handoff uses checked x and retains original equation sides',async({page})=>{
 await page.goto('/solve?lang=en');await page.locator('#equation-mode').selectOption('numeric');
 await page.locator('#equation-expression').fill('cos(x)=x');await page.locator('#equation-max').fill('1');
 const response=page.waitForResponse(r=>r.url().includes('/api/equations/solve?'));
 await page.locator('#equation-submit').click();const root=(await (await response).json()).roots[0];
 await page.locator('.equation-graph-link').click();
 await expect(page.locator('#graph-expression')).toHaveValue('(cos(x))-(x)');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin',String(Number(root.x)-10));
 await expect(page.locator('.graph-feature[data-kind="x-axis"][data-verified="true"]')).toHaveCount(1);
 expect(Number(await page.locator('.graph-feature[data-kind="x-axis"]').getAttribute('data-x'))).toBeCloseTo(Number(root.x),7);
});

test('graph capacity blocks handoff without deleting existing functions',async({page})=>{
 await page.goto('/solve?lang=en');
 await page.evaluate(()=>sessionStorage.setItem('numforge-graph-state',JSON.stringify({version:2,curves:['x','x+1','x+2','x+3'].map(expression=>({expression,color:'#20c997'}))})));
 await page.locator('#equation-submit').click();await page.locator('.equation-graph-link').first().click();
 await expect(page).toHaveURL('/solve?lang=en');await expect(page.locator('#equation-error')).toContainText('four functions');
 expect(await page.evaluate(()=>JSON.parse(sessionStorage.getItem('numforge-graph-state')).curves.map(c=>c.expression))).toEqual(['x','x+1','x+2','x+3']);
});

test('one-digit preview only positions a wide camera and never publishes a fake intercept',async({page})=>{
 await page.goto('/solve?lang=en');await page.locator('#equation-expression').fill('x=149');
 await page.route('**/api/equations/solve?**',route=>{const url=new URL(route.request().url());url.searchParams.set('precision','1');return route.continue({url:url.href});});
 await page.locator('#equation-submit').click();await page.locator('.equation-graph-link').click();
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','40');
 await expect(page.locator('.graph-feature[data-kind="x-axis"][data-verified="true"]')).toHaveCount(1);
 expect(Number(await page.locator('.graph-feature[data-kind="x-axis"]').getAttribute('data-x'))).toBeCloseTo(149,6);
});

test('unavailable storage reports a failed handoff and keeps the solution',async({page})=>{
 await page.goto('/solve?lang=en');await page.locator('#equation-submit').click();
 await page.evaluate(()=>{Storage.prototype.setItem=()=>{throw new Error('disabled');};});
 await page.locator('.equation-graph-link').first().click();
 await expect(page).toHaveURL('/solve?lang=en');await expect(page.locator('#equation-error')).toContainText('browser storage');
 await expect(page.locator('.equation-root')).toHaveCount(2);
});

test('long previews retain graph actions while unrepresentable roots do not',async({page})=>{
 await page.route('**/api/equations/solve?**',route=>{const url=new URL(route.request().url());url.searchParams.set('precision','200');return route.continue({url:url.href});});
 await page.goto('/solve?lang=en');await page.locator('#equation-submit').click();
 await expect(page.locator('.equation-graph-link')).toHaveCount(2);
 await page.locator('#equation-expression').fill('x=1E400');await page.locator('#equation-submit').click();
 await expect(page.locator('.equation-root')).toHaveCount(1);await expect(page.locator('.equation-graph-link')).toHaveCount(0);
 await page.locator('#equation-expression').fill('x=1E-400');await page.locator('#equation-submit').click();
 await expect(page.locator('.equation-root')).toHaveCount(1);await expect(page.locator('.equation-graph-link')).toHaveCount(0);
});

test('malformed graph transfer cannot replace saved functions or camera',async({page})=>{
 await page.goto('/solve?lang=en');
 await page.evaluate(()=>{
  sessionStorage.setItem('numforge-graph-state',JSON.stringify({version:2,curves:[{expression:'x+1',color:'#20c997'}],view:{x:[-10,10],y:[-10,10]}}));
  sessionStorage.setItem('numforge-equation-graph',JSON.stringify({expression:'x-999',x:[1,1]}));
 });
 await page.locator('.primary-nav a[href="/graph?lang=en"]').click();
 await expect(page.locator('#graph-expression')).toHaveValue('x+1');await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');
 expect(await page.evaluate(()=>sessionStorage.getItem('numforge-equation-graph'))).toBeNull();
});
