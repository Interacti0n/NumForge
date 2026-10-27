const {test,expect}=require('@playwright/test');
const {artifactPath}=require('./artifacts');
const provider='https://api.frankfurter.dev/v2/providers/ecb/rates?base=EUR';
const today=()=>new Date().toISOString().slice(0,10);
const rows=()=>Object.entries({EUR:1,USD:1.2,CZK:24,GBP:.8,JPY:160}).map(([quote,rate])=>({date:today(),base:'EUR',quote,rate}));
const cache=(extra={})=>({provider:'ecb',date:today(),checkedAt:Date.now()-7200000,rates:{EUR:'1',USD:'1.2',CZK:'24',GBP:'0.8',JPY:'160'},...extra});
async function mock(page,data=rows()){let calls=0;await page.route(provider,route=>{++calls;return route.fulfill({json:data});});return()=>calls;}
async function open(page,lang='en'){await page.goto('/units?lang='+lang);await page.locator('#currencies-tab').click();}
async function convert(page,value,expected){await page.locator('#currency-amount').fill(value);await page.locator('#currency-submit').click();await expect(page.locator('#currency-result')).toHaveAttribute('data-raw-result',expected);}

for(const lang of ['sk','en'])test(`daily currencies use shared C arithmetic, source metadata and a private amount ${lang}`,async({page})=>{
 const calls=await mock(page),external=[],calculations=[];
 page.on('request',r=>{if(r.url().startsWith('https://api.frankfurter.dev'))external.push(r);if(r.url().includes('/api/currency/convert?'))calculations.push(r);});
 await page.goto('/units?lang='+lang);expect(calls()).toBe(0);await page.locator('#currencies-tab').click();
 await expect(page.locator('#currency-result')).toHaveAttribute('data-raw-result','120');
 await expect(page.locator('#currency-rate-status')).toContainText(lang==='sk'?'Denné':'Daily');await expect(page.locator('#currency-date')).not.toBeEmpty();
 await expect(page.locator('#currency-rates tr')).toHaveCount(5);expect(calls()).toBe(1);
 await page.locator('#currency-from').selectOption('USD');await page.locator('#currency-to').selectOption('CZK');await convert(page,'100,50','2010');
 await page.locator('#currency-swap').click();await expect(page.locator('#currency-result')).toHaveAttribute('data-raw-result','5.02');
 await page.locator('#currency-from').selectOption('EUR');await page.locator('#currency-to').selectOption('USD');await convert(page,'9007199254740993','10808639105689191.6');
 await page.locator('#currency-copy').click();expect(await page.evaluate(()=>navigator.clipboard.readText())).toBe('10808639105689191.6');
 await page.locator('#currency-to').selectOption('JPY');await convert(page,'1.234','197');
 await page.locator('#currency-places').selectOption('4');await convert(page,'-0.0001','-0.016');
 const before=calculations.length;await page.locator('#currency-amount').fill('1+2');await page.locator('#currency-submit').click();await expect(page.locator('#currency-status')).toHaveClass(/error/);expect(calculations.length).toBe(before);
 expect(external).toHaveLength(1);expect(external[0].postData()).toBeNull();expect(external[0].url()).toBe(provider);
 expect(calculations.some(r=>r.postData().includes('9007199254740993'))).toBe(true);
 await page.locator('#currency-search').fill('USD');await expect(page.locator('#currency-rates tr')).toHaveCount(1);
 await page.locator('#currency-search').fill('missing');await expect(page.locator('#currency-empty')).toBeVisible();
});

test('rate cache, amount and chosen mode survive reload and SPA language/tool navigation',async({page})=>{
 const calls=await mock(page);await open(page);await expect(page.locator('#currency-result')).toHaveAttribute('data-raw-result','120');
 await convert(page,'42','50.4');await page.reload();await expect(page.locator('#currency-result')).toHaveAttribute('data-raw-result','50.4');expect(calls()).toBe(1);
 await page.locator('.language-switch a[lang=sk]').click();await expect(page.locator('#currency-title')).toHaveText('Prevod mien');await expect(page.locator('#currency-result')).toHaveAttribute('data-raw-result','50.4');expect(calls()).toBe(1);
 await page.locator('#currency-from').selectOption('USD');await page.locator('#currency-to').selectOption('CZK');await convert(page,'42','840');
 await page.locator('#units-tab').click();await expect(page.locator('#unit-result')).not.toBeEmpty();await expect(page.locator('#currency-panel')).toBeHidden();
 await page.locator('.primary-nav a[href="/?lang=sk"]').click();await expect(page.locator('#calculator')).toBeVisible();
 await page.locator('.primary-nav a[href="/units?lang=sk"]').click();await page.locator('#currencies-tab').click();await expect(page.locator('#currency-result')).toHaveAttribute('data-raw-result','840');expect(calls()).toBe(1);
});

