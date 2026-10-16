const {test,expect}=require('@playwright/test');
const {assess}=require('../../scripts/benchmark_graph_audit.cjs');
const expression='sin(x*x)+cos(1000x)',f=x=>Math.sin(x*x)+Math.cos(1000*x);
async function sample(request,body=expression,extra={}){const q={mode:'fast',dense:'band',interpolation:'hermite-subpixel',min:-2,max:2,ymin:-2,ymax:2,width:800,height:440,...extra};const response=await request.post('/api/graph/plot?'+new URLSearchParams(q),{data:body});expect(response.ok()).toBe(true);return response.json();}
test('all intermediate two-axis scales retain independent geometry without exhausting points',async({request})=>{
 for(const [width,height] of [[800,440],[320,300]])for(const span of [.3125,.625,1,1.25,2,2.5,5]){
  const d=await sample(request,expression,{width,height,min:-span,max:span,ymin:-span,ymax:span});expect(d.limited).toBe(false);expect(d.count).toBeLessThan(8000);
  const q=assess({f,width,height,min:-span,max:span,ymin:-span,ymax:span},d);expect(q.visible_coverage).toBe(1);expect(q.max_render_error_px).toBeLessThan(.55);
  if(!d.bands){expect(d.subpixel_hermite).toBe(true);expect(d.slopes).toHaveLength(d.count);}
 }
 const legacy=await sample(request,expression,{interpolation:'linear'});expect(legacy.subpixel_hermite).toBeUndefined();expect(legacy.slopes).toBeUndefined();
 const oldHermite=await sample(request,expression,{interpolation:'hermite'});expect(oldHermite.subpixel_hermite).toBeUndefined();expect(oldHermite.limited).toBe(true);
 const precise=await sample(request,'sin(x*x)',{precision:35,min:-1,max:1});expect(precise.schema_version).toBe(2);expect(precise.subpixel_hermite).toBeUndefined();
 const fallback=await sample(request,'sin((x*1E8)^2)+cos(1E11*x)',{min:-2.5e-8,max:2.5e-8,ymin:-2.5,ymax:2.5,height:1200});
 expect(fallback.subpixel_hermite).toBeUndefined();expect(fallback.limited).toBe(true);expect(Buffer.byteLength(JSON.stringify(fallback))).toBeLessThan(512*1024);
});
test('required slope policy rejects malformed or absent interpolation data',async({page})=>{
 let bad='missing';await page.route('**/api/graph/plot?**',async route=>{const r=await route.fetch(),d=await r.json();d.subpixel_hermite=true;if(bad==='missing')delete d.slopes;else d.subpixel_hermite='true';await route.fulfill({json:d});});
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('sin(x*x)');await page.locator('#graph-submit').click();await expect(page.locator('#graph-status')).toHaveClass(/error/);await expect(page.locator('path.graph-line')).toHaveCount(0);
 bad='type';await page.locator('#graph-expression').fill('cos(x*x)');await page.locator('#graph-submit').click();await expect(page.locator('#graph-status')).toHaveClass(/error/);
});
for(const width of [390,1280])test(`sequential zoom stays visible and draws accurate subpixel SVG at width ${width}`,async({page})=>{
 test.setTimeout(30000);await page.setViewportSize({width,height:844});await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill(expression);await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 for(let i=0;i<5;i++){await page.locator('#graph-plot').press('+');await page.waitForTimeout(220);await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('path.graph-band,path.graph-line').first()).toBeVisible();await expect(page.locator('#graph-status')).not.toContainText('unresolved');}
 const quality=await page.locator('#graph-plot').evaluate(svg=>{
  const w=svg.viewBox.baseVal.width,h=svg.viewBox.baseVal.height,lo=Number(svg.dataset.xmin),hi=Number(svg.dataset.xmax),yl=Number(svg.dataset.ymin),yh=Number(svg.dataset.ymax);let error=0,checks=0,cubic=false;
  for(const p of svg.querySelectorAll('path.graph-line')){let a;for(const command of p.getAttribute('d').matchAll(/([MLC])([^MLC]+)/g)){
   const n=command[2].trim().split(/[ ,]+/).map(Number),b={x:n.at(-2),y:n.at(-1)};
   if(command[1]!=='M'&&a){cubic||=command[1]==='C';for(const t of [.125,.25,.381966,.5,.75,.875]){const u=1-t;
    const px=command[1]==='C'?u**3*a.x+3*u*u*t*n[0]+3*u*t*t*n[2]+t**3*b.x:a.x*u+b.x*t;
    const py=command[1]==='C'?u**3*a.y+3*u*u*t*n[1]+3*u*t*t*n[3]+t**3*b.y:a.y*u+b.y*t;
    const x=lo+(px-64)/(w-86)*(hi-lo),y=yh-(py-22)/(h-68)*(yh-yl),v=Math.sin(x*x)+Math.cos(1000*x);
    if(x>=lo&&x<=hi&&v>=yl&&v<=yh){error=Math.max(error,Math.abs(y-v)*(h-68)/(yh-yl));checks++;}
   }}a=b;
  }}return{error,checks,cubic};
 });expect(quality.checks).toBeGreaterThan(50);expect(quality.cubic).toBe(true);expect(quality.error).toBeLessThan(.8);
 if(width===390)await expect(page.locator('path.graph-line-subpixel').first()).toHaveCSS('stroke-width','0.9px');
 const target=await page.locator('#graph-plot').evaluate(svg=>{const w=svg.viewBox.baseVal.width,h=svg.viewBox.baseVal.height,lo=Number(svg.dataset.xmin),hi=Number(svg.dataset.xmax),yl=Number(svg.dataset.ymin),yh=Number(svg.dataset.ymax),x=.005,y=Math.sin(x*x)+Math.cos(1000*x);const p=new DOMPoint(64+(x-lo)/(hi-lo)*(w-86),22+(yh-y)/(yh-yl)*(h-68)).matrixTransform(svg.getScreenCTM());return{x:p.x,y:p.y,scale:(h-68)/(yh-yl)};});
 // Dense subpixel waves require an exact coordinate: native mouse injection
 // rounds to a CSS pixel in Firefox, potentially selecting a different phase.
 await page.locator('#graph-plot').dispatchEvent('pointermove',{pointerType:'mouse',clientX:target.x,clientY:target.y});await expect(page.locator('#graph-inspector')).toBeVisible();const point=await page.locator('#graph-inspector').evaluate(e=>({x:Number(e.dataset.x),y:Number(e.dataset.y)}));expect(Math.abs(point.y-f(point.x))*target.scale).toBeLessThan(.8);
 await page.locator('.graph-result').screenshot({path:`C:/Incoming/Projekty/NumForge/build/graph-subpixel-${width}.png`});
 await page.locator('#graph-plot').press('Home');await expect(page.locator('path.graph-band')).toHaveCount(1);
});
