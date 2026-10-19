const { artifactPath } = require('./artifacts');
const {test,expect}=require('@playwright/test');
const rows=page=>page.locator('.graph-curve');
const expression=(page,i)=>rows(page).nth(i).locator('input:not([type=color])');
const toggle=(page,i)=>rows(page).nth(i).locator('.graph-visibility');
async function plot(page){await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');}
for(const [lang,width] of [['en',1280],['sk',390]])test(`visibility reuses computed curves, colors and camera ${lang}`,async({page})=>{
 await page.setViewportSize({width,height:844});await page.goto('/graph?lang='+lang);await page.locator('#graph-add').click();await expression(page,1).fill('cos(x)');
 const prepared=new Set();page.on('response',r=>{const q=new URL(r.url()).searchParams;if(r.url().includes('/api/graph/plot?')&&q.get('min')==='-10'&&q.get('ymin')==='-5')prepared.add(r.request().postData());});
 await plot(page);await expect(page.locator('path.graph-line')).toHaveCount(2);await expect.poll(()=>prepared.size).toBe(2);
 const id=await rows(page).nth(1).getAttribute('data-curve'),original=await page.locator(`path.graph-line[data-curve="${id}"]`).getAttribute('d');
 const calls=[];page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))calls.push(r);});
 await toggle(page,1).focus();await toggle(page,1).press('Space');await expect(toggle(page,1)).toHaveAttribute('aria-pressed','false');await expect(page.locator(`path.graph-line[data-curve="${id}"]`)).toHaveCount(0);await expect(page.locator('path.graph-line')).toHaveCount(1);
await expect(expression(page,1)).toHaveValue('cos(x)');
 await toggle(page,1).click();await expect(page.locator(`path.graph-line[data-curve="${id}"]`)).toHaveAttribute('d',original);await page.waitForTimeout(250);expect(calls.length).toBe(0);
 await toggle(page,1).click();await page.locator('#graph-plot').press('ArrowRight');await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-6');await page.waitForTimeout(350);expect(calls.every(r=>r.postData()!=='cos(x)')).toBe(true);
 await rows(page).nth(1).locator('input[type=color]').evaluate(e=>{e.value='#ff2233';e.dispatchEvent(new Event('input',{bubbles:true}));});
 calls.length=0;await toggle(page,1).click();await expect(page.locator(`path.graph-line[data-curve="${id}"]`)).toHaveCSS('stroke','rgb(255, 34, 51)');await page.waitForTimeout(250);expect(calls.filter(r=>r.postData()==='cos(x)').length).toBe(0);
 await page.locator('.graph-workspace').screenshot({path:artifactPath(`graph-visibility-${lang}.png`)});
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBe(true);
});
test('all hidden curves preserve axes and do no work until explicitly shown',async({page})=>{
 await page.goto('/graph?lang=en');await toggle(page,0).click();let calls=0;page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))++calls;});
 await plot(page);await page.locator('#graph-plot').press('+');await page.waitForTimeout(300);expect(calls).toBe(0);await expect(page.locator('#graph-status')).toHaveText('All functions are hidden.');await expect(page.locator('.graph-axis[data-axis=x]')).toHaveCount(1);await expect(page.locator('#graph-plot')).toBeVisible();
await page.locator('#graph-expand').click();await page.locator('#graph-legend button').click();await expect(page.locator('path.graph-line')).toBeVisible();await expect(page.locator('#graph-legend button')).toHaveAttribute('aria-pressed','true');expect(calls).toBeGreaterThan(0);
 await page.locator('#graph-legend button').press('Space');await expect(page.locator('#graph-legend button')).toBeFocused();await expect(page.locator('path.graph-line')).toHaveCount(0);await page.keyboard.press('Escape');await expect(toggle(page,0)).toHaveAttribute('aria-pressed','false');
});
test('hidden curves retain their expression and visibility across languages and reload',async({page})=>{
 await page.goto('/graph?lang=en');await toggle(page,0).click();await page.locator('#graph-add').click();await expression(page,1).fill('cos(t)');await expect(toggle(page,1)).toHaveAttribute('aria-pressed','true');
 await page.locator('.language-switch a[lang=sk]').click();await expect(toggle(page,0)).toHaveAttribute('aria-label','Zobraziť funkciu 1');await expect(toggle(page,0)).toHaveAttribute('aria-pressed','false');await expect(toggle(page,1)).toHaveAttribute('aria-pressed','true');
 await page.reload();await expect(toggle(page,0)).toHaveAttribute('aria-pressed','false');await expect(expression(page,0)).toHaveValue('sin(x)');await expect(expression(page,1)).toHaveValue('cos(t)');
 const bodies=[];page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))bodies.push(r.postData());});await plot(page);await expect(page.locator('path.graph-line')).toHaveCount(1);expect(bodies.every(body=>body==='cos(t)')).toBe(true);
});
test('hiding a pending curve cancels stale work; edits while hidden are calculated only on show',async({page})=>{
 await page.goto('/graph?lang=en');await page.locator('#graph-add').click();await expression(page,1).fill('cos(x)');let release;const held=new Promise(resolve=>release=resolve);let waiting=false;
 await page.route('**/api/graph/plot?**',async route=>{const response=await route.fetch();if(route.request().postData()==='cos(x)'){waiting=true;await held;}try{await route.fulfill({response});}catch(_) {}});
 await page.locator('#graph-submit').click();await expect.poll(()=>waiting).toBe(true);await toggle(page,1).click();release();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('path.graph-line')).toHaveCount(1);
 await expression(page,1).fill('x*x');const bodies=[];page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))bodies.push(r.postData());});await plot(page);expect(bodies.includes('x*x')).toBe(false);
 await toggle(page,1).click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('path.graph-line')).toHaveCount(2);await expect.poll(()=>bodies.includes('x*x')).toBe(true);
});
