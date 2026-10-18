const {test,expect}=require('@playwright/test');
async function scan(page,input,low,high){
 await page.locator('#equation-mode').selectOption('numeric_scan');await page.locator('#equation-expression').fill(input);
 await page.locator('#equation-min').fill(low);await page.locator('#equation-max').fill(high);
 const response=page.waitForResponse(r=>r.url().includes('/api/equations/solve?mode=numeric_scan'));
 await page.locator('#equation-submit').click();return (await response).json();
}
for(const lang of ['sk','en'])test(`multiple roots retain full coordinates, language and graph integration ${lang}`,async({page})=>{
 await page.goto('/solve?lang='+lang);const result=await scan(page,'sin(x)=0','-10','10');
 expect(result.ok).toBe(true);expect(result.complete).toBe(false);expect(result.count).toBe(7);
 await expect(page.locator('.equation-root')).toHaveCount(7);
 await expect(page.locator('#equation-status')).toContainText(lang==='sk'?'Ďalšie korene môžu existovať':'Other roots may exist');
 await page.locator('.equation-root').first().locator('button').click();
 await expect.poll(()=>page.evaluate(()=>navigator.clipboard.readText())).toBe(result.roots[0].x);
 const other=lang==='sk'?'en':'sk';await page.locator(`.language-switch a[lang=${other}]`).click();
 await expect(page.locator('#equation-mode')).toHaveValue('numeric_scan');await expect(page.locator('.equation-root')).toHaveCount(7);
 await page.locator('.primary-nav a[href="/?lang='+other+'"]').click();await page.locator('.primary-nav a[href="/solve?lang='+other+'"]').click();
 await expect(page.locator('.equation-root')).toHaveCount(7);
 await page.locator('.equation-root').nth(3).locator('.equation-graph-link').click();await expect(page).toHaveURL('/graph?lang='+other);
 await expect(page.locator('#graph-expression')).toHaveValue('(sin(x))-(0)');await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');
});

test('scan keeps domain gaps and nonconvergence separate from no roots',async({page})=>{
 await page.goto('/solve?lang=en');let result=await scan(page,'1/x=0','-1','1');
 expect(result.count).toBe(0);expect(result.domain_gaps).toBe(2);await expect(page.locator('#equation-status')).toContainText('could not confirm');
 await expect(page.locator('#equation-status')).toContainText('undefined');await expect(page.locator('.equation-root')).toHaveCount(0);
 result=await scan(page,'ln(x)=0','-2','2');expect(result.count).toBe(1);expect(Number(result.roots[0].x)).toBe(1);
 result=await scan(page,'x-0.1234+1E-30*(x-0.1234)/abs(x-0.1234)=0','0','1');expect(result.count).toBe(0);expect(result.rejected_brackets).toBe(1);
 result=await scan(page,'(x-0.1234)^2=0','0','1');expect(result.count).toBe(0);await expect(page.locator('#equation-status')).toContainText('Other roots may exist');
});

test('scan work and result limits remain explicit in the browser',async({page})=>{
 await page.goto('/solve?lang=en');let result=await scan(page,'x=x','0','1');
 expect(result.outcome).toBe('root_limit');expect(result.complete).toBe(false);await expect(page.locator('.equation-root')).toHaveCount(32);
 await expect(page.locator('#equation-status')).toContainText('32-result limit');
 result=await scan(page,'2*mod(floor(x);2)-1=0','0','128');expect(result.outcome).toBe('evaluation_limit');expect(result.evaluations).toBeLessThanOrEqual(4096);
 await expect(page.locator('#equation-status')).toContainText('work limit');await expect(page.locator('.equation-root')).toHaveCount(0);
 await page.reload();await expect(page.locator('#equation-status')).toContainText('work limit');
});

test('editing aborts a scan and numeric precision controls stay appropriate',async({page})=>{
 await page.goto('/solve?lang=en');await page.locator('#equation-mode').selectOption('numeric_scan');
 await expect(page.locator('.equation-settings summary')).toHaveText('Calculation precision');
 await page.locator('.equation-settings summary').click();await expect(page.locator('#equation-precision')).toHaveAttribute('min','16');
 await expect(page.locator('#equation-precision')).toHaveAttribute('max','100');
 let started,release;const waiting=new Promise(resolve=>started=resolve),held=new Promise(resolve=>release=resolve);
 await page.route('**/api/equations/solve?mode=numeric_scan**',async route=>{started();await held;await route.fulfill({json:{ok:true,kind:'numeric_scan',outcome:'scanned',complete:false,count:1,roots:[{x:'999',approximation:'999',residual:'0'}]}}).catch(()=>{});});
 await page.locator('#equation-submit').click();await waiting;await page.locator('#equation-expression').fill('x=1');release();
 await expect(page.locator('#equation-submit')).toBeEnabled();await expect(page.locator('.equation-root')).toHaveCount(0);
 await page.locator('#equation-mode').selectOption('expression');await expect(page.locator('.equation-settings summary')).toHaveText('Preview precision');
 await expect(page.locator('#equation-range')).toBeHidden();
});

test('scan results fit narrow screens and long lists preserve navigation',async({page})=>{
 await page.setViewportSize({width:390,height:844});await page.goto('/solve?lang=sk');await scan(page,'sin(x)=0','-10','10');
 await expect(page.locator('.equation-root')).toHaveCount(7);
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBe(true);
 await page.screenshot({path:'build/numeric-root-scan-2026-10-19/mobile.png',fullPage:true});
});
