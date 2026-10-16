const {test,expect}=require('@playwright/test');
const field=(page,i)=>page.locator('.graph-curve').nth(i).locator('input:not([type=color])');
async function navigate(page,path,lang='sk'){
 const toggle=page.locator('.nav-toggle');if(await toggle.isVisible())await toggle.click();
 await page.locator(`.primary-nav a[href="${path}?lang=${lang}"]`).click();
}
for(const width of [390,1280]){
 test(`all function rows share typography and color control styling at ${width}`,async({page})=>{
  await page.setViewportSize({width,height:900});await page.goto('/graph?lang=sk');
  for(let i=1;i<4;i++)await page.locator('#graph-add').click();
  const styles=await page.locator('.graph-curve').evaluateAll(rows=>rows.map(row=>{
   const get=(selector,props)=>{const s=getComputedStyle(row.querySelector(selector));return props.map(p=>s[p]);};
   return {input:get('input:not([type=color])',['fontFamily','fontSize','fontWeight','lineHeight','height']),color:get('input[type=color]',['width','height','padding','border','borderRadius','backgroundColor'])};
  }));
  for(const style of styles)expect(style).toEqual(styles[0]);
  expect(styles[0].color.slice(0,2)).toEqual(['38px','44px']);
  await page.locator('.graph-remove').first().click();
  await expect(field(page,0)).toHaveCSS('font-size',styles[0].input[1]);
  await expect(page.locator('#graph-color')).toHaveCSS('width','38px');
 });
 test(`completed graphs and camera survive language and calculator navigation at ${width}`,async({page})=>{
  await page.setViewportSize({width,height:900});await page.goto('/graph?lang=en');
  await page.locator('#graph-add').click();await field(page,1).fill('cos(x)');
  await page.locator('.graph-curve input[type=color]').last().evaluate(e=>{e.value='#ff2233';e.dispatchEvent(new Event('input',{bubbles:true}));});
  await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toHaveCount(2);
  const svg=page.locator('#graph-plot');await svg.press('+');await svg.press('ArrowRight');await svg.press('ArrowUp');
  await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await page.waitForTimeout(700);
  const camera=await svg.evaluate(e=>[e.dataset.xmin,e.dataset.xmax,e.dataset.ymin,e.dataset.ymax]);
  let calls=0;page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))++calls;});
  if(await page.locator('.nav-toggle').isVisible())await page.locator('.nav-toggle').click();
  await page.locator('.language-switch a[lang=sk]').click();await expect(page.locator('path.graph-line')).toHaveCount(2);
  expect(await svg.evaluate(e=>[e.dataset.xmin,e.dataset.xmax,e.dataset.ymin,e.dataset.ymax])).toEqual(camera);
  await expect(page.locator('.graph-curve input[type=color]').last()).toHaveValue('#ff2233');
  await page.locator('.graph-visibility').last().click();await expect(page.locator('path.graph-line')).toHaveCount(1);
  await navigate(page,'/');await expect(page.locator('#expression')).toBeVisible();
  await page.locator('#expression').fill('2+2');await page.locator('#expression').press('Enter');await expect(page.locator('#result')).toHaveText('4');
  await navigate(page,'/graph');await expect(page.locator('path.graph-line')).toHaveCount(1);
  await expect(page.locator('.graph-visibility').last()).toHaveAttribute('aria-pressed','false');
  await page.locator('.graph-visibility').last().click();await expect(page.locator('path.graph-line')).toHaveCount(2);
  expect(await svg.evaluate(e=>[e.dataset.xmin,e.dataset.xmax,e.dataset.ymin,e.dataset.ymax])).toEqual(camera);
  expect(calls).toBe(0);
  await page.reload();await expect(page.locator('path.graph-line')).toHaveCount(2);
  expect(await svg.evaluate(e=>[e.dataset.xmin,e.dataset.xmax,e.dataset.ymin,e.dataset.ymax])).toEqual(camera);
 });
}
test('legacy and invalid saved cameras cannot replace the safe default',async({page})=>{
 await page.addInitScript(()=>sessionStorage.setItem('numforge-graph-state',JSON.stringify({version:2,view:{x:[1,1],y:[-10,10]},curves:[{expression:'x',color:'#dfafff'}]})));
 await page.goto('/graph?lang=en');await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');await expect(page.locator('path.graph-line')).toHaveCount(0);
});
test('edited input stays unsubmitted and cannot restore an older curve',async({page})=>{
 await page.goto('/graph?lang=sk');await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toHaveCount(1);
 await field(page,0).fill('cos(x)');await navigate(page,'/');await navigate(page,'/graph');
 await expect(field(page,0)).toHaveValue('cos(x)');await expect(page.locator('path.graph-line')).toHaveCount(0);
 await page.reload();await expect(page.locator('path.graph-line')).toHaveCount(0);await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
});
test('returning to a larger canvas retains the camera and refines restored curves',async({page})=>{
 await page.setViewportSize({width:390,height:844});await page.goto('/graph?lang=sk');await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toHaveCount(1);
 await page.waitForTimeout(400);await navigate(page,'/');await page.setViewportSize({width:1280,height:900});await navigate(page,'/graph');
 await expect(page.locator('path.graph-line')).toHaveCount(1);await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymin','-10');
});
