const {test,expect}=require('@playwright/test');
const {artifactPath}=require('./artifacts');

for(const lang of ['sk','en'])test(`precision modes preserve settings geometry ${lang}`,async({page})=>{
 await page.goto('/?lang='+lang);
 for(const [width,height] of [[390,844],[981,768],[1024,768],[1280,720],[1440,900]]){
  await page.setViewportSize({width,height});
  await page.locator('#precision-mode').selectOption('auto');
  const geometry=()=>page.locator('.precision,.expression-card').evaluateAll(nodes=>nodes.map(e=>{
   const r=e.getBoundingClientRect();return{x:r.x,y:r.y,width:r.width,height:r.height};
  }));
  const initial=await geometry();
  await expect(page.locator('#custom-precision')).toBeHidden();
  await expect(page.locator('#precision')).toBeDisabled();
  for(const mode of ['custom','full','auto']){
   await page.locator('#precision-mode').selectOption(mode);
   expect(await geometry()).toEqual(initial);
   if(width>960){
    const row=await page.locator('#settings-controls').evaluate(e=>[...e.querySelectorAll('select,input,.angle-switch')]
     .filter(n=>getComputedStyle(n).visibility!=='hidden'&&!n.closest('[hidden]'))
     .map(n=>n.getBoundingClientRect().y));
    expect(Math.max(...row)-Math.min(...row)).toBeLessThan(1);
    expect((await page.locator('.precision').boundingBox()).height).toBeLessThanOrEqual(46);
   }
   if(mode==='custom'){
    await expect(page.locator('#precision')).toBeEnabled();
    await page.locator('#precision').fill('2');
   }else await expect(page.locator('#custom-precision')).toBeHidden();
  }
  expect(await page.evaluate(()=>document.documentElement.scrollWidth)).toBeLessThanOrEqual(width);
  if(width>960)expect(await page.evaluate(()=>document.documentElement.scrollHeight)).toBe(height);
 }
 await page.locator('#precision-mode').selectOption('custom');
 await page.locator('#expression').fill('1/8');await page.locator('#expression').press('Enter');
 await expect(page.locator('#result')).toHaveText('0.12');
 await page.locator('.precision').screenshot({path:artifactPath('settings-stable-'+lang+'.png')});
});
