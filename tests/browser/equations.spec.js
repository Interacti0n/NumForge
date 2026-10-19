const { artifactPath } = require('./artifacts');
const {test,expect}=require('@playwright/test');
async function coefficients(page,a,b,c) {
 await page.locator('#equation-mode').selectOption('coefficients');
 for(const [name,value] of Object.entries({a,b,c}))await page.locator('#equation-'+name).fill(value);
}
for(const language of ['sk','en'])test(`equation roots, exact expressions and errors ${language}`,async({page})=>{
 await page.goto('/solve?lang='+language);
 await expect(page.locator('#equation-form')).toBeVisible();
 await page.locator('#equation-submit').click();
 await expect(page.locator('#equation-roots')).toContainText('-sqrt(2)');
 await expect(page.locator('#equation-roots')).toContainText('≈ 1.414213562373095');
 await coefficients(page,'0','3','-1');await page.locator('#equation-c').press('Enter');
 await expect(page.locator('#equation-roots')).toContainText('x = 1/3');
 await expect(page.locator('#equation-roots')).toContainText('≈ 0.3333333333333333');
 await page.locator('.equation-root button').click();
 await expect.poll(()=>page.evaluate(()=>navigator.clipboard.readText())).toBe('1/3');
 await coefficients(page,'1','0','1');await page.locator('#equation-submit').click();
 await expect(page.locator('.equation-root')).toHaveCount(2);
 await expect(page.locator('.equation-root').first()).toContainText('x₁ = -i');
 await coefficients(page,'4','12','9');await page.locator('#equation-submit').click();
 await expect(page.locator('.equation-root')).toHaveCount(1);
 await expect(page.locator('#equation-roots')).toContainText(language==='sk'?'Dvojnásobný koreň':'Double root');
 await coefficients(page,'0','0','0');await page.locator('#equation-submit').click();
 await expect(page.locator('#equation-status')).toContainText(language==='sk'?'každé komplexné':'Every complex');
 await coefficients(page,'0','0','1');await page.locator('#equation-submit').click();
 await expect(page.locator('#equation-status')).toContainText(language==='sk'?'nemá riešenie':'no solutions');
 await coefficients(page,'1','sqrt(2)','1');await page.locator('#equation-submit').click();
 await expect(page.locator('#equation-error')).toBeVisible();
 await expect(page.locator('#equation-b')).toBeFocused();
 await expect(page.locator('#equation-b')).toHaveAttribute('aria-invalid','true');
 await expect(page.locator('.equation-root')).toHaveCount(0);
 await page.locator('#equation-clear').click();
 await expect(page.locator('#equation-a')).toHaveValue('');
 await expect(page.locator('#equation-a')).toBeFocused();
 await expect(page.locator('#equation-error')).toBeHidden();
});

test('equations preserve inputs and results across language and tool navigation',async({page})=>{
 await page.goto('/solve?lang=en');await coefficients(page,'0','3','-1');await page.locator('#equation-submit').click();
 await expect(page.locator('#equation-roots')).toContainText('1/3');
 await page.locator('.language-switch a[lang=sk]').click();
 await expect(page).toHaveURL(/\/solve\?lang=sk/);
 await expect(page.locator('#equation-b')).toHaveValue('3');await expect(page.locator('#equation-roots')).toContainText('1/3');
 await page.locator('.primary-nav a[href="/?lang=sk"]').click();await expect(page.locator('#calculator')).toBeVisible();
 await page.locator('.primary-nav a[href="/solve?lang=sk"]').click();await expect(page.locator('#equation-roots')).toContainText('1/3');
 await expect(page.locator('#equation-submit')).toBeEnabled();
});

test('editing cancels pending equation work and prevents stale solutions',async({page})=>{
 await page.goto('/solve?lang=en');
 let release;const held=new Promise(resolve=>release=resolve);let started;
 const waiting=new Promise(resolve=>started=resolve);
 await page.route('**/api/equations/solve?**',async route=>{started();await held;await route.fulfill({json:{ok:true,kind:'finite',count:1,roots:[{exact:'999',approximation:'999',multiplicity:1}]}}).catch(()=>{});});
 await page.locator('#equation-submit').click();await waiting;
 await page.locator('#equation-expression').fill('2x=1');release();
 await expect(page.locator('#equation-submit')).toBeEnabled();
 await expect(page.locator('.equation-root')).toHaveCount(0);
 await expect(page.locator('#equation-error')).toBeHidden();
});

