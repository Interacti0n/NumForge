const { artifactPath } = require('./artifacts');
const {test,expect}=require('@playwright/test');
const parts={
 calculator:['intro','syntax','variables','functions','complex','quantities','precision','cache','random','history','calculator-limits'],
 units:['units','unit-results','unit-history','unit-limits'],
 graphs:['graphs','graph-settings','graph-reading','graph-color','graph-library','graph-limits'],
 equations:['equations','equation-results','equation-numeric','equation-scan','equation-limits'],
 matrices:['matrices'],
 api:['api-intro','http','session-http','unit-http','graph-http','equation-http','limits','library','documentation']
};
async function choose(page,part) {
 if(await page.locator('#guide-part').isVisible())await page.locator('#guide-part').selectOption(part);
 else await page.locator(`[data-guide-part="${part}"]`).click();
 await expect(page.locator(`[data-guide-panel="${part}"]`)).toBeVisible();
}
for(const lang of ['sk','en'])test(`focused guide parts, scroll tracking and mobile navigation ${lang}`,async({page})=>{
 test.setTimeout(30000);
 await page.setViewportSize({width:390,height:844});await page.goto('/api?lang='+lang);
 for(const [part,ids] of Object.entries(parts)) {
  await page.setViewportSize({width:390,height:568});await choose(page,part);
  expect(await page.locator('.guide-toc a:not([hidden])').evaluateAll(links=>links.map(link=>link.hash.slice(1)))).toEqual(ids);
  expect(await page.locator(`[data-guide-panel="${part}"] h1,[data-guide-panel="${part}"] h2`).evaluateAll(headings=>headings.map(heading=>heading.id))).toEqual(ids);
  await expect(page.locator('[data-guide-panel]:visible')).toHaveCount(1);
  for(const id of ids) {
   await page.evaluate(id=>{const pane=document.querySelector('.guide-content'),h=document.getElementById(id);pane.scrollTop+=h.getBoundingClientRect().top-pane.getBoundingClientRect().top-30;},id);
   await expect(page.locator('.guide-toc a[aria-current="location"]')).toHaveAttribute('href','#'+id);
   await expect(page.locator('.guide-position')).toHaveText(String(ids.indexOf(id)+1).padStart(2,'0')+' / '+ids.length);
   await expect.poll(()=>page.locator('.guide-toc a[aria-current="location"]').evaluate(n=>{const b=n.getBoundingClientRect(),bar=n.parentElement.getBoundingClientRect();return b.left>=bar.left-1&&b.right<=bar.right+1;})).toBe(true);
  }
  for(const [width,height] of [[320,568],[812,375],[1280,720],[390,844]]) {
   await page.setViewportSize({width,height});
   expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBe(true);
   await expect(page.locator('.guide-toc a[aria-current="location"]')).toHaveCount(1);
  }
 }
 await page.screenshot({path:artifactPath(`guide-parts-${lang}.png`),fullPage:true});
 const docs=page.locator('[data-guide-panel="api"] a[href^="https://github.com/Interacti0n/NumForge/blob/main/docs/"]');
 expect(await docs.count()).toBeGreaterThanOrEqual(10);
 for(const link of await docs.all()){await expect(link).toHaveAttribute('target','_blank');await expect(link).toHaveAttribute('rel','noopener noreferrer');}
});
for(const lang of ['sk','en'])test(`legacy guide anchors open the correct part and language links preserve it ${lang}`,async({page})=>{
 for(const [anchor,part] of [['variables','calculator'],['units','units'],['http','api'],['limits','api'],['graph-http','api'],['graph-variables','graphs']]) {
  await page.goto(`/api?lang=${lang}#${anchor}`);
  await expect(page.locator(`[data-guide-panel="${part}"]`)).toBeVisible();
  await expect(page.locator('.guide-toc a[aria-current="location"]')).toHaveAttribute('href','#'+(anchor==='graph-variables'?'graph-color':anchor));
 }
 await page.goto(`/api?section=api&lang=${lang}#graph-http`);
 const other=lang==='sk'?'en':'sk';
 await page.locator(`.language-switch a[lang="${other}"]`).click();
 await expect(page).toHaveURL(`/api?section=api&lang=${other}#graph-http`);
 await expect(page.locator('#graph-http')).toBeVisible();
 await expect(page.locator('.guide-nav-heading')).toHaveCount(1);
 await choose(page,'units');await page.locator('a[href="#quantities"]').last().click();
 await expect(page.locator('[data-guide-panel="calculator"]')).toBeVisible();
 await expect(page.locator('#quantities')).toBeVisible();
});
test('guide part switching keeps the document, supports Back/Forward and avoids HTML requests',async({page})=>{
 await page.goto('/api?lang=en');await page.evaluate(()=>window.guidePane=document.querySelector('.guide-content'));
 let requests=0;page.on('request',r=>{if(r.isNavigationRequest()||new URL(r.url()).pathname==='/api')requests++;});
 await choose(page,'units');await choose(page,'graphs');await choose(page,'api');
 expect(requests).toBe(0);expect(await page.evaluate(()=>window.guidePane===document.querySelector('.guide-content'))).toBe(true);
 await page.goBack();await expect(page.locator('[data-guide-panel="graphs"]')).toBeVisible();
 await page.goBack();await expect(page.locator('[data-guide-panel="units"]')).toBeVisible();
 await page.goForward();await expect(page.locator('[data-guide-panel="graphs"]')).toBeVisible();
 await page.locator('.header-guide').click();await expect(page.locator('[data-guide-panel="calculator"]')).toBeVisible();
});
test('guide routes accept only supported parts and languages in either order',async({request})=>{
 for(const query of ['section=calculator','section=units&lang=en','lang=sk&section=graphs','section=equations&lang=sk','section=api&lang=en']) {
  const response=await request.get('/api?'+query);expect(response.ok()).toBe(true);expect(await response.text()).toContain('id="guide-part"');
 }
 for(const query of ['section=unknown','lang=de','section=api&section=units','lang=en&lang=sk','section=api&extra=x','section=api&'])expect((await request.get('/api?'+query)).status()).toBe(404);
});

