const {test,expect}=require('@playwright/test');
function settled(page){
 const active=new Set();let last=Date.now();
 page.on('request',r=>{if(r.url().includes('/api/graph/plot?')){active.add(r);last=Date.now();}});
 const done=r=>{if(active.delete(r))last=Date.now();};page.on('requestfinished',done);page.on('requestfailed',done);
 return async()=>{await page.waitForTimeout(250);await expect.poll(()=>!active.size&&Date.now()-last>400).toBe(true);await page.evaluate(()=>new Promise(requestAnimationFrame));};
}
async function centerPlateaus(page){
 return page.locator('#graph-plot').evaluate(svg=>{
  const w=svg.viewBox.baseVal.width,h=svg.viewBox.baseVal.height,low=Number(svg.dataset.xmin),high=Number(svg.dataset.xmax),bottom=Number(svg.dataset.ymin),top=Number(svg.dataset.ymax);
  return [-.375,.375].map(x=>{const sx=64+(x-low)/(high-low)*(w-86),sy=h-46-(Math.floor(x)-bottom)/(top-bottom)*(h-68);
   return [...svg.querySelectorAll('path.graph-line')].some(path=>{
    const rect=svg.querySelector(path.getAttribute('clip-path').slice(4,-1)+' rect');
    if(!rect||sx<Number(rect.getAttribute('x'))||sx>Number(rect.getAttribute('x'))+Number(rect.getAttribute('width')))return false;
    let previous=null;for(const command of path.getAttribute('d').match(/[MLC][^MLC]*/g)||[]){
     const values=command.slice(1).split(',').map(Number),point=values.slice(-2);
     if(command[0]!=='M'&&previous&&previous[0]<=sx&&sx<=point[0]&&Math.abs(previous[1]-sy)<.01&&Math.abs(point[1]-sy)<.01)return true;
     previous=point;
    }return false;
   });
  });
 });
}
for(const width of [390,1280])test(`limited precision zoom completes and retains central step geometry at width ${width}`,async({page})=>{
 const idle=settled(page);await diagnostics(page);await page.setViewportSize({width,height:900});await page.goto('/graph?lang=en');
 await page.locator('#graph-expression').fill('floor(x)');await page.locator('#graph-submit').click();await idle();
 for(let i=0;i<3;i++){
  await page.locator('#graph-plot').press('-');await idle();
  await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
  await expect(page.locator('#graph-status')).not.toContainText('Sampling');
  expect(await centerPlateaus(page)).toEqual([true,true]);
  const [cache]=await page.evaluate(()=>window.__graphReconcileTest());expect(cache.unique).toBe(cache.ranges);expect(cache.records).toBeLessThanOrEqual(12288);expect(cache.pendingView).toBe(false);
 }
 await page.locator('#graph-reset').click();await idle();expect(await centerPlateaus(page)).toEqual([true,true]);
});

test('editing while a full-view repair is held cannot publish stale cache or status',async({page})=>{
 const idle=settled(page);await diagnostics(page);let release,waiting=false;const held=new Promise(resolve=>release=resolve);
 await page.route('**/api/graph/plot?**',async route=>{
  if(route.request().postData()!=='x/100')return route.continue();
  const q=new URL(route.request().url()).searchParams,low=Number(q.get('min')),high=Number(q.get('max'));
  if(low===-20&&high===20){waiting=true;await held;}
  const count=low===-10&&high===10?2048:8192,points=Array.from({length:count},(_,i)=>{const x=low+(high-low)*i/(count-1);return [x,x/100,i?1:0,0,1];});
  try{await route.fulfill({json:{ok:true,schema_version:3,evaluation:'double',parameter:'x',count,limited:true,quality:q.get('quality'),width:Number(q.get('width')),height:Number(q.get('height')),points}});}catch(_){}
 });
 try{
  await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('x/100');await page.locator('#graph-submit').click();await idle();
  await page.locator('#graph-plot').press('-');await expect.poll(()=>waiting).toBe(true);
  await page.locator('#graph-expression').fill('2*x');await page.locator('#graph-submit').click();release();await idle();
  await expect(page.locator('#graph-expression')).toHaveValue('2*x');await expect(page.locator('#graph-status')).not.toHaveClass(/error/);await expect(page.locator('#graph-status')).not.toContainText('Sampling');
  const error=await page.locator('#graph-plot').evaluate(svg=>{
   const w=svg.viewBox.baseVal.width,h=svg.viewBox.baseVal.height,low=Number(svg.dataset.xmin),high=Number(svg.dataset.xmax),bottom=Number(svg.dataset.ymin),top=Number(svg.dataset.ymax);let worst=0,count=0;
   for(const path of svg.querySelectorAll('path.graph-line'))for(const command of path.getAttribute('d').match(/[MLC][^MLC]*/g)||[]){const a=command.slice(1).split(',').map(Number);
    for(let i=0;i<a.length;i+=2){const x=low+(a[i]-64)/(w-86)*(high-low),y=top-(a[i+1]-22)/(h-68)*(top-bottom);worst=Math.max(worst,Math.abs(y-2*x));count++;}}
   return count?worst:Infinity;
  });expect(error).toBeLessThan(1e-6);
  const [cache]=await page.evaluate(()=>window.__graphReconcileTest());expect(cache.pendingView).toBe(false);expect(cache.busy).toBe(false);
 }finally{release();}
});