test('preview precision changes decimals while preserving exact roots',async({page})=>{
 await page.goto('/solve?lang=en');
 await page.locator('.equation-settings summary').click();
 await page.locator('#equation-precision').fill('4');
 await page.locator('#equation-submit').click();
 await expect(page.locator('.equation-root').last().locator('code').first()).toHaveText('x₂ = sqrt(2)');
 await expect(page.locator('.equation-root').last().locator('.equation-approx')).toHaveText('≈ 1.414');
});

for(const language of ['sk','en'])test(`whole equation entry, products and supported domains ${language}`,async({page})=>{
 await page.goto('/solve?lang='+language);
 await expect(page.locator('#equation-expression')).toBeVisible();
 await expect(page.locator('#equation-a')).toBeHidden();
 await page.locator('#equation-expression').fill('(x-2)*(x+3)=0');
 await page.locator('#equation-expression').press('Enter');
 await expect(page.locator('.equation-root').first()).toContainText('x₁ = -3');
 await expect(page.locator('.equation-root').last()).toContainText('x₂ = 2');
 await page.locator('#equation-expression').fill('3x+1=7');await page.locator('#equation-submit').click();
 await expect(page.locator('#equation-roots')).toContainText('x = 2');
 await page.locator('#equation-expression').fill('x/x=1');await page.locator('#equation-submit').click();
 await expect(page.locator('#equation-error')).toContainText(language==='sk'?'Bez x v menovateli':'No x in denominators');
 await expect(page.locator('#equation-expression')).toBeFocused();
 await expect(page.locator('.equation-root')).toHaveCount(0);
 await page.locator('#equation-expression').fill('x^2=2');await page.locator('#equation-submit').click();
 await expect(page.locator('#equation-roots')).toContainText('sqrt(2)');
 const other=language==='sk'?'en':'sk';await page.locator(`.language-switch a[lang=${other}]`).click();
 await expect(page.locator('#equation-mode')).toHaveValue('expression');
 await expect(page.locator('#equation-expression')).toHaveValue('x^2=2');
 await expect(page.locator('#equation-roots')).toContainText('sqrt(2)');
 await page.locator('#equation-clear').click();await expect(page.locator('#equation-expression')).toHaveValue('');
 await expect(page.locator('#equation-expression')).toBeFocused();
});