test('tools open focused help, expandable detail and readable mobile/desktop layouts',async({page})=>{
 for(const [tool,part] of [['units','units'],['graph','graphs']]) {
  await page.goto('/'+tool+'?lang=sk');await page.locator('.header-guide').click();
  await expect(page.locator(`[data-guide-panel="${part}"]`)).toBeVisible();
 }
 await page.goto('/api?lang=sk');await expect(page.locator('#http')).toBeHidden();
 await page.locator('.guide-toc a[href="#complex"]').click();
 const details=page.locator('[data-guide-panel=calculator] .guide-reference').nth(1);
 await expect(details).not.toHaveAttribute('open','');await details.locator('summary').click();await expect(details).toHaveAttribute('open','');
 await page.setViewportSize({width:1280,height:720});await choose(page,'calculator');
 await page.screenshot({path:artifactPath('guide-parts-desktop.png'),fullPage:true});
 await page.setViewportSize({width:390,height:844});await choose(page,'units');
 await page.screenshot({path:artifactPath('guide-parts-mobile.png'),fullPage:true});
});

test('complete guide stays readable without JavaScript',async({browser,baseURL})=>{
 const context=await browser.newContext({javaScriptEnabled:false,viewport:{width:390,height:844}});
 try {
  const page=await context.newPage();await page.goto(baseURL+'/api?section=units&lang=en');
  await expect(page.locator('#intro')).toBeVisible();await expect(page.locator('#units')).toBeVisible();
  await expect(page.locator('#http')).toBeVisible();
  expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBe(true);
 } finally {await context.close();}
});
