const {test,expect}=require('@playwright/test');
const {artifactPath}=require('./artifacts');
const rates=page=>page.route('https://api.frankfurter.dev/**',r=>r.fulfill({json:[{base:'EUR',quote:'USD',rate:1.2,date:new Date().toISOString().slice(0,10)}]}));
const path=tool=>tool==='calculator'?'/':tool==='currencies'?'/units':'/'+tool;
async function open(page,tool,lang='en'){await page.goto(path(tool)+(path(tool).includes('?')?'&':'?')+'lang='+lang+(tool==='currencies'?'#currencies':''));}

for(const [width,height]of [[1280,768],[1440,900],[1920,1080]])for(const tool of ['calculator','graph','solve','matrices','units','currencies'])test(`desktop workbench fills its frame and keeps scrolling inside panels: ${tool} ${width}`,async({page})=>{
 await page.setViewportSize({width,height});await rates(page);await open(page,tool);
 if(tool==='units')await expect(page.locator('#unit-result')).not.toBeEmpty();if(tool==='currencies')await expect(page.locator('#currency-result')).not.toBeEmpty();
 const geometry=await page.evaluate(()=>{const shell=document.querySelector('.app-shell').getBoundingClientRect(),footer=document.querySelector('.page-footer').getBoundingClientRect();return{height:document.documentElement.scrollHeight,width:document.documentElement.scrollWidth,shell:Math.round(shell.bottom),frameWidth:Math.round(shell.width),cards:[...document.querySelectorAll('.card')].filter(e=>e.getClientRects().length&&!e.closest('dialog')).map(e=>{const r=e.getBoundingClientRect();return{top:r.top,bottom:r.bottom,width:r.width};}),footer:footer.bottom};});
 expect(geometry.height).toBeLessThanOrEqual(height+1);expect(geometry.width).toBe(width);expect(geometry.shell).toBe(height);expect(geometry.footer).toBeLessThanOrEqual(height+1);
 expect(geometry.cards.every(r=>r.top>=72&&r.bottom<=height-25&&r.width>240)).toBe(true);
 if(width===1920)expect(geometry.frameWidth).toBeGreaterThan(1800);
 if(tool==='solve'){
  const aligned=await page.evaluate(()=>['.equation-entry','.equation-result','.equation-library','.equation-history'].map(selector=>document.querySelector(selector).getBoundingClientRect()));
  expect(aligned[0].top).toBe(aligned[1].top);expect(aligned[0].bottom).toBe(aligned[1].bottom);expect(aligned[2].bottom).toBe(aligned[3].bottom);
 }
 if(tool==='currencies')expect(await page.locator('.currency-result').evaluate(e=>e.scrollHeight-e.clientHeight)).toBeLessThanOrEqual(1);
 if(width===1440)await page.screenshot({path:artifactPath(`unified-${tool}.png`),fullPage:true});
});

for(const tool of ['solve','matrices','units','currencies'])test(`small windows retain reachable controls and page scrolling: ${tool}`,async({page})=>{
 await rates(page);for(const [width,height]of [[320,568],[390,844],[844,390],[1024,768]]){
  await page.setViewportSize({width,height});await open(page,tool);expect(await page.evaluate(()=>document.documentElement.scrollWidth)).toBeLessThanOrEqual(width);
  const action=page.locator(tool==='solve'?'#equation-submit':tool==='matrices'?'#matrix-submit':tool==='currencies'?'#currency-submit':'#unit-submit');await action.scrollIntoViewIfNeeded();await expect(action).toBeInViewport();
  await page.locator('.page-footer').scrollIntoViewIfNeeded();await expect(page.locator('.page-footer')).toBeInViewport();
 }
});

for(const lang of ['sk','en'])test(`equation recent tasks restore all inputs without computing and retain independent history ${lang}`,async({page})=>{
 await open(page,'solve',lang);let calls=0;page.on('request',r=>{if(r.url().includes('/api/equations/solve'))++calls;});
 for(const expression of ['x=1','x=2']){await page.locator('#equation-expression').fill(expression);await page.locator('#equation-submit').click();await expect(page.locator('.equation-root')).toHaveCount(1);}
 const items=page.locator('#equation-history [data-restore]');await expect(items).toHaveCount(2);await expect(items.first()).toHaveAttribute('aria-current','true');
 await items.last().click();await expect(page.locator('#equation-expression')).toHaveValue('x=1');await expect(page.locator('.equation-root')).toHaveCount(0);expect(calls).toBe(2);
 await page.locator('#equation-mode').selectOption('numeric');await page.locator('#equation-expression').fill('cos(x)=x');await page.locator('#equation-min').fill('0');await page.locator('#equation-max').fill('1');await page.locator('.equation-settings summary').click();await page.locator('#equation-precision').fill('24');await page.locator('#equation-submit').click();await expect(page.locator('.equation-root')).toHaveCount(1);
 await page.locator('#equation-mode').selectOption('expression');await items.first().click();await expect(page.locator('#equation-mode')).toHaveValue('numeric');await expect(page.locator('#equation-max')).toHaveValue('1');await expect(page.locator('#equation-precision')).toHaveValue('24');expect(calls).toBe(3);
 await page.reload();await expect(items).toHaveCount(3);await page.locator('#equation-history [data-remove]').last().click();await expect(items).toHaveCount(2);
 await page.locator('#equation-history [data-history-clear]').click();await expect(items).toHaveCount(0);await expect(page.locator('#equation-history')).toBeFocused();await expect(page.locator('#equation-expression')).toHaveValue('cos(x)=x');
});

