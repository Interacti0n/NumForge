const {test,expect}=require('@playwright/test');
const {artifactPath}=require('./artifacts');
async function fill(page,name,values) {
 await page.locator(`#matrix-${name}-rows`).selectOption(String(values.length));
 await page.locator(`#matrix-${name}-columns`).selectOption(String(values[0].length));
 const inputs=page.locator(`#matrix-${name} input`);
 for(let i=0;i<values.flat().length;i++)await inputs.nth(i).fill(values.flat()[i]);
}
for(const lang of ['sk','en'])test(`phone matrix confirmation reveals the result and dismisses input ${lang}`,async({page})=>{
 await page.setViewportSize({width:390,height:844});
 await page.emulateMedia({reducedMotion:lang==='en'?'reduce':'no-preference'});
 await page.goto('/matrices?lang='+lang);
 await page.locator('#matrix-preset').selectOption('infinite');
 const input=page.locator('#matrix-a input').first();await input.focus();
 await input.press('Control+Enter');
 await expect(page.locator('.solution-formula')).toBeVisible();
 await expect(input).not.toBeFocused();
 await expect.poll(async()=>{
  const box=await page.locator('.matrix-result').boundingBox();return box.y;
 }).toBeGreaterThanOrEqual(70);
 await expect.poll(async()=>{
  const box=await page.locator('.matrix-result').boundingBox();return box.y;
 }).toBeLessThan(200);
 await expect(page.locator('#matrix-copy')).toBeInViewport();
 expect(await page.evaluate(()=>document.documentElement.scrollWidth)).toBe(390);
});
for(const lang of ['sk','en'])test(`exact matrix workflow, errors, copy and navigation ${lang}`,async({page})=>{
 await page.goto('/matrices?lang='+lang);
 const run=async()=>page.locator('#matrix-submit').click();
 await expect(page.locator('#matrix-panel-b')).toBeHidden();await run();
 await expect(page.locator('#matrix-output')).toHaveText('-2');
 await fill(page,'a',[['1/2','0.25'],['-2','3']]);await run();
 await expect(page.locator('#matrix-output')).toHaveText('2');
 await page.locator('#matrix-copy').click();expect(await page.evaluate(()=>navigator.clipboard.readText())).toBe('2');
 await page.locator('#matrix-operation').selectOption('multiply');
 await fill(page,'a',[['1','2'],['3','4']]);await fill(page,'b',[['5'],['6']]);await run();
 await expect(page.locator('#matrix-output td')).toHaveText(['17','39']);
 await page.locator('#matrix-copy').click();expect((await page.evaluate(()=>navigator.clipboard.readText())).replace(/\r\n/g,'\n')).toBe('17\n39');
 await page.locator('#matrix-operation').selectOption('solve');await run();
 await expect(page.locator('#matrix-output td')).toHaveText(['-4','9/2']);
 await fill(page,'a',[['1','2'],['2','4']]);await run();
 await expect(page.locator('#matrix-error')).toContainText(lang==='sk'?'singulárna':'singular');
 await expect(page.locator('#matrix-copy')).toBeDisabled();
 await page.locator('#matrix-operation').selectOption('rank');await run();
 await expect(page.locator('#matrix-output')).toHaveText('1');
 await page.locator('#matrix-a input').first().fill('1/0');await run();
 await expect(page.locator('#matrix-a input').first()).toHaveAttribute('aria-invalid','true');
 await expect(page.locator('#matrix-a input').first()).toBeFocused();
 await page.locator('#matrix-a input').first().fill('1 2');await run();
 await expect(page.locator('#matrix-error')).toBeVisible();
 await page.locator('#matrix-a input').first().fill('7/3');
 await page.locator('.language-switch a:not([aria-current])').click();
 await expect(page).toHaveURL(/matrices/);await expect(page.locator('#matrix-a input').first()).toHaveValue('7/3');
 await page.locator('.brand').click();await expect(page.locator('#calculator')).toBeVisible();
 await page.locator('.primary-nav a[href*="/matrices"]').click();
 await expect(page.locator('#matrix-a input').first()).toHaveValue('7/3');
});
test('matrix grids fit narrow screens and paste a spreadsheet without losing dimensions',async({page})=>{
 for(const width of [320,390,981,1440]) {
  await page.setViewportSize({width,height:844});await page.goto('/matrices?lang=sk');
  await page.locator('#matrix-operation').selectOption('rank');
  await page.locator('#matrix-a-columns').selectOption('8');
  expect(await page.evaluate(()=>document.documentElement.scrollWidth)).toBeLessThanOrEqual(width);
  const scroll=page.locator('#matrix-panel-a .matrix-scroll');
  if(width<500)expect(await scroll.evaluate(n=>n.scrollWidth>n.clientWidth)).toBe(true);
 }
 await page.locator('#matrix-a input').first().evaluate(input=>{
  const data=new DataTransfer();data.setData('text/plain','1\t2\t3\n2\t4\t6');
  input.dispatchEvent(new ClipboardEvent('paste',{clipboardData:data,bubbles:true,cancelable:true}));
 });
 await expect(page.locator('#matrix-a-columns')).toHaveValue('3');
 await page.locator('#matrix-submit').click();await expect(page.locator('#matrix-output')).toHaveText('1');
 await page.screenshot({path:artifactPath('matrices-desktop.png')});
 await page.setViewportSize({width:390,height:844});await page.screenshot({path:artifactPath('matrices-mobile.png'),fullPage:true});
});
test('editing and navigation cancel stale matrix responses; a failed request can be retried',async({page})=>{
 await page.goto('/matrices?lang=en');
 let release;const gate=new Promise(r=>release=r);
 await page.route('**/api/matrix?*',async route=>{await gate;await route.fulfill({status:200,json:{ok:true,exact:true,scalar:'999'}}).catch(()=>{});});
 await page.locator('#matrix-submit').click();await page.locator('#matrix-a input').first().fill('10');release();
 await expect(page.locator('#matrix-output')).toBeEmpty();await expect(page.locator('#matrix-copy')).toBeDisabled();
 await page.unroute('**/api/matrix?*');
 await page.route('**/api/matrix?*',route=>route.fulfill({status:503,json:{ok:false,code:'server_busy'}}));
 await page.locator('#matrix-submit').click();await expect(page.locator('#matrix-error')).toBeVisible();
 await page.unroute('**/api/matrix?*');await page.locator('#matrix-submit').click();
 await expect(page.locator('#matrix-output')).toHaveText('34');
 await page.reload();await expect(page.locator('#matrix-a input').first()).toHaveValue('10');
});
test('matrix HTTP contract rejects malformed requests and preserves shared origin policy',async({request})=>{
 const url='/api/matrix?operation=';
 const good=await request.post(url+'determinant',{data:'1 2\n3 4'});expect(await good.json()).toEqual({ok:true,exact:true,scalar:'-2'});
 for(const [op,body] of [['solve','1 2\n3 4'],['rank','1 2\n3'],['inverse','1'],['rank','1\0x']])
  expect((await request.post(url+op,{data:body})).status()).toBe(400);
 expect((await request.post(url+'rank',{data:'1',headers:{Origin:'https://untrusted.example'}})).status()).toBe(403);
});
for(const lang of ['sk','en'])test(`general matrix systems distinguish full solution families ${lang}`,async({page})=>{
 await page.goto('/matrices?lang='+lang);
 await page.locator('#matrix-preset').selectOption('infinite');await page.locator('#matrix-submit').click();
 await expect(page.locator('.solution-title')).toHaveText(lang==='sk'?'Nekonečne veľa riešení':'Infinitely many solutions');
 await expect(page.locator('.solution-formula')).toHaveText('x = x₀ + N · t');
 await expect(page.locator('.solution-panels').first().locator('td')).toHaveText(['1','0','0','-2','-3','1','0','0','1']);
 await page.locator('#matrix-copy').click();const copied=await page.evaluate(()=>navigator.clipboard.readText());
 expect(copied).toContain('t1 · [-2; 1; 0]');expect(copied).toContain('t2 · [-3; 0; 1]');
 await page.screenshot({path:artifactPath('matrix-family-'+lang+'.png'),fullPage:true});
 await page.locator('#matrix-preset').selectOption('none');await page.locator('#matrix-submit').click();
 await expect(page.locator('.solution-title')).toHaveText(lang==='sk'?'Sústava nemá riešenie':'The system has no solution');
 await expect(page.locator('#matrix-error')).toBeHidden();await expect(page.locator('.solution-formula')).toHaveCount(0);
 await page.locator('#matrix-preset').selectOption('unique');
 await fill(page,'a',[['1','0'],['0','1'],['1','1']]);await fill(page,'b',[['2'],['3'],['5']]);
 await page.locator('#matrix-submit').click();await expect(page.locator('#matrix-output td')).toHaveText(['2','3']);
});
test('matrix input tools preserve exact text and keyboard navigation stays inside the grid',async({page})=>{
 await page.goto('/matrices?lang=en');
 await fill(page,'a',[['1/3','2','3'],['4','5','6']]);
 await expect(page.locator('[data-identity=a]')).toBeDisabled();
 await page.locator('[data-transpose=a]').click();await expect(page.locator('#matrix-a-rows')).toHaveValue('3');
 expect(await page.locator('#matrix-a input').evaluateAll(inputs=>inputs.map(e=>e.value))).toEqual(['1/3','4','2','5','3','6']);
 await page.locator('[data-transpose=a]').click();await expect(page.locator('#matrix-a input').first()).toHaveValue('1/3');
 await page.locator('#matrix-a-columns').selectOption('2');await page.locator('[data-identity=a]').click();
 expect(await page.locator('#matrix-a input').evaluateAll(inputs=>inputs.map(e=>e.value))).toEqual(['1','0','0','1']);
 await page.locator('#matrix-a input').first().focus();await page.keyboard.press('Enter');
 await expect(page.locator('#matrix-a input').nth(2)).toBeFocused();await page.keyboard.press('Shift+Enter');
 await expect(page.locator('#matrix-a input').first()).toBeFocused();await page.keyboard.press('Alt+ArrowRight');
 await expect(page.locator('#matrix-a input').nth(1)).toBeFocused();await page.keyboard.press('Control+Enter');
 await expect(page.locator('#matrix-output')).toHaveText('1');
 await page.locator('#matrix-operation').selectOption('multiply');
 const originalA=await page.locator('#matrix-a input').evaluateAll(inputs=>inputs.map(e=>e.value));
 const originalB=await page.locator('#matrix-b input').evaluateAll(inputs=>inputs.map(e=>e.value));
 await page.locator('#matrix-swap').click();expect(await page.locator('#matrix-a input').evaluateAll(inputs=>inputs.map(e=>e.value))).toEqual(originalB);
 expect(await page.locator('#matrix-b input').evaluateAll(inputs=>inputs.map(e=>e.value))).toEqual(originalA);
});
test('matrix workspaces round-trip through a bounded file; invalid files leave inputs intact',async({page})=>{
 await page.goto('/matrices?lang=en');await page.locator('#matrix-preset').selectOption('infinite');
 const downloadPending=page.waitForEvent('download');await page.locator('#matrix-save').click();const download=await downloadPending;
 const stream=await download.createReadStream();let text='';for await(const chunk of stream)text+=chunk.toString('utf8');
 const data=JSON.parse(text);expect(data).toMatchObject({format:'numforge.matrix-workspace',version:1,operation:'solve_general'});
 await page.locator('#matrix-example').click();await expect(page.locator('#matrix-operation')).toHaveValue('determinant');
 await page.locator('#matrix-file').setInputFiles({name:'workspace.json',mimeType:'application/json',buffer:Buffer.from(text)});
 await expect(page.locator('#matrix-operation')).toHaveValue('solve_general');await expect(page.locator('#matrix-a-columns')).toHaveValue('3');
 await page.locator('#matrix-submit').click();await expect(page.locator('.solution-formula')).toBeVisible();
 const before=await page.locator('#matrix-a input').evaluateAll(inputs=>inputs.map(e=>e.value));
 for(const invalid of ['{bad',JSON.stringify({...data,version:99}),JSON.stringify({...data,a:[['1'],['2','3']]}),JSON.stringify({...data,a:[['1\n2']]}),JSON.stringify({...data,a:[['1\t2']]}),'x'.repeat(20001)]) {
  await page.locator('#matrix-file').setInputFiles({name:'bad.json',mimeType:'application/json',buffer:Buffer.from(invalid)});
  await expect(page.locator('#matrix-error')).toContainText('Inputs were preserved');
  expect(await page.locator('#matrix-a input').evaluateAll(inputs=>inputs.map(e=>e.value))).toEqual(before);
 }
});
test('spreadsheet paste preserves blank cell positions and rejects malformed tables atomically',async({page})=>{
 await page.goto('/matrices?lang=en');
 const paste=async text=>page.locator('#matrix-a input').first().evaluate((input,text)=>{
  const data=new DataTransfer();data.setData('text/plain',text);input.dispatchEvent(new ClipboardEvent('paste',{clipboardData:data,bubbles:true,cancelable:true}));
 },text);
 await paste('\t2\r\n3\t\r\n');
 expect(await page.locator('#matrix-a input').evaluateAll(inputs=>inputs.map(e=>e.value))).toEqual(['','2','3','']);
 await paste('1\t2\n3');await expect(page.locator('#matrix-error')).toContainText('Inputs were preserved');
 expect(await page.locator('#matrix-a input').evaluateAll(inputs=>inputs.map(e=>e.value))).toEqual(['','2','3','']);
 await paste('1\r2');expect(await page.locator('#matrix-a input').evaluateAll(inputs=>inputs.map(e=>e.value))).toEqual(['1','2']);
});
test('malformed solution payloads cannot publish a partial successful result',async({page})=>{
 await page.goto('/matrices?lang=en');await page.locator('#matrix-preset').selectOption('infinite');
 await page.route('**/api/matrix?*',route=>route.fulfill({json:{ok:true,exact:true,solution:{kind:'infinite',rank:1,nullity:2,particular:[['1']],basis:null,free_columns:[1,2]}}}));
 await page.locator('#matrix-submit').click();await expect(page.locator('#matrix-error')).toBeVisible();
 await expect(page.locator('#matrix-output')).toBeEmpty();await expect(page.locator('#matrix-copy')).toBeDisabled();
});
test('matrix family layout and controls remain usable on a touch phone',async({browser,baseURL})=>{
 const context=await browser.newContext({baseURL,viewport:{width:390,height:844},isMobile:true,hasTouch:true});
 try {
  const page=await context.newPage();await page.goto('/matrices?lang=sk');await page.locator('#matrix-preset').selectOption('infinite');
  await page.locator('#matrix-submit').click();await expect(page.locator('.solution-formula')).toBeVisible();
  expect(await page.evaluate(()=>document.documentElement.scrollWidth)).toBeLessThanOrEqual(390);
  for(const control of await page.locator('.matrix-dimensions button:visible,.matrix-dimensions select:visible,#matrix-submit').all())
   expect((await control.boundingBox()).height).toBeGreaterThanOrEqual(44);
  await page.screenshot({path:artifactPath('matrix-family-touch.png'),fullPage:true});
 }finally{await context.close();}
});
