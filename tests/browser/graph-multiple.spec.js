const {test,expect}=require('@playwright/test');
const rows=page=>page.locator('.graph-curve');
const input=(page,i)=>rows(page).nth(i).locator('input:not([type=color])');
const color=(page,i)=>rows(page).nth(i).locator('input[type=color]');
async function add(page,value){await page.locator('#graph-add').click();await input(page,await rows(page).count()-1).fill(value);}
async function plot(page){await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');}
for(const [lang,width] of [['en',1280],['sk',390]])test(`multiple functions have independent colors, results and a shared camera ${lang}`,async({page})=>{
 await page.setViewportSize({width,height:900});await page.goto('/graph?lang='+lang);
 await add(page,'cos(x)');await add(page,'x*x');
 const colors=await rows(page).locator('input[type=color]').evaluateAll(nodes=>nodes.map(n=>n.value));expect(new Set(colors).size).toBe(3);
 const prepared=new Set();page.on('response',r=>{const q=new URL(r.url()).searchParams;if(r.url().includes('/api/graph/plot?')&&q.get('min')==='-10'&&q.get('ymin')==='-5')prepared.add(r.request().postData());});
 await plot(page);await expect(page.locator('path.graph-line')).toHaveCount(3);
 expect(new Set(await page.locator('path.graph-line').evaluateAll(nodes=>nodes.map(n=>n.dataset.curve))).size).toBe(3);
 await expect(page.locator('#graph-legend li')).toHaveCount(3);await expect(page.locator('#graph-legend')).toContainText('cos(x)');
 await expect.poll(()=>prepared.size).toBe(3);
 let calls=0;page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))++calls;});
 await color(page,1).evaluate(e=>{e.value='#ff2233';e.dispatchEvent(new Event('input',{bubbles:true}));});
 const id=await rows(page).nth(1).getAttribute('data-curve');await expect(page.locator(`path.graph-line[data-curve="${id}"]`)).toHaveCSS('stroke','rgb(255, 34, 51)');
 await page.waitForTimeout(250);expect(calls).toBe(0);
 await page.locator('#graph-plot').press('ArrowUp');await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymin','-6');
 await input(page,1).fill('cos(2x)');await expect(page.locator('path.graph-line')).toHaveCount(2);await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymin','-6');
 await expect(page.locator('#graph-zoom-in,#graph-zoom-out')).toHaveCount(0);
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBe(true);
 await page.locator('.graph-workspace').screenshot({path:`C:/Incoming/Projekty/NumForge/build/graph-multiple-${lang}.png`});
});
test('one failed function does not hide valid curves and the four-row limit is enforced',async({page})=>{
 await page.goto('/graph?lang=en');await add(page,'x+y');await add(page,'cos(t)');await add(page,'x*x');
 await expect(page.locator('#graph-add')).toBeDisabled();await plot(page);
 await expect(page.locator('path.graph-line')).toHaveCount(3);await expect(rows(page).nth(1).locator('.graph-curve-status')).toHaveClass(/error/);
 await expect(page.locator('#graph-status')).toContainText('3 functions plotted');
 await rows(page).nth(1).locator('.graph-remove').click();await expect(rows(page)).toHaveCount(3);await expect(page.locator('#graph-add')).toBeEnabled();
 await expect(page.locator('path.graph-line')).toHaveCount(3);
});
test('deleting a curve cancels its result and does not attach it to a new row',async({page})=>{
 await page.goto('/graph?lang=en');await add(page,'cos(x)');await add(page,'x*x');
 let release;const held=new Promise(r=>release=r);
 await page.route('**/api/graph/plot?**',async route=>{const response=await route.fetch();if(route.request().postData()==='cos(x)')await held;try{await route.fulfill({response});}catch(_) {}});
 await page.locator('#graph-submit').click();await expect(rows(page).nth(1).locator('.graph-curve-status')).toContainText('Sampling');
 const deleted=await rows(page).nth(1).getAttribute('data-curve');await rows(page).nth(1).locator('.graph-remove').click();await add(page,'x+1');release();
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator(`path[data-curve="${deleted}"]`)).toHaveCount(0);
 expect(await input(page,2).inputValue()).toBe('x+1');await expect(page.locator('path.graph-line')).toHaveCount(2);
});
for(const lang of ['en','sk'])test(`graph library searches shared aliases and inserts into the selected row ${lang}`,async({page})=>{
 await page.goto('/graph?lang='+lang);await add(page,'');await page.locator('#graph-library summary').click();
 const search=page.locator('#function-search');await search.fill(lang==='sk'?'KVADRATICKA ODMOCNINA':'SQUARE ROOT');
 await page.locator('#graph-functions button[data-function=sqrt]').click();await expect(input(page,1)).toHaveValue('sqrt()');await expect(input(page,0)).toHaveValue('sin(x)');
 await search.fill(lang==='sk'?'LUDOLFOVO CISLO':"LUDOLPH'S NUMBER");await page.locator('#graph-functions button[data-insert="π"]').click();await expect(input(page,1)).toHaveValue('sqrt(π)');
 await search.fill('ARCTG');await expect(page.locator('#graph-functions button[data-function=atan]')).toBeVisible();
 await search.fill('');await expect(page.locator('#graph-functions button[data-function=rand]')).toHaveCount(0);
 await page.locator('#graph-function-category').selectOption('2');await expect(page.locator('#graph-functions button[data-function=sin]')).toBeVisible();await expect(page.locator('#graph-functions button[data-function=sqrt]')).toBeHidden();
 await page.locator('#graph-library').screenshot({path:`C:/Incoming/Projekty/NumForge/build/graph-library-${lang}.png`});
});
for(const [width,height] of [[1280,900],[390,844],[844,390]])test(`expanded graph keeps the canvas and camera and returns cleanly ${width}x${height}`,async({page})=>{
 await page.setViewportSize({width,height});await page.goto('/graph?lang=en');await add(page,'cos(x)');await plot(page);
 const svg=page.locator('#graph-plot');await svg.press('ArrowRight');await expect(svg).toHaveAttribute('data-xmin','-6');
 await page.locator('#graph-expand').scrollIntoViewIfNeeded();const beforeScroll=await page.evaluate(()=>({x:scrollX,y:scrollY}));await page.evaluate(()=>window.graphCanvas=document.querySelector('#graph-plot'));await page.locator('#graph-expand').click();
 await expect(page.locator('#graph-expanded')).toHaveAttribute('open','');await expect(page.locator('#graph-expand')).toHaveAttribute('aria-expanded','true');
 await expect(svg).toHaveAttribute('data-xmin','-6');expect(await page.evaluate(()=>window.graphCanvas===document.querySelector('#graph-plot'))).toBe(true);
 const rect=await page.locator('#graph-expanded').boundingBox();expect(rect.width).toBeGreaterThan(width*.9);expect(rect.height).toBeGreaterThan(height*.9);
 await expect(page.locator('#graph-expanded path.graph-line')).toHaveCount(2);
 await svg.press('ArrowUp');await expect(svg).toHaveAttribute('data-ymin','-6');
 const canvasRect=await svg.boundingBox();await svg.dispatchEvent('wheel',{deltaY:-100,clientX:canvasRect.x+canvasRect.width/2,clientY:canvasRect.y+canvasRect.height/2});
 const camera=await svg.evaluate(e=>({xmin:e.dataset.xmin,ymin:e.dataset.ymin}));
 expect(Number(await svg.getAttribute('data-xmax'))-Number(camera.xmin)).toBeLessThan(20);
 expect(await svg.evaluate(e=>!/NaN|Infinity/.test(e.innerHTML))).toBe(true);
 await page.locator('#graph-expanded').screenshot({path:`C:/Incoming/Projekty/NumForge/build/graph-expanded-${width}.png`});
 await page.keyboard.press('Escape');await expect(page.locator('#graph-expanded')).not.toHaveAttribute('open','');await expect(page.locator('body')).not.toHaveClass(/graph-expanded-open/);
 await expect(svg).toHaveAttribute('data-xmin',camera.xmin);await expect(svg).toHaveAttribute('data-ymin',camera.ymin);await expect(page.locator('#graph-expand')).toBeFocused();expect(await page.evaluate(()=>({x:scrollX,y:scrollY}))).toEqual(beforeScroll);
 await page.locator('#graph-expand').click();await page.locator('#graph-expand').click();await expect(page.locator('#graph-expanded')).not.toHaveAttribute('open','');
 await page.locator('#graph-reset').click();await expect(svg).toHaveAttribute('data-xmin','-10');await expect(svg).toHaveAttribute('data-ymin','-10');
});
test('curve rows persist across language navigation and the shared library loads through SPA routes',async({page})=>{
 await page.goto('/units?lang=en');await page.locator('.primary-nav a[href^="/graph"]').click();await add(page,'cos(t)');
 await color(page,1).evaluate(e=>{e.value='#ff2233';e.dispatchEvent(new Event('input',{bubbles:true}));});
 await page.locator('.language-switch a[lang=sk]').click();await expect(page).toHaveURL(/lang=sk/);await expect(rows(page)).toHaveCount(2);await expect(input(page,1)).toHaveValue('cos(t)');await expect(color(page,1)).toHaveValue('#ff2233');
 await page.reload();await expect(rows(page)).toHaveCount(2);await expect(input(page,1)).toHaveValue('cos(t)');
 await page.locator('.primary-nav a[href="/?lang=sk"]').click();await page.locator('#function-search').fill('KVADRATICKA ODMOCNINA');await expect(page.locator('button[data-function=sqrt]')).toBeVisible();
 await page.locator('#expression').fill('2+2');await page.locator('#expression').press('Enter');await expect(page.locator('#result')).toHaveText('4');
});