test('matrix recent tasks are bounded, restore the operation and full grids, and never auto-calculate',async({page})=>{
 await open(page,'matrices');let calls=0;page.on('request',r=>{if(r.url().includes('/api/matrix?'))++calls;});
 for(let i=0;i<10;i++){await page.locator('#matrix-a input').first().fill(String(i));await page.locator('#matrix-submit').click();await expect(page.locator('#matrix-status')).toHaveAttribute('data-state','ready');}
 const items=page.locator('#matrix-history [data-restore]');await expect(items).toHaveCount(8);await items.last().click();expect(calls).toBe(10);await expect(page.locator('#matrix-a input').first()).toHaveValue('2');await expect(page.locator('#matrix-output')).toBeEmpty();
 await page.locator('#matrix-preset').selectOption('infinite');await page.locator('#matrix-submit').click();await expect(page.locator('.solution-formula')).toBeVisible();
 await page.locator('#matrix-preset').selectOption('unique');await items.first().click();await expect(page.locator('#matrix-operation')).toHaveValue('solve_general');await expect(page.locator('#matrix-a input')).toHaveCount(6);await expect(page.locator('#matrix-b input')).toHaveCount(2);expect(calls).toBe(11);
 await page.reload();await expect(items).toHaveCount(8);await expect(page.locator('#matrix-output')).toBeEmpty();await page.locator('#matrix-history [data-remove]').first().click();await expect(items).toHaveCount(7);
});

test('large matrix grids and many equation roots scroll inside their own desktop panels',async({page})=>{
 await page.setViewportSize({width:1280,height:768});await open(page,'matrices');await page.locator('#matrix-operation').selectOption('multiply');
 for(const name of ['a','b'])for(const dimension of ['rows','columns'])await page.locator(`#matrix-${name}-${dimension}`).selectOption('8');
 for(const name of ['a','b']){const last=page.locator(`#matrix-${name} input`).last();await last.scrollIntoViewIfNeeded();await expect(last).toBeInViewport();await last.fill('2');}
 await expect(page.locator('#matrix-submit')).toBeInViewport();expect(await page.evaluate(()=>document.documentElement.scrollHeight)).toBe(768);
 await open(page,'solve');await page.locator('#equation-mode').selectOption('numeric_scan');await page.locator('#equation-expression').fill('sin(x)=0');await page.locator('#equation-min').fill('-50');await page.locator('#equation-max').fill('50');await page.locator('#equation-submit').click();await expect(page.locator('.equation-root')).toHaveCount(31);
 const area=page.locator('#equation-roots');expect(await area.evaluate(e=>e.scrollHeight>e.clientHeight)).toBe(true);await page.locator('.equation-root').last().scrollIntoViewIfNeeded();expect(await page.evaluate(()=>document.documentElement.scrollHeight)).toBe(768);await expect(page.locator('.page-footer')).toBeInViewport();
});

test('untrusted recent history is discarded and leaving a pending task cannot create a recent entry',async({page})=>{
 await page.addInitScript(()=>{sessionStorage.setItem('numforge-equation-recent-v1',JSON.stringify({version:1,items:[{mode:'bad',expression:'<img src=x onerror=alert(1)>'},{mode:'expression',inputs:['1','0','-2'],expression:'x=1',bounds:['0','1'],precision:'16',unused:'x'.repeat(21000)}]}));sessionStorage.setItem('numforge-matrix-recent-v1','x'.repeat(64001));});
 await open(page,'solve');await expect(page.locator('#equation-history [data-restore]')).toHaveCount(0);
 let release;const gate=new Promise(r=>release=r);await page.route('**/api/equations/solve?*',async route=>{const response=await route.fetch();await gate;await route.fulfill({response}).catch(()=>{});});await page.locator('#equation-submit').click();await expect(page.locator('#equation-form')).toHaveAttribute('aria-busy','true');
 await page.locator('.primary-nav a[href="/matrices?lang=en"]').click();release();await expect(page.locator('#matrix-history [data-restore]')).toHaveCount(0);
 await page.locator('.primary-nav a[href="/solve?lang=en"]').click();await expect(page.locator('#equation-history [data-restore]')).toHaveCount(0);
});

for(const tool of ['solve','matrices'])test(`restoring a recent task reveals its input on a phone: ${tool}`,async({page})=>{
 await page.setViewportSize({width:390,height:844});await open(page,tool);
 const matrix=tool==='matrices',input=page.locator(matrix?'#matrix-a input':'#equation-expression').first(),history=page.locator(matrix?'#matrix-history [data-restore]':'#equation-history [data-restore]');
 await page.locator(matrix?'#matrix-submit':'#equation-submit').click();await expect(history).toHaveCount(1);
 await history.click();await expect(input).toBeFocused();await expect(input).toBeInViewport();
 expect(await page.evaluate(()=>document.documentElement.scrollWidth)).toBe(390);
});