test('provider failure labels cached rates and manual retry works after its cooldown',async({page})=>{
 await page.clock.install();const saved=cache();await page.addInitScript(data=>sessionStorage.setItem('numforge-ecb-rates-v1',JSON.stringify(data)),saved);
 let fail=true,calls=0;await page.route(provider,route=>{++calls;return fail?route.abort():route.fulfill({json:rows()});});await open(page);
 await expect(page.locator('#currency-rate-status')).toContainText('Saved rates');await expect(page.locator('#currency-result')).toHaveAttribute('data-raw-result','120');
 await expect(page.locator('#currency-refresh')).toBeDisabled();await page.clock.runFor(60010);await expect(page.locator('#currency-refresh')).toBeEnabled();
 fail=false;await page.locator('#currency-refresh').click();await expect(page.locator('#currency-rate-status')).toHaveText('Daily ECB reference rates');expect(calls).toBe(2);
});

for(const kind of ['mixed dates','duplicate code','negative rate','future date','self rate','oversized'])test(`invalid provider response cannot create usable rates: ${kind}`,async({page})=>{
 const data=rows();if(kind==='mixed dates')data[0].date='2025-01-01';if(kind==='duplicate code')data[1].quote=data[0].quote;if(kind==='negative rate')data[0].rate=-1;if(kind==='future date')data.forEach(r=>r.date='2099-01-01');if(kind==='self rate')data[0].rate=2;
 await page.route(provider,r=>r.fulfill({contentType:'application/json',body:kind==='oversized'?' '.repeat(64001):JSON.stringify(data)}));await open(page);
 await expect(page.locator('#currency-rate-status')).toContainText('unavailable');await expect(page.locator('#currency-submit')).toBeDisabled();await expect(page.locator('#currency-result')).toBeEmpty();
 expect(await page.evaluate(()=>sessionStorage.getItem('numforge-ecb-rates-v1'))).toBeNull();
});

test('expired cached rates are rejected and late calculations cannot replace edited inputs',async({page})=>{
 await page.addInitScript(data=>sessionStorage.setItem('numforge-ecb-rates-v1',JSON.stringify(data)),cache({date:'2020-01-01'}));await page.route(provider,r=>r.abort());await open(page);
 await expect(page.locator('#currency-submit')).toBeDisabled();await expect(page.locator('#currency-result')).toBeEmpty();
 await page.unroute(provider);await mock(page);await page.evaluate(()=>sessionStorage.removeItem('numforge-ecb-rates-v1'));await page.reload();await expect(page.locator('#currency-result')).toHaveAttribute('data-raw-result','120');
 let release;const gate=new Promise(r=>release=r);await page.route('**/api/currency/convert?places=2',async route=>{const response=await route.fetch();await gate;await route.fulfill({response}).catch(()=>{});});
 await page.locator('#currency-amount').fill('300');await page.locator('#currency-submit').click();await expect(page.locator('#currency-form')).toHaveAttribute('aria-busy','true');await page.locator('#currency-amount').fill('700');release();
 await expect(page.locator('#currency-result')).toBeEmpty();await expect(page.locator('#currency-copy')).toBeDisabled();
 await page.locator('#units-tab').click();await expect(page.locator('#currency-panel')).toBeHidden();
});

test('currencies fit phones and desktops and tabs support keyboard navigation',async({page})=>{
 await mock(page);await page.setViewportSize({width:1440,height:900});await open(page);await expect(page.locator('#currency-result')).not.toBeEmpty();
 for(const [width,height] of [[1440,900],[1280,768],[390,844],[320,568]]){
  await page.setViewportSize({width,height});expect(await page.evaluate(()=>document.documentElement.scrollWidth)).toBeLessThanOrEqual(width);
  await expect(page.locator('#currency-submit')).toBeVisible();expect((await page.locator('#currency-submit').boundingBox()).height).toBeGreaterThanOrEqual(44);
  if(width===1440||width===390)await page.screenshot({path:artifactPath(`currency-${width}.png`),fullPage:true});
 }
 await page.locator('#currencies-tab').focus();await page.keyboard.press('Home');await expect(page.locator('#units-tab')).toBeFocused();await expect(page.locator('#unit-converter-panel')).toBeVisible();
 await page.keyboard.press('End');await expect(page.locator('#currencies-tab')).toBeFocused();await expect(page.locator('#currency-panel')).toBeVisible();
});

test('currency HTTP arithmetic validates supplied data and retains origin protections',async({request})=>{
 const path='/api/currency/convert?places=2';
 const good=await request.post(path,{data:'9007199254740993\n1\n1.2'});expect(good.status()).toBe(200);
 expect(await good.json()).toEqual({ok:true,approximate:true,result:'10808639105689191.6'});
 for(const [url,data]of [[path,'1\n0\n1'],[path,'1\n1\n1\u0000x'],[path,'9'.repeat(309)],[path+'&client=x','1\n1\n1']]){
  const bad=await request.post(url,{data});expect(bad.status()).toBe(400);expect(await bad.json()).toEqual({ok:false,status:'invalid argument'});
 }
 const forbidden=await request.post(path,{data:'1\n1\n1',headers:{Origin:'https://example.invalid'}});expect(forbidden.status()).toBe(403);
});
