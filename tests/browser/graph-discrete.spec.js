const {test,expect}=require('@playwright/test');
async function plot(page,expression,mode='auto'){
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill(expression);await page.locator('.graph-render').selectOption(mode);await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
}
test('factorial automatically draws isolated exact integer values and inspects their coordinates',async({page})=>{
 await plot(page,'x!');const dots=page.locator('.graph-discrete-point');await expect(dots).toHaveCount(4);await expect(page.locator('path.graph-line')).toHaveCount(0);
 const values=await dots.evaluateAll(nodes=>nodes.map(e=>[Number(e.dataset.x),Number(e.dataset.y)]));expect(values).toEqual([[0,1],[1,1],[2,2],[3,6]]);
 const point=await dots.nth(2).evaluate(e=>{const p=new DOMPoint(Number(e.getAttribute('cx')),Number(e.getAttribute('cy'))).matrixTransform(e.getScreenCTM());return{x:p.x,y:p.y};});await page.mouse.move(point.x,point.y);
 await expect(page.locator('#graph-inspector')).toBeVisible();await expect(page.locator('#graph-inspector')).toHaveAttribute('data-x','2');await expect(page.locator('#graph-inspector')).toHaveAttribute('data-y','2');
});
test('explicit points mode uses an integer lattice and switching back restores the curve',async({page})=>{
 await plot(page,'sin(t)','points');await expect(page.locator('.graph-discrete-point')).toHaveCount(21);await expect(page.locator('path.graph-line')).toHaveCount(0);
 await page.locator('.graph-render').selectOption('curve');await expect(page.locator('.graph-discrete-point')).toHaveCount(0);await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toBeVisible();
});
test('ordinary curves stay curves in auto and a single integer point is a valid response',async({page})=>{
 await plot(page,'sin(x)');await expect(page.locator('path.graph-line')).toBeVisible();await expect(page.locator('.graph-discrete-point')).toHaveCount(0);
 const response=await page.request.post('/api/graph/plot?mode=fast&render=points&min=0.2&max=1.2',{data:'x*x'}),body=await response.json();expect(body.ok).toBe(true);expect(body.count).toBe(1);expect(body.points[0].slice(0,3)).toEqual([1,1,0]);
});
test('points mode survives tool navigation and reload without becoming a curve',async({page})=>{
 await plot(page,'x*x','points');await expect(page.locator('.graph-discrete-point').first()).toBeVisible();await page.reload();await expect(page.locator('.graph-render')).toHaveValue('points');await expect(page.locator('.graph-discrete-point').first()).toBeVisible();await expect(page.locator('path.graph-line')).toHaveCount(0);
});
test('discrete values intersect ordinary curves only at their actual x coordinates',async({page})=>{
 await plot(page,'x!');await page.locator('#graph-add').click();await page.locator('.graph-curve').last().locator('input:not([type=color])').fill('x');await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('#graph-plot')).toHaveAttribute('data-features-state',/ready|limited/);
 const values=await page.locator('.graph-feature[data-kind="intersection"]').evaluateAll(nodes=>nodes.map(e=>[Number(e.dataset.x),Number(e.dataset.y)]));expect(values).toEqual(expect.arrayContaining([[1,1],[2,2]]));expect(values.every(([x])=>Number.isInteger(x))).toBe(true);
});
test('a view between integer positions stays empty without an invalid-response error',async({page})=>{
 await page.addInitScript(()=>sessionStorage.setItem('numforge-graph-state',JSON.stringify({version:2,view:{x:[.1,.9],y:[-10,10]},curves:[{expression:'x',render:'points',visible:true,plotted:true}]})));
 await page.goto('/graph?lang=en');await expect(page.locator('#graph-status')).toContainText('No integer x');await expect(page.locator('#graph-status')).not.toHaveClass(/error/);await expect(page.locator('.graph-discrete-point')).toHaveCount(0);
});
