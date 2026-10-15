const {test,expect}=require('@playwright/test');
const query=r=>new URL(r.url()).searchParams;
for(const width of [390,1280])test(`all visible function intervals finish before optional buffering at width ${width}`,async({page})=>{
 await page.setViewportSize({width,height:844});await page.goto('/graph?lang=en');const expressions=['sin(33x)','cos(17x)','exp(-x*x/4)','x^3-3x'];await page.locator('#graph-expression').fill(expressions[0]);for(const expression of expressions.slice(1)){await page.locator('#graph-add').click();await page.locator('.graph-curve').last().locator('input:not([type=color])').fill(expression);}
 const calls=[],finished=new Set();page.on('response',r=>{if(r.url().includes('/api/graph/plot?')&&query(r).get('min')==='-10'&&query(r).get('ymin')==='-10')finished.add(r.request().postData());});
 page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))calls.push({body:r.postData(),low:query(r).get('min'),high:query(r).get('max'),ymin:query(r).get('ymin'),finished:finished.size});});
 await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('path.graph-line')).toHaveCount(4);
 expect(calls.slice(0,4).map(r=>r.body)).toEqual(expressions);expect(calls.slice(0,4).every(r=>r.low==='-10'&&r.high==='10'&&r.ymin==='-10')).toBe(true);
 await expect.poll(()=>calls.some(r=>r.low==='-30'&&r.high==='30')).toBe(true);expect(calls.slice(4).every(r=>r.finished===4)).toBe(true);
});
test('a held background buffer cannot delay or replace the visible curve',async({page})=>{
 await page.goto('/graph?lang=en');let release,waiting=false;const held=new Promise(resolve=>release=resolve);
 await page.route('**/api/graph/plot?**',async route=>{const response=await route.fetch();if(query(route.request()).get('min')==='-30'){waiting=true;await held;}try{await route.fulfill({response});}catch(_) {}});
 await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toBeVisible();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect.poll(()=>waiting).toBe(true);
 const original=await page.locator('path.graph-line').getAttribute('d');release();await page.waitForTimeout(300);expect(await page.locator('path.graph-line').getAttribute('d')).toBe(original);
});
test('visible-only dense mixed expression keeps all accepted edges and skips the wide preparation',async({page})=>{
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('sqrt(abs(x))*sin(33x)');const calls=[];let foreground;
 page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))calls.push(query(r));});
 page.on('response',r=>{if(r.url().includes('/api/graph/plot?')&&query(r).get('ymin')==='-10')foreground=r;});
 await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');const data=await foreground.json();expect(data.ok).toBe(true);expect(data.limited).toBe(false);expect(data.points.slice(1).every(p=>p[2]===1)).toBe(true);
 for(let i=0;i<data.points.length;i+=Math.ceil(data.points.length/64)){const p=data.points[i];expect(p[1]).toBeCloseTo(Math.sqrt(Math.abs(p[0]))*Math.sin(33*p[0]),8);}
 await expect(page.locator('path.graph-line')).toHaveCount(1);await page.waitForTimeout(350);expect(calls.every(q=>q.get('min')==='-10'&&q.get('max')==='10')).toBe(true);await expect(page.locator('#graph-status')).not.toContainText('could not be resolved');
});
test('extreme unresolved oscillation is bounded and only accepted strokes are counted',async({page})=>{
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('sin(1000x)+sin(1001x)+sin(x*x)');await page.locator('#graph-add').click();await page.locator('.graph-curve').last().locator('input:not([type=color])').fill('x');let dense;
 page.on('response',r=>{if(r.url().includes('/api/graph/plot?')&&r.request().postData()==='sin(1000x)+sin(1001x)+sin(x*x)')dense=r;});await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');const data=await dense.json();expect(data.count).toBeLessThanOrEqual(8192);expect(data.limited).toBe(true);
 await expect(page.locator('path.graph-line')).toHaveCount(1);await expect(page.locator('#graph-status')).toContainText('1 function plotted');await expect(page.locator('#graph-status')).toContainText('with unresolved detail');
});
test('visible-first sampling and later cached pan preserve reciprocal pole breaks',async({page})=>{
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('1/(x-0.1)^2');await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 await page.locator('#graph-plot').press('ArrowRight');await page.waitForTimeout(350);await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 const data=await page.locator('#graph-plot').evaluate(svg=>({width:svg.viewBox.baseVal.width,low:Number(svg.dataset.xmin),high:Number(svg.dataset.xmax),paths:[...svg.querySelectorAll('path.graph-line')].map(p=>p.getAttribute('d'))}));const pole=64+(.1-data.low)/(data.high-data.low)*(data.width-86);
 for(const path of data.paths){let previous;for(const command of path.matchAll(/([MLC])([^MLC]+)/g)){const numbers=command[2].split(/[ ,]+/).map(Number),x=numbers.at(-2);if(command[1]!=='M')expect(previous<pole&&x>pole).toBe(false);previous=x;}}
});
