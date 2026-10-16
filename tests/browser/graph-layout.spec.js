const {test,expect}=require('@playwright/test');
for(const [width,height,lang] of [[1280,800,'en'],[390,844,'sk'],[844,390,'en'],[320,568,'sk']]){
 test(`library fits without scrolling and every function remains reachable ${width}x${height}`,async({page})=>{
  await page.setViewportSize({width,height});await page.goto('/graph?lang='+lang);
  const before=await page.evaluate(()=>document.documentElement.scrollHeight);
  await page.locator('#graph-library summary').click();const dialog=page.locator('#graph-library-dialog');await expect(dialog).toBeVisible();
  await expect(page.locator('#graph-functions button').first()).toBeVisible();
  const visited=new Set(),total=await page.locator('#graph-functions button').count();
  for(let i=0;i<40;i++){
   const fit=await dialog.evaluate(e=>{const r=e.getBoundingClientRect();return {top:r.top,bottom:r.bottom,height:innerHeight,overflow:e.scrollHeight-e.clientHeight};});
   expect(fit.top).toBeGreaterThanOrEqual(0);expect(fit.bottom).toBeLessThanOrEqual(fit.height);expect(fit.overflow).toBeLessThanOrEqual(1);
   const visible=await page.locator('#graph-functions button:visible').evaluateAll(nodes=>nodes.map(e=>e.dataset.insert));expect(visible.length).toBeLessThanOrEqual(height<=420?3:height<=560?6:9);visible.forEach(v=>visited.add(v));
   if(await page.locator('#graph-library-next').isDisabled())break;await page.locator('#graph-library-next').click();
  }
  expect(visited.size).toBe(total);expect(await page.evaluate(()=>document.documentElement.scrollHeight)).toBeLessThanOrEqual(before+1);
  await page.locator('#function-search').fill('LUDOLPH');await expect(page.locator('#graph-functions button[data-insert="π"]')).toBeVisible();
  await dialog.screenshot({path:`C:/Incoming/Projekty/NumForge/build/graph-library-compact-${width}.png`});
  await page.locator('#graph-library-close').click();await expect(dialog).not.toBeVisible();await expect(page.locator('#graph-expression')).toBeFocused();await expect(page.locator('body')).not.toHaveClass(/graph-library-open/);
 });
 test(`expanded graph spends its height on the canvas ${width}x${height}`,async({page})=>{
  await page.setViewportSize({width,height});await page.goto('/graph?lang='+lang);await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toBeVisible();
  await page.locator('#graph-expand').click();const dialog=page.locator('#graph-expanded');await expect(dialog).toBeVisible();
  await expect(page.locator('#graph-interaction-hint')).not.toBeVisible();await expect(page.locator('#graph-status')).not.toBeVisible();
  const geometry=await dialog.evaluate(d=>{const plot=d.querySelector('#graph-plot').getBoundingClientRect(),r=d.getBoundingClientRect(),result=d.querySelector('.graph-result');return {fraction:plot.height/r.height,bottom:plot.bottom,limit:r.bottom,overflow:result.scrollHeight-result.clientHeight};});
  expect(geometry.fraction).toBeGreaterThan(.72);expect(geometry.bottom).toBeLessThanOrEqual(geometry.limit);expect(geometry.overflow).toBeLessThanOrEqual(1);
  await expect(page.locator('path.graph-line')).toBeVisible();await dialog.screenshot({path:`C:/Incoming/Projekty/NumForge/build/graph-expanded-tall-${width}.png`});
  await page.locator('#graph-reset').click();await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');await page.keyboard.press('Escape');await expect(dialog).not.toBeVisible();
 });
}
test('expanded plot preserves important errors instead of hiding every status',async({page})=>{
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('x+y');await page.locator('#graph-submit').click();await expect(page.locator('#graph-status')).toHaveClass(/error/);
 await page.locator('#graph-expand').click();await expect(page.locator('#graph-status')).toBeVisible();await expect(page.locator('#graph-status')).toContainText('one variable');
});
for(const width of [390,1280])test(`library edits the selected complete expression while remaining open at ${width}`,async({page})=>{
 await page.setViewportSize({width,height:900});await page.goto('/graph?lang=en');await page.locator('#graph-add').click();
 const target=page.locator('.graph-curve').last().locator('input:not([type=color])');await target.fill('');await page.locator('#graph-library summary').click();
 const editor=page.locator('#graph-library-expression');await expect(editor).toBeFocused();await expect(page.locator('#graph-library-target')).toContainText('2');
 await editor.fill('x + ');await page.locator('#graph-library-next').click();await expect(editor).toHaveValue('x + ');
 await page.locator('#function-search').fill('square root');await page.locator('#graph-functions button[data-function=sqrt]').click();await expect(editor).toBeFocused();await editor.press('2');
 await expect(editor).toHaveValue('x + sqrt(2)');await expect(target).toHaveValue('x + sqrt(2)');await expect(page.locator('#graph-expression')).toHaveValue('sin(x)');
 await editor.evaluate(e=>e.setSelectionRange(0,1));await page.locator('#function-search').fill('sine');await page.locator('#graph-functions button[data-function=sin]').click();
 await expect(editor).toHaveValue('sin(x) + sqrt(2)');await editor.press('End');await editor.press('+');await editor.press('3');
 await page.locator('#graph-library-dialog').screenshot({path:`C:/Incoming/Projekty/NumForge/build/graph-library-editor-${width}.png`});
 await editor.press('Enter');await expect(page.locator('#graph-library-dialog')).not.toBeVisible();await expect(target).toHaveValue('sin(x) + sqrt(2)+3');await expect(page.locator('path.graph-line')).toHaveCount(2);
});
