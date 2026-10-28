const { artifactPath } = require('./artifacts');
const {test,expect}=require('@playwright/test');
async function open(page){await expect(page.locator('.equation-library')).toBeVisible();}
async function find(page,name){await page.locator('#equation-editor-search').fill(name);}

for(const lang of ['sk','en'])test(`equation editor inserts balanced functions and solves without preview ${lang}`,async({page})=>{
 await page.goto('/solve?lang='+lang);let solves=0;page.on('request',r=>{if(r.url().includes('/api/equations/solve'))solves++;});
 await page.locator('#equation-expression').fill('x=');await open(page);await find(page,lang==='sk'?'odmocnina':'square root');
 await page.locator('#equation-editor-functions [data-function=sqrt]').click();
 await expect(page.locator('#equation-expression')).toHaveValue('x=sqrt()');
 expect(await page.locator('#equation-expression').evaluate(e=>e.selectionStart)).toBe(7);
 await page.keyboard.type('4');await expect(page.locator('#equation-expression')).toHaveValue('x=sqrt(4)');expect(solves).toBe(0);
 await page.locator('#equation-submit').click();
 await expect(page.locator('#equation-roots')).toContainText('x = 2');expect(solves).toBe(1);
});

test('coefficient targets insert into the chosen input and validation retains focus',async({page})=>{
 await page.goto('/solve?lang=en');await page.locator('#equation-mode').selectOption('coefficients');
 await page.locator('#equation-a').fill('');await find(page,'sqrt');
 await page.locator('#equation-editor-functions [data-function=sqrt]').click();await page.keyboard.type('4');
 await page.locator('#equation-editor-target').selectOption('b');await page.locator('#equation-b').fill('3');
 await page.locator('#equation-editor-target').selectOption('c');await page.locator('#equation-c').fill('-2');
 await page.locator('#equation-submit').click();await expect(page.locator('#equation-roots')).toContainText('x₁ = -2');
 await expect(page.locator('#equation-roots')).toContainText('x₂ = 1/2');await expect(page.locator('#equation-a')).toHaveValue('sqrt(4)');
 await page.locator('#equation-b').fill('');await page.locator('#equation-submit').click();await expect(page.locator('#equation-b')).toBeFocused();
});

test('selected arguments and minimum arity place the cursor at the next argument',async({page})=>{
 await page.goto('/solve?lang=en');await page.locator('#equation-mode').selectOption('numeric');
 await page.locator('#equation-expression').fill('x=4');await page.locator('#equation-expression').evaluate(e=>e.setSelectionRange(2,3));
 await open(page);await find(page,'pow');await page.locator('#equation-editor-functions [data-function=pow]').click();
 await expect(page.locator('#equation-expression')).toHaveValue('x=pow(4;)');expect(await page.locator('#equation-expression').evaluate(e=>e.selectionStart)).toBe(8);
 await page.keyboard.type('2');await page.keyboard.press('Escape');await expect(page.locator('#equation-expression')).toHaveValue('x=pow(4;2)');
 await expect(page.locator('#equation-expression')).toBeFocused();expect(await page.locator('#equation-expression').evaluate(e=>e.selectionStart)).toBe(9);
});

for(const lang of ['en','sk'])test(`numeric constants use shared translated aliases and exact modes keep rational inputs ${lang}`,async({page})=>{
 await page.goto('/solve?lang='+lang);await page.locator('#equation-mode').selectOption('numeric');await page.locator('#equation-expression').fill('x=');
 await page.locator('#equation-min').fill('3');await page.locator('#equation-max').fill('4');await open(page);
 await find(page,lang==='en'?'ARCHIMEDES CONSTANT':'Ludolfovo číslo');await page.locator('#equation-editor-functions [data-insert="π"]').click();
 await page.locator('#equation-expression').press('Enter');await expect(page.locator('#equation-roots')).toContainText('3.14159');
 await page.locator('#equation-mode').selectOption('expression');await open(page);await find(page,'constant');
 await expect(page.locator('#equation-editor-functions [data-insert="π"]')).toHaveCount(0);
 await find(page,'rand');await expect(page.locator('#equation-editor-functions button:not([hidden])')).toHaveCount(0);
});

test('examples are mode-aware, fill interval and coefficients, and wait for explicit solve',async({page})=>{
 await page.goto('/solve?lang=en');
 await page.locator('#equation-examples button').last().click();await expect(page.locator('#equation-expression')).toHaveValue('x^2+1=0');
 await expect(page.locator('.equation-root')).toHaveCount(0);await page.locator('#equation-submit').click();await expect(page.locator('#equation-roots')).toContainText('-i');
 await page.locator('#equation-mode').selectOption('numeric');await page.locator('#equation-examples button').first().click();
 await expect(page.locator('#equation-min')).toHaveValue('0');await expect(page.locator('#equation-max')).toHaveValue('1');await expect(page.locator('.equation-root')).toHaveCount(0);
 await page.locator('#equation-mode').selectOption('coefficients');await page.locator('#equation-examples button').first().click();
 await expect(page.locator('#equation-a')).toHaveValue('0');await expect(page.locator('#equation-b')).toHaveValue('1/3');
});