for(const language of ['sk','en'])test(`numerical interval root, residual and honest diagnostics ${language}`,async({page,request})=>{
 await page.goto('/solve?lang='+language);await page.locator('#equation-mode').selectOption('numeric');
 await page.locator('#equation-expression').fill('cos(x)=x');
 await page.locator('#equation-min').fill('0');await page.locator('#equation-max').fill('1');
 const response=page.waitForResponse(r=>r.url().includes('mode=numeric')&&r.request().postData()==='cos(x)=x');
 await page.locator('#equation-submit').click();const payload=await (await response).json();
 expect(payload.complete).toBe(false);expect(payload.outcome).toBe('found');
 expect(Math.abs(Number(payload.roots[0].approximation)-0.7390851332151607)).toBeLessThan(1e-12);
 await expect(page.locator('.equation-root code').first()).toContainText('x ≈ 0.739085133215');
 await expect(page.locator('.equation-root small')).toContainText(language==='sk'?'Zvyšok':'Residual');
 await page.locator('.equation-root button').click();
 await expect.poll(()=>page.evaluate(()=>navigator.clipboard.readText())).toBe(payload.roots[0].x);
 const checked=await request.post('/api/evaluate?precision=50&angle=rad',{data:`cos(${payload.roots[0].x})-(${payload.roots[0].x})`});
 expect(checked.ok()).toBe(true);expect(Math.abs(Number((await checked.json()).result))).toBeLessThanOrEqual(1e-12);
 const other=language==='sk'?'en':'sk';await page.locator(`.language-switch a[lang=${other}]`).click();
 await expect(page.locator('#equation-mode')).toHaveValue('numeric');await expect(page.locator('#equation-max')).toHaveValue('1');
 await expect(page.locator('.equation-root')).toHaveCount(1);
 await page.locator('#equation-expression').fill('(x-0.1234)^2=0');await page.locator('#equation-submit').click();
 await expect(page.locator('#equation-status')).toContainText(other==='sk'?'Koreň môže existovať':'A root may still exist');
 await expect(page.locator('.equation-root')).toHaveCount(0);
 await page.locator('#equation-expression').fill('x-0.1234+1E-30*(x-0.1234)/abs(x-0.1234)=0');
 await page.locator('#equation-submit').click();
 await expect(page.locator('#equation-status')).toContainText(other==='sk'?'nepodarilo overiť spojitosť':'could not be verified');
 await expect(page.locator('.equation-root')).toHaveCount(0);
 await page.locator('#equation-min').fill('230.25850929940456840179914546843642076010');
 await page.locator('#equation-max').fill('230.25850929940456840179914546843642076012');
 await page.locator('.equation-settings summary').click();await page.locator('#equation-precision').fill('34');
 await page.locator('#equation-expression').fill('exp(x)=1E100');
 await page.locator('#equation-submit').click();
 await expect(page.locator('#equation-status')).toContainText(other==='sk'?'Zvýš presnosť':'Increase precision');
 await expect(page.locator('.equation-root')).toHaveCount(0);await page.locator('#equation-max').fill('1');
 await page.locator('#equation-min').fill('-1');await page.locator('#equation-expression').fill('1/x=0');await page.locator('#equation-submit').click();
 await expect(page.locator('#equation-status')).toContainText(other==='sk'?'nedefinovaná':'undefined');
 await page.locator('#equation-min').fill('2');await page.locator('#equation-submit').click();
 await expect(page.locator('#equation-error')).toContainText(other==='sk'?'Skontroluj interval':'Check that from');
 await expect(page.locator('#equation-min')).toBeFocused();
});

test('legacy numerical results are discarded while keeping the input',async({page})=>{
 await page.goto('/solve?lang=en');
 await page.evaluate(()=>sessionStorage.setItem('numforge-equations-v1',JSON.stringify({
  inputs:['1','0','-2'],mode:'numeric',expression:'cos(x)=x',bounds:['0','1'],precision:'16',
  numericDomainVersion:1,
  result:{kind:'numeric',outcome:'found',roots:[{x:'0.7',approximation:'0.7',residual:'0'}]}
 })));
 await page.reload();await expect(page.locator('#equation-mode')).toHaveValue('numeric');
 await expect(page.locator('#equation-expression')).toHaveValue('cos(x)=x');
 await expect(page.locator('#equation-max')).toHaveValue('1');await expect(page.locator('.equation-root')).toHaveCount(0);
 await page.locator('#equation-submit').click();await expect(page.locator('.equation-root')).toHaveCount(1);
 await page.reload();await expect(page.locator('.equation-root')).toHaveCount(1);
});

test('equation layout fits phone, landscape and desktop',async({page})=>{
 await page.goto('/solve?lang=sk');await page.locator('#equation-submit').click();
 await expect(page.locator('.equation-root')).toHaveCount(2);
 for(const [width,height] of [[390,844],[812,375],[1280,800]]){
  await page.setViewportSize({width,height});
  await page.evaluate(async()=>{await document.fonts.ready;await new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)));});
  await expect(page.locator('.equation-entry h1')).toBeVisible();
  expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBe(true);
  await page.screenshot({path:artifactPath(`equations-2026-10-17/layout-${width}.png`),fullPage:true});
 }
 await page.locator('#equation-mode').selectOption('numeric');await page.locator('#equation-expression').fill('cos(x)=x');
 await page.locator('#equation-min').fill('0');await page.locator('#equation-max').fill('1');await page.locator('#equation-submit').click();
 await expect(page.locator('.equation-root')).toHaveCount(1);
 for(const [width,height] of [[390,844],[812,375],[1280,800]]){
  await page.setViewportSize({width,height});
  await page.evaluate(async()=>{await document.fonts.ready;await new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)));});
  expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBe(true);
  await page.screenshot({path:artifactPath(`numeric-root-2026-10-18/layout-${width}.png`),fullPage:true});
 }
});
