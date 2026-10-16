const {test,expect}=require('@playwright/test');
async function plot(page,expressions,lang='en'){
 await page.goto('/graph?lang='+lang);await page.locator('#graph-expression').fill(expressions[0]);
 for(const expression of expressions.slice(1)){await page.locator('#graph-add').click();await page.locator('.graph-curve').last().locator('input:not([type=color])').fill(expression);}
 await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('path.graph-line').first()).toBeVisible();await expect(page.locator('#graph-plot')).toHaveAttribute('data-features-state',/ready|limited/);
}
async function markers(page,kind){return page.locator(`.graph-feature[data-kind="${kind}"]`).evaluateAll(nodes=>nodes.map(e=>({x:Number(e.dataset.x),y:Number(e.dataset.y),curve:e.dataset.curve,other:e.dataset.other})));}
function has(points,x,y,tolerance=1e-5){return points.some(p=>Math.abs(p.x-x)<tolerance&&Math.abs(p.y-y)<tolerance);}
async function screenPoint(marker){return marker.evaluate(e=>{const p=new DOMPoint(Number(e.getAttribute('cx')),Number(e.getAttribute('cy'))).matrixTransform(e.getScreenCTM());return {x:p.x,y:p.y};});}
for(const lang of ['en','sk'])test(`axis intercept markers snap to coordinates without requests ${lang}`,async({page})=>{
 await plot(page,['x-5'],lang);expect(has(await markers(page,'x-axis'),5,0)).toBe(true);expect(has(await markers(page,'y-axis'),0,-5)).toBe(true);
 const marker=page.locator('.graph-feature[data-kind="x-axis"]').first(),p=await screenPoint(marker);await page.waitForTimeout(350);let requests=0;page.on('request',r=>{if(r.url().includes('/api/graph/plot'))requests++;});
 await page.mouse.move(p.x+2,p.y+2);const inspector=page.locator('#graph-inspector');await expect(inspector).toBeVisible();await expect(inspector).toHaveAttribute('data-feature','x-axis');
 expect(Number(await inspector.getAttribute('data-x'))).toBeCloseTo(5,6);expect(Number(await inspector.getAttribute('data-y'))).toBe(0);await expect(inspector).toContainText(lang==='en'?'x-axis intercept':'Priesečník s osou x');
 await page.waitForTimeout(150);expect(requests).toBe(0);
});
test('pair intersections include crossings and tangent contacts but omit coincident curves',async({page})=>{
 await plot(page,['x','2-x']);expect(has(await markers(page,'intersection'),1,1)).toBe(true);
 await page.locator('#graph-expression').fill('x*x');await page.locator('.graph-curve').nth(1).locator('input:not([type=color])').fill('0');await page.locator('#graph-submit').click();
 await expect.poll(async()=>has(await markers(page,'intersection'),0,0)).toBe(true);
 await page.locator('.graph-curve').nth(1).locator('input:not([type=color])').fill('x*x');await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('.graph-feature[data-kind="intersection"]')).toHaveCount(0);
});
test('all visible sine zeros are marked and undefined poles never become zeros',async({page})=>{
 await plot(page,['sin(x)']);const roots=await markers(page,'x-axis');for(let n=-3;n<=3;n++)expect(has(roots,n*Math.PI,0,.002)).toBe(true);
 await page.locator('#graph-expression').fill('1/x');await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('.graph-feature')).toHaveCount(0);
});
test('hiding and editing a curve clears its intersections and expanded markers keep inspection',async({page})=>{
 await plot(page,['x','2-x']);await page.locator('#graph-expand').click();const marker=page.locator('.graph-feature[data-kind="intersection"]').first();await expect(marker).toBeVisible();await expect.poll(async()=>{const p=await screenPoint(marker);await page.mouse.move(p.x,p.y);return page.locator('#graph-inspector').getAttribute('data-feature');}).toBe('intersection');await expect(page.locator('#graph-inspector-name')).toContainText('1 ∩ 2');
 await page.keyboard.press('Escape');await page.locator('.graph-curve').nth(1).locator('.graph-visibility').click();await expect(page.locator('.graph-feature[data-kind="intersection"]')).toHaveCount(0);
 await page.locator('#graph-expression').fill('x+9');await expect(page.locator('.graph-feature')).toHaveCount(0);
});
test('touching an intersection pins the marker coordinates',async({browser,baseURL})=>{
 const context=await browser.newContext({baseURL,hasTouch:true,viewport:{width:390,height:844}}),page=await context.newPage();
 try{await plot(page,['x','2-x']);await page.locator('#graph-plot').scrollIntoViewIfNeeded();const p=await screenPoint(page.locator('.graph-feature[data-kind="intersection"]').first());await page.touchscreen.tap(p.x,p.y);await expect(page.locator('#graph-inspector')).toHaveAttribute('data-feature','intersection');expect(Number(await page.locator('#graph-inspector').getAttribute('data-x'))).toBeCloseTo(1,5);}finally{await context.close();}
});
test('find function centers on the selected y intercept and reset retains the default camera',async({page})=>{
 await plot(page,['x','100+cos(t)']);await page.locator('#graph-locate').click();
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymin','91');await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymax','111');await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');
 await expect.poll(async()=>has(await markers(page,'y-axis'),0,101)).toBe(true);
 await page.locator('#graph-reset').click();await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymin','-10');await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymax','10');
});
test('find function works before plotting with an inferred variable and explains missing y intercepts',async({page})=>{
 await page.goto('/graph?lang=sk');await page.locator('#graph-expression').fill('t+1000');await page.locator('#graph-locate').click();await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymin','990');
 await page.locator('#graph-expression').fill('1/t');await page.locator('#graph-locate').click();await expect(page.locator('#graph-status')).toContainText('nemá pri x = 0');await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymin','-10');
 await expect(page.locator('#graph-locate')).toBeEnabled();
});
test('positive curves and dense envelopes do not invent axis zeros',async({page})=>{
 await plot(page,['x*x+0.000000000001','exp(-x)','abs(x-0.1234)+0.000000000001']);await expect(page.locator('.graph-feature[data-kind="x-axis"]')).toHaveCount(0);
});
for(const [expression,roots] of [['x*x-4',[-2,2]],['(x-0.1234)^2',[.1234]],['abs(x-0.1234)',[.1234]],['sqrt(x)',[0]]])test(`represented zeros and contacts in ${expression}`,async({page})=>{
 await plot(page,[expression]);const points=await markers(page,'x-axis');for(const x of roots)expect(has(points,x,0,.002)).toBe(true);
});
test('two oscillating curves retain each visible intersection',async({page})=>{
 await plot(page,['sin(x)','cos(x)']);const points=await markers(page,'intersection');for(let n=-3;n<=2;n++){const x=Math.PI/4+n*Math.PI;expect(has(points,x,Math.sin(x),.002)).toBe(true);}
});
test('editing while a y-intercept lookup is pending cannot move a newer camera',async({page})=>{
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('t+1000');
 let release,started;const held=new Promise(r=>release=r),sent=new Promise(r=>started=r);
 await page.route('**/api/graph/plot?**',async route=>{const response=await route.fetch();started();await held;try{await route.fulfill({response});}catch(_) {}});
 await page.locator('#graph-locate').click();await sent;await page.locator('#graph-expression').fill('x');release();
 await expect(page.locator('#graph-locate')).toBeEnabled();await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymin','-10');await page.waitForTimeout(150);await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymax','10');
});
test('C verification refines geometry against the original expression and rejects false roots',async({page})=>{
 const response=await page.request.post('/api/graph/verify',{data:'sin(x)\n\n0 3.1415 3.1 3.2 0.00001 0.000001\n'}),body=await response.json();expect(body.ok).toBe(true);expect(body.points[0].x).toBeCloseTo(Math.PI,9);expect(body.points[0].residual).toBeLessThan(1e-10);
 for(const expression of ['1/x','floor(x)-0.5','(x-0.1234)^2+0.000000000001']){
  const x=expression.startsWith('floor')?1:expression.startsWith('1/')?.001:.1234,low=expression.startsWith('floor')?.9:expression.startsWith('1/')?-.1:.12,high=expression.startsWith('floor')?1.1:expression.startsWith('1/')?.1:.13;
  const checked=await (await page.request.post('/api/graph/verify',{data:expression+'\n\n0 '+[x,low,high,.00001,.000001].join(' ')+'\n'})).json();expect(checked.ok).toBe(true);expect(checked.points).toEqual([null]);
 }
});
test('dense marker selection covers every curve and both sides of the view',async({page})=>{
 await plot(page,['sin(33*x)','cos(31*x)','sin(17*x)','cos(13*x)']);const points=await page.locator('.graph-feature').evaluateAll(nodes=>nodes.map(e=>({kind:e.dataset.kind,x:Number(e.dataset.x),curve:e.dataset.curve,verified:e.dataset.verified})));
 expect(points.length).toBeLessThanOrEqual(128);expect(new Set(points.map(p=>p.curve)).size).toBe(4);expect(points.every(p=>p.verified==='true')).toBe(true);
 for(const curve of ['1','2','3','4']){const roots=points.filter(p=>p.curve===curve&&p.kind==='x-axis');expect(roots.some(p=>p.x<-6)).toBe(true);expect(roots.some(p=>p.x>6)).toBe(true);}
});
test('verification failures leave the plot usable without displaying unchecked markers',async({page})=>{
 await page.route('**/api/graph/verify',route=>route.fulfill({status:503,contentType:'application/json',body:'{"ok":false}'}));
 await plot(page,['x-5']);await expect(page.locator('path.graph-line')).toBeVisible();await expect(page.locator('.graph-feature')).toHaveCount(0);await expect(page.locator('#graph-status')).toContainText('could not be verified');
});
test('late verification responses cannot restore markers after editing the expression',async({page})=>{
 let release,started;const held=new Promise(r=>release=r),sent=new Promise(r=>started=r);
 await page.route('**/api/graph/verify',async route=>{const response=await route.fetch();started();await held;try{await route.fulfill({response});}catch(_) {}});
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('x-5');await page.locator('#graph-submit').click();await sent;
 await page.locator('#graph-expression').fill('x+2');release();await expect(page.locator('.graph-feature')).toHaveCount(0);await page.waitForTimeout(200);await expect(page.locator('.graph-feature')).toHaveCount(0);await expect(page.locator('path.graph-line')).toHaveCount(0);
});
test('confirmed markers survive color and same-camera tool navigation without new verification',async({page})=>{
 await plot(page,['x-5']);await expect(page.locator('.graph-feature')).toHaveCount(2);let calls=0;page.on('request',r=>{if(r.url().includes('/api/graph/verify'))calls++;});
 await page.locator('#graph-color').evaluate(e=>{e.value='#20c997';e.dispatchEvent(new Event('input',{bubbles:true}));});await expect(page.locator('.graph-feature').first()).toHaveCSS('fill','rgb(32, 201, 151)');
 await page.locator('.primary-nav a[href="/?lang=en"]').click();await expect(page).toHaveURL(url=>url.pathname==='/');await expect(page.locator('#expression')).toBeVisible();
 await page.locator('.primary-nav a[href="/graph?lang=en"]').click();await expect(page).toHaveURL(url=>url.pathname==='/graph');await expect(page.locator('.graph-feature')).toHaveCount(2);await expect(page.locator('#graph-plot')).toHaveAttribute('data-features-state','ready');await page.waitForTimeout(200);expect(calls).toBe(0);
});
