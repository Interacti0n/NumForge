const {test,expect}=require('@playwright/test');
const fs=require('node:fs');
const {artifactPath}=require('./artifacts');
const workspace=()=>({format:'numforge.graph-workspace',version:1,view:{x:[-3,7],y:[-2,4]},curves:[
 {expression:'sin(x)',color:'#ff1234',visible:true},{expression:'cos(x)',color:'#00ff00',visible:false}]});
const load=(page,data)=>page.locator('#graph-file').setInputFiles({name:'graph.json',mimeType:'application/json',buffer:Buffer.from(typeof data==='string'?data:JSON.stringify(data))});
async function download(page,selector){
 const pending=page.waitForEvent('download');await page.locator(selector).click();const file=await pending;
 const stream=await file.createReadStream(),chunks=[];for await(const chunk of stream)chunks.push(chunk);return Buffer.concat(chunks);
}
for(const lang of ['sk','en'])test(`graph workspace files preserve camera, colors, visibility and drafts ${lang}`,async({page})=>{
 await page.goto('/graph?lang='+lang);let requests=0;page.on('request',r=>{if(r.url().includes('/api/graph/'))++requests;});
 const data=workspace();data.curves.push({expression:'',color:'#aabbcc',visible:true},{expression:'x^2',color:'#123abc',visible:false});await load(page,data);
 await expect(page.locator('#graph-workspace-status')).toContainText(lang==='sk'?'Úloha načítaná':'Workspace loaded');
 expect(requests).toBe(0);await expect(page.locator('path.graph-line')).toHaveCount(0);
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-3');await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymax','4');
 await expect(page.locator('.graph-visibility').nth(1)).toHaveAttribute('aria-pressed','false');
 await expect(page.locator('#graph-color')).toHaveValue('#ff1234');
 expect(JSON.parse((await download(page,'#graph-save')).toString())).toEqual(data);
 await page.locator('#graph-plot').press('+');await page.waitForTimeout(400);expect(requests).toBe(0);
 await page.reload();await expect(page.locator('.graph-curve')).toHaveCount(4);
 await expect(page.locator('path.graph-line')).toHaveCount(0);
 await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toHaveCount(1);
 await expect(page.locator('#graph-history-list .graph-history-restore')).toHaveCount(1);
 const current=await page.locator('#graph-expression').inputValue();
 const invalid=[{...workspace(),version:99},{...workspace(),view:{x:[2,1],y:[-1,1]}},{...workspace(),curves:[{expression:'x\n+1',color:'#ffffff',visible:true}]},
  {...workspace(),curves:[{expression:'π'.repeat(3000),color:'#ffffff',visible:true}]},{...workspace(),curves:[]},{...workspace(),curves:Array(5).fill(workspace().curves[0])},
  {...workspace(),curves:[{expression:'x',color:'red',visible:true}]},{...workspace(),curves:[{expression:'x',color:'#ffffff',visible:'true'}]},'{bad','x'.repeat(100001)];
 for(const value of invalid){await load(page,value);await expect(page.locator('#graph-workspace-status')).toHaveClass(/error/);await expect(page.locator('#graph-expression')).toHaveValue(current);}
 await page.screenshot({path:artifactPath('graph-files-'+lang+'.png'),fullPage:true});
});
test('exported SVG/PNG are standalone, styled, bounded and available offline',async({page})=>{
 await page.goto('/graph?lang=en');await load(page,workspace());await page.locator('#graph-submit').click();
 await expect(page.locator('#graph-export-svg')).toBeEnabled();await expect(page.locator('#graph-export-png')).toBeEnabled();
 await page.locator('#graph-color').evaluate(e=>{e.value='#ff5678';e.dispatchEvent(new Event('input',{bubbles:true}));});
 await expect(page.locator('#graph-export-svg')).toBeEnabled();
 const path=await page.locator('path.graph-line').getAttribute('d');await page.context().setOffline(true);
 const history=await page.locator('.graph-history-restore').count(),camera=await page.locator('#graph-plot').getAttribute('data-xmin');
 await page.locator('#graph-export-menu > summary').click();const svg=await download(page,'#graph-export-svg');
 const xml=svg.toString();expect(xml).toContain('Approximate graph');expect(xml).toContain('sin(x)');expect(xml).not.toContain('cos(x)');
 expect(xml).not.toContain('graph-inspection');expect(xml).not.toContain('http://127.0.0.1');expect(xml).not.toContain('var(');
 expect(xml).toContain(path);expect(xml).toContain('rgb(255, 86, 120)');
 expect(await page.evaluate(text=>new DOMParser().parseFromString(text,'image/svg+xml').querySelector('parsererror')!==null,xml)).toBe(false);
 fs.writeFileSync(artifactPath('graph-export.svg'),svg);
 await page.locator('#graph-export-menu > summary').click();const png=await download(page,'#graph-export-png');
 expect(png.subarray(0,8).toString('hex')).toBe('89504e470d0a1a0a');
 expect(png.readUInt32BE(16)).toBeLessThanOrEqual(2048);expect(png.readUInt32BE(20)).toBeLessThanOrEqual(2048);expect(png.length).toBeGreaterThan(1000);
 const pixels=await page.evaluate(async data=>{
  const image=await createImageBitmap(new Blob([Uint8Array.from(data)],{type:'image/png'}));const canvas=document.createElement('canvas');canvas.width=image.width;canvas.height=image.height;
  const ctx=canvas.getContext('2d');ctx.drawImage(image,0,0);const rgba=ctx.getImageData(0,0,canvas.width,canvas.height).data;let count=0;
  for(let i=0;i<rgba.length;i+=4)if(rgba[i]===255&&rgba[i+1]===86&&rgba[i+2]===120)++count;image.close();return count;
 },[...png]);expect(pixels).toBeGreaterThan(10);fs.writeFileSync(artifactPath('graph-export.png'),png);
 await expect(page.locator('.graph-history-restore')).toHaveCount(history);await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin',camera);
 await page.context().setOffline(false);
});
test('failed visible functions block export until hidden and explicit locate can calculate an imported draft',async({page})=>{
 await page.goto('/graph?lang=en');const data=workspace();data.curves[0].expression='x+20';await load(page,data);
 await page.locator('#graph-locate').click();await expect(page.locator('path.graph-line')).toHaveCount(1);
 await expect(page.locator('#graph-export-svg')).toBeEnabled();
 const second=page.locator('.graph-curve').nth(1);await second.locator('input:not([type=color])').fill('unknown(x)');
 await second.locator('.graph-visibility').click();await page.locator('#graph-submit').click();
 await expect(page.locator('#graph-status')).toHaveClass(/error/);await expect(page.locator('#graph-export-svg')).toBeDisabled();
 await second.locator('.graph-visibility').click();await expect(page.locator('#graph-export-svg')).toBeEnabled();
});
test('workspace import cancels pending plots and never resumes imported drafts on reload',async({page})=>{
 await page.goto('/graph?lang=en');let release;const gate=new Promise(r=>release=r);
 await page.route('**/api/graph/plot?*',async route=>{await gate;await route.abort().catch(()=>{});});
 await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).toHaveAttribute('aria-busy','true');
 const data=workspace();data.curves[0].expression='x+2';await load(page,data);release();
 await expect(page.locator('#graph-expression')).toHaveValue('x+2');await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 await expect(page.locator('#graph-export-svg')).toBeDisabled();await expect(page.locator('path.graph-line')).toHaveCount(0);
 await page.unroute('**/api/graph/plot?*');await page.reload();await expect(page.locator('path.graph-line')).toHaveCount(0);
 await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toHaveCount(1);
});
test('history marks the current set and removes one entry without changing the current graph',async({page})=>{
 await page.goto('/graph?lang=en');await page.locator('#graph-submit').click();
 await page.locator('#graph-expression').fill('x');await page.locator('#graph-submit').click();
 await expect(page.locator('.graph-history-restore')).toHaveCount(2);
 await expect(page.locator('.graph-history-restore').first()).toHaveAttribute('aria-current','true');
 await page.locator('.graph-history-remove').last().click();await expect(page.locator('.graph-history-restore')).toHaveCount(1);
 await expect(page.locator('#graph-expression')).toHaveValue('x');await expect(page.locator('path.graph-line')).toHaveCount(1);
 await page.locator('.graph-history-remove').click();await expect(page.locator('.graph-history-restore')).toHaveCount(0);
 await expect(page.locator('#graph-save')).toBeFocused();await expect(page.locator('path.graph-line')).toHaveCount(1);
});
test('graph file actions survive SPA navigation and fit a touch phone',async({browser,baseURL})=>{
 const context=await browser.newContext({baseURL,viewport:{width:390,height:844},isMobile:true,hasTouch:true});
 try{
  const page=await context.newPage();await page.goto('/?lang=sk');await page.locator('.nav-toggle').click();await page.locator('.primary-nav a[href="/graph?lang=sk"]').click();
  await load(page,workspace());await page.locator('#graph-submit').click();await expect(page.locator('#graph-export-svg')).toBeEnabled();
  await page.locator('#graph-export-menu > summary').click();await expect(page.locator('#graph-export-svg')).toBeInViewport();
  expect(await page.evaluate(()=>document.documentElement.scrollWidth)).toBeLessThanOrEqual(390);
  for(const item of ['#graph-export-svg','#graph-export-png','#graph-save','#graph-load'])expect((await page.locator(item).boundingBox()).height).toBeGreaterThanOrEqual(44);
  await page.keyboard.press('Escape');await expect(page.locator('#graph-export-menu')).not.toHaveAttribute('open','');
 }finally{await context.close();}
});