test('catalogue failures allow manual editing and recover after navigation',async({page})=>{
 await page.route('**/api/functions',route=>route.fulfill({status:503,json:{ok:false}}));
 await page.goto('/solve?lang=en');await open(page);await expect(page.locator('#equation-editor-status')).toContainText('unavailable');
 await page.setViewportSize({width:390,height:844});await expect(page.locator('#equation-editor-status')).toBeVisible();
 await page.locator('#equation-expression').fill('x=2');await page.locator('#equation-submit').click();await expect(page.locator('#equation-roots')).toContainText('x = 2');
 await page.unroute('**/api/functions');await page.reload();await open(page);await find(page,'sqrt');await expect(page.locator('#equation-editor-functions [data-function=sqrt]')).toBeVisible();
});

test('failed numerical copy exposes the full checked coordinate for manual selection',async({page})=>{
 await page.addInitScript(()=>Object.defineProperty(navigator,'clipboard',{configurable:true,value:{writeText:()=>Promise.reject(new Error('Clipboard unavailable'))}}));
 await page.goto('/solve?lang=en');await page.locator('#equation-mode').selectOption('numeric_scan');
 await page.locator('#equation-expression').fill('(x-1/3)^2=0');await page.locator('#equation-min').fill('0');await page.locator('#equation-max').fill('1');
 await page.locator('#equation-submit').click();const root=page.locator('.equation-root');await expect(root).toHaveCount(1);
 const details=root.locator('.equation-verification');await expect(details).not.toHaveAttribute('open');
 await root.getByRole('button',{name:'Copy point',exact:true}).click();await expect(details).toHaveAttribute('open','');
 await expect(details.locator('code')).toHaveText('x = 1/3');await expect(details.locator('code')).toBeVisible();
 await expect(root.getByRole('button')).toContainText('Could not copy');
});

test('SPA entry loads shared metadata once and keeps inline insertion across language changes',async({page})=>{
 let requests=0;page.on('request',r=>{if(r.url().endsWith('/api/functions'))requests++;});
 await page.goto('/units?lang=en');await page.locator('.primary-nav a[href="/solve?lang=en"]').click();
 await page.locator('#equation-expression').fill('x=');await find(page,'sqrt');
 await page.locator('#equation-editor-functions [data-function=sqrt]').click();
 await expect(page.locator('#equation-expression')).toHaveValue('x=sqrt()');expect(requests).toBe(1);
 await page.locator('.language-switch a[lang=sk]').click();await expect(page.locator('#equation-expression')).toHaveValue('x=sqrt()');
 await expect(page.locator('#equation-editor')).toHaveCount(0);
});

test('inline categories, compact examples and search fit phone, landscape and desktop',async({page})=>{
 await page.goto('/solve?lang=sk');await page.locator('#equation-mode').selectOption('numeric_scan');
 await find(page,'sin');await expect(page.locator('#equation-editor-functions [data-function=sin]')).toBeVisible();
 await find(page,'');await page.locator('#equation-editor-tabs [data-category="algebra"]').click();
 for(const [width,height] of [[320,568],[390,844],[812,375],[1280,800],[1600,900]]){
  await page.evaluate(()=>document.activeElement?.blur());
  await page.setViewportSize({width,height});await page.evaluate(async()=>{await document.fonts.ready;await new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)));});
  expect(await page.evaluate(()=>document.documentElement.scrollWidth)).toBeLessThanOrEqual(width);
  const {library,entry}=await page.evaluate(()=>({library:document.querySelector('.equation-library').getBoundingClientRect().toJSON(),entry:document.querySelector('.equation-entry').getBoundingClientRect().toJSON()}));
  expect(library.y).toBeGreaterThanOrEqual(entry.y+entry.height);
  for(const button of await page.locator('#equation-examples button').all())expect((await button.boundingBox()).height).toBeLessThanOrEqual(50);
  await page.evaluate(()=>window.scrollTo(0,0));
  await page.screenshot({path:artifactPath(`equations-inline-20261028/solve-${width}.png`),fullPage:true});
 }
 await page.locator('#equation-editor-search').fill('');
 await page.locator('#equation-editor-tabs [data-category="powers"]').click();
 await expect(page.locator('#equation-editor-functions [data-function=sqrt]')).toBeVisible();
 await expect(page.locator('#equation-editor-functions [data-function=sin]')).toBeHidden();
 await find(page,'sínus');await expect(page.locator('#equation-editor-functions [data-function=sin]')).toBeVisible();
 await expect(page.locator('#equation-editor-tabs')).toBeHidden();
});

test('desktop solve buttons stay visible in every mode without scrolling the input panel',async({page})=>{
 await page.setViewportSize({width:1280,height:768});await page.goto('/solve?lang=en');
 for(const mode of ['expression','coefficients','numeric','numeric_scan']){
  await page.locator('#equation-mode').selectOption(mode);
  const entry=await page.locator('.equation-entry').boundingBox(),submit=await page.locator('#equation-submit').boundingBox();
  expect(submit.y).toBeGreaterThan(entry.y);expect(submit.y+submit.height).toBeLessThanOrEqual(entry.y+entry.height-8);
  expect(await page.evaluate(()=>document.documentElement.scrollHeight)).toBeLessThanOrEqual(769);
 }
});

test('mobile library insertion returns to the visible equation and keeps the argument caret',async({page})=>{
 await page.setViewportSize({width:390,height:844});await page.goto('/solve?lang=en');
 const input=page.locator('#equation-expression');await input.fill('x=');await find(page,'sqrt');
 await page.locator('#equation-editor-functions [data-function=sqrt]').click();
 await expect(input).toBeInViewport();await expect(input).toBeFocused();
 await page.keyboard.type('4');await expect(input).toHaveValue('x=sqrt(4)');
 await page.locator('#equation-submit').click();await expect(page.locator('#equation-roots')).toContainText('x = 2');
});