async function diagnostics(page){
 await page.route('**/assets/graph.js',async route=>{
  const response=await route.fetch();let source=await response.text();
  const replace=(a,b)=>{expect(source.split(a)).toHaveLength(2);source=source.replace(a,b);};
  replace('state:()=>({visible,samples,','testCache:()=>({ranges:cache.length,unique:new Set(cache).size,records:cache.reduce((n,e)=>n+e.points.length+e.bands.length,0),pendingView,busy}),state:()=>({visible,samples,');
  replace('loadLibrary();draw();','loadLibrary();draw();window.__graphReconcileTest=()=>curves.map(c=>c.testCache());');
  replace('return ()=>{save();','return ()=>{delete window.__graphReconcileTest;save();');
  await route.fulfill({response,body:source});
 });
}
for(const fail of [false,true])test(`cache pressure reconciles once and finishes in a terminal state: repair failure ${fail}`,async({page})=>{
 const idle=settled(page),calls=[];await diagnostics(page);
 await page.route('**/api/graph/plot?**',async route=>{
  const q=new URL(route.request().url()).searchParams,low=Number(q.get('min')),high=Number(q.get('max'));
  calls.push({low,high,quality:q.get('quality')});
  if(fail&&low===-20&&high===20)return route.fulfill({status:503,json:{ok:false,error:'Repair failed'}});
  const count=low===-10&&high===10?2048:8192,points=Array.from({length:count},(_,i)=>{const x=low+(high-low)*i/(count-1);return [x,x/100,i?1:0,0,1];});
  await route.fulfill({json:{ok:true,schema_version:3,evaluation:'double',parameter:'x',count,limited:true,quality:q.get('quality'),width:Number(q.get('width')),height:Number(q.get('height')),points}});
 });
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('x/100');await page.locator('#graph-submit').click();await idle();
 await page.locator('#graph-plot').press('-');await idle();
 expect(calls.filter(c=>c.low===-20&&c.high===20&&c.quality==='refined'),JSON.stringify({calls,status:await page.locator('#graph-status').textContent(),stats:await page.evaluate(()=>window.__graphReconcileTest())})).toHaveLength(1);
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 await expect(page.locator('#graph-status')).not.toContainText('Sampling');
 if(fail){await expect(page.locator('#graph-status')).toHaveClass(/error/);await expect(page.locator('#graph-status')).toContainText('Repair failed');}
 else {await expect(page.locator('#graph-status')).not.toHaveClass(/error/);await expect(page.locator('path.graph-line')).toBeVisible();}
 const [state]=await page.evaluate(()=>window.__graphReconcileTest());
 expect(state.pendingView).toBe(false);expect(state.busy).toBe(false);expect(state.ranges).toBe(state.unique);expect(state.records).toBeLessThanOrEqual(12288);expect(state.ranges).toBeLessThanOrEqual(12);
 const finished=calls.length;await page.waitForTimeout(300);expect(calls).toHaveLength(finished);
});
