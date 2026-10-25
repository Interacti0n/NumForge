const {test,expect}=require('@playwright/test');
const {artifactPath}=require('./artifacts');
async function fill(page,name,values) {
 await page.locator(`#matrix-${name}-rows`).selectOption(String(values.length));
 await page.locator(`#matrix-${name}-columns`).selectOption(String(values[0].length));
 const inputs=page.locator(`#matrix-${name} input`);
 for(let i=0;i<values.flat().length;i++)await inputs.nth(i).fill(values.flat()[i]);
}
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
