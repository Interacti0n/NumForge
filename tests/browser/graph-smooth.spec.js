const {test,expect}=require('@playwright/test');
const fs=require('node:fs');
async function plot(request,expression,extra={}){
 const response=await request.post('/api/graph/plot?'+new URLSearchParams({mode:'fast',min:'-10',max:'10',ymin:'-10',ymax:'10',width:'800',height:'440',interpolation:'hermite',...extra}),{data:expression});
 expect(response.ok()).toBe(true);const bytes=await response.body();expect(bytes.length).toBeLessThan(512*1024);return JSON.parse(bytes);
}
test('C slopes and stationary samples agree with independent analytic references',async({request})=>{
 for(const [expression,f,df,peaks] of [
  ['x^3-3x',x=>x*x*x-3*x,x=>3*x*x-3,[-1,1]],
  ['exp(-(x-0.37)^2)',x=>Math.exp(-((x-.37)**2)),x=>-2*(x-.37)*Math.exp(-((x-.37)**2)),[.37]],
  ['sin(x)+x/5',x=>Math.sin(x)+x/5,x=>Math.cos(x)+.2,[Math.acos(-.2)]],
  ['sin(3x)+cos(2x)',x=>Math.sin(3*x)+Math.cos(2*x),x=>3*Math.cos(3*x)-2*Math.sin(2*x),[]]]){
  const data=await plot(request,expression);expect(data.schema_version).toBe(3);expect(data.limited,expression).toBe(false);
  expect(data.slopes).toHaveLength(data.count);
  for(let i=0;i<data.count;++i){const p=data.points[i];expect(p).toHaveLength(5);
   expect(Math.abs(p[1]-f(p[0]))/Math.max(1,Math.abs(f(p[0]))),expression).toBeLessThan(1e-9);
   expect(Math.abs(data.slopes[i]-df(p[0]))/Math.max(1,Math.abs(df(p[0]))),expression).toBeLessThan(1e-8);
   if(i)expect(p[2],expression).toBe(1);
  }
  for(const peak of peaks)expect(Math.min(...data.points.map(p=>Math.abs(p[0]-peak))),expression).toBeLessThan(1e-5);
 }
});
test('optional slopes preserve poles, undefined slopes, response bounds and fallback schemas',async({request})=>{
 for(const [expression,pole] of [['1/x',0],['1/(x-0.1)^2',.1],['tan(x)',Math.PI/2]]){
  const data=await plot(request,expression);expect(data.slopes).toHaveLength(data.count);
  data.points.forEach((p,i)=>{if(i&&p[2])expect(data.points[i-1][0]<pole&&p[0]>pole).toBe(false);});
 }
 const corner=await plot(request,'abs(x)');const index=corner.points.findIndex(p=>p[0]===0);
 expect(corner.points[index][1]).toBe(0);expect(corner.slopes[index]).toBeNull();
 const linear=await plot(request,'sin(x)',{interpolation:'linear'});expect(linear.slopes).toBeUndefined();
 const fallback=await plot(request,'floor(x)');expect(fallback.schema_version).toBe(2);expect(fallback.slopes).toBeUndefined();
 const dense=await plot(request,'sin(100x)',{min:'-30',max:'30',width:'2048'});expect(dense.count).toBeLessThanOrEqual(8192);expect(dense.limited).toBe(false);
 expect(dense.slopes===undefined||dense.slopes.length===dense.count).toBe(true);
});
for(const width of [390,1280])test(`rendered smooth curves follow analytic extrema without overshoot at width ${width}`,async({page})=>{
 await page.setViewportSize({width,height:900});await page.goto('/graph?lang=en');
 for(const expression of ['sin(x)','exp(-(x-0.37)^2)','x^3-3x']){
  await page.locator('#graph-expression').fill(expression);await page.locator('#graph-submit').click();
  await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
  await expect(page.locator('path.graph-line')).toBeVisible();
  const result=await page.locator('#graph-plot').evaluate((svg,expression)=>{
   const w=svg.viewBox.baseVal.width,h=svg.viewBox.baseVal.height,errors=[];
   const f=expression==='sin(x)'?Math.sin:expression.startsWith('exp')?x=>Math.exp(-((x-.37)**2)):x=>x*x*x-3*x;
   let cubic=false;
   for(const path of svg.querySelectorAll('path.graph-line')){
    cubic=cubic||path.getAttribute('d').includes('C');const length=path.getTotalLength();
    for(let i=0;i<=1500;++i){const p=path.getPointAtLength(length*i/1500),x=-10+(p.x-64)/(w-86)*20,y=10-(p.y-22)/(h-68)*20;
     if(x>=-10&&x<=10&&Math.abs(f(x))<=10)errors.push(Math.abs(y-f(x))*(h-68)/20);
    }
   }
   return {cubic,maxError:Math.max(...errors)};
  },expression);
  expect(result.cubic,expression).toBe(true);expect(result.maxError,expression).toBeLessThan(.15);
 }
 await page.locator('.graph-result').screenshot({path:`C:/Incoming/Projekty/NumForge/build/graph-smooth-${width}.png`});
});
test('next zoom is prepared before interaction and redraws from the cache',async({page})=>{
 await page.goto('/graph?lang=en');const calls=[];
 page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))calls.push(new URL(r.url()).searchParams);});
 const prepared=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&new URL(r.url()).searchParams.get('min')==='-10'&&new URL(r.url()).searchParams.get('ymin')==='-5');
 await page.locator('#graph-submit').click();const q=new URL((await prepared).url()).searchParams;
 expect(Number(q.get('width'))).toBeGreaterThanOrEqual(640);
 await page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));
 const before=calls.length;
 const ms=await page.evaluate(async()=>{const start=performance.now();document.querySelector('#graph-zoom-in').click();await new Promise(r=>requestAnimationFrame(r));return performance.now()-start;});
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-5');
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 expect(await page.locator('path.graph-line').getAttribute('d')).toContain('C');
 await page.waitForTimeout(350);
 expect(calls.slice(before).some(q=>q.get('min')==='-5'&&q.get('max')==='5'&&q.get('ymin')==='-5'&&q.get('ymax')==='5')).toBe(false);
 fs.writeFileSync('C:/Incoming/Projekty/NumForge/build/graph-smooth-zoom-frame.json',JSON.stringify({event_to_animation_frame_ms:ms}));
});
test('missing slope data draws linear fallback and malformed slope data is rejected',async({page})=>{
 let malformed=false;
 await page.route('**/api/graph/plot?**',async route=>{const response=await route.fetch(),json=await response.json();
  if(malformed)json.slopes=[0];else delete json.slopes;
  await route.fulfill({json});
 });
 await page.goto('/graph?lang=en');await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toBeVisible();
 expect(await page.locator('path.graph-line').getAttribute('d')).not.toContain('C');
 malformed=true;await page.locator('#graph-expression').fill('cos(x)');await page.locator('#graph-submit').click();
 await expect(page.locator('#graph-status')).toHaveClass(/error/);await expect(page.locator('path.graph-line')).toHaveCount(0);
});