test('expansion during sampling restarts with the new dimensions and navigation cleans up',async({page})=>{
 await page.goto('/units?lang=en');await page.locator('.primary-nav a[href^="/graph"]').click();let release,first=true;const held=new Promise(r=>release=r);
 await page.route('**/api/graph/plot?**',async route=>{const response=await route.fetch();if(first){first=false;await held;}try{await route.fulfill({response});}catch(_) {}});
 await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).toHaveAttribute('aria-busy','true');
 await page.locator('#graph-expand').click();release();await expect(page.locator('#graph-expanded path.graph-line')).toBeVisible();
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 await page.goBack();
 await expect(page.locator('#unit-converter')).toBeVisible();await expect(page.locator('body')).not.toHaveClass(/graph-expanded-open/);await expect(page.locator('dialog[open]')).toHaveCount(0);
});

test('unresolved detail remains visible for its curve in a multi-function plot',async({page})=>{
 await page.goto('/graph?lang=en');await input(page,0).fill('sin(1000x)+sin(1001x)+sin(x*x)');await add(page,'x');await plot(page);
 await expect(page.locator('#graph-status')).toContainText('with unresolved detail');
 await expect(rows(page).nth(0).locator('.graph-curve-status')).toContainText('Some visible intervals');
 await expect(page.locator('path.graph-line')).toBeVisible();
});
test('calculator arithmetic survives a missing shared library asset',async({page})=>{
 await page.route('**/assets/function-library.js',route=>route.abort());await page.goto('/?lang=en');
 await page.locator('#expression').fill('2+2');await page.locator('#expression').press('Enter');await expect(page.locator('#result')).toHaveText('4');
});
