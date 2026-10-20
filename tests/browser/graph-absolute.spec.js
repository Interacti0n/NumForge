const { artifactPath } = require('./artifacts');
const {test,expect}=require('@playwright/test');
const {assess}=require('../../benchmarks/application/benchmark_graph_audit.cjs');
async function plot(request,expression,extra={}){
 const q={mode:'fast',dense:'band',interpolation:'hermite-subpixel',min:-100,max:100,ymin:-40,ymax:110,width:930,height:530,...extra};
 const r=await request.post('/api/graph/plot?'+new URLSearchParams(q),{data:expression});expect(r.ok()).toBe(true);return r.json();
}
test('absolute sine carriers retain actual zeros and accurate resolved curves',async({request})=>{
 for(const [expression,f,phase,offset] of [['abs(x*sin(x))',x=>Math.abs(x*Math.sin(x)),1,0],['abs((2x-3)*sin(3x+0.2))',x=>Math.abs((2*x-3)*Math.sin(3*x+.2)),3,.2]]){
  const d=await plot(request,expression);expect(d.limited,expression).toBe(false);expect(d.bands).toBeUndefined();
  for(let k=-90;k<=90;k++){const x=(k*Math.PI-offset)/phase;if(x<=-100||x>=100)continue;
   const candidates=d.points.filter(p=>Math.abs(p[0]-x)<1e-6);expect(candidates.length,`${expression} zero ${x}`).toBeGreaterThan(0);expect(Math.min(...candidates.map(p=>p[1]))).toBeLessThan(1e-6);
  }
  const a=assess({f,min:-100,max:100,ymin:-40,ymax:110,width:930,height:530},d);expect(a.visible_coverage).toBe(1);expect(a.max_render_error_px).toBeLessThan(1);
 }
});
test('shifted inner zeros and translated absolute values preserve their actual cusp height',async({request})=>{
 for(const shift of [0,1]){
  const expression=shift?'1+abs(sin(x)+0.2)':'abs(sin(x)+0.2)',d=await plot(request,expression,{ymin:shift-.2,ymax:shift+1.5});expect(d.limited).toBe(false);
  for(let k=-15;k<=15;k++)for(const phase of [Math.asin(-.2),Math.PI-Math.asin(-.2)]){
   const x=phase+2*k*Math.PI;if(x<=-100||x>=100)continue;const points=d.points.filter(p=>Math.abs(p[0]-x)<1e-6);expect(points.length).toBeGreaterThan(0);expect(Math.min(...points.map(p=>Math.abs(p[1]-shift)))).toBeLessThan(1e-6);
  }
  const a=assess({f:x=>shift+Math.abs(Math.sin(x)+.2),min:-100,max:100,ymin:shift-.2,ymax:shift+1.5,width:930,height:530},d);expect(a.visible_coverage).toBe(1);expect(a.max_render_error_px).toBeLessThan(1);
 }
});
test('square-root of a square shares real corners without projecting complex intermediates',async({request})=>{
 const q={min:-10,max:10,ymin:-1,ymax:10},d=await plot(request,'sqrt((x*sin(x))^2)',q);
 expect(d.limited).toBe(false);const a=assess({...q,width:930,height:530,f:x=>Math.abs(x*Math.sin(x))},d);expect(a.visible_coverage).toBe(1);expect(a.max_render_error_px).toBeLessThan(1);
 const guarded=await plot(request,'sqrt((sqrt(x))^2)',q);expect(guarded.bands).toBeUndefined();
 for(const p of guarded.points){if(p[0]<0)expect(p[3]).not.toBe(0);else if(p[1]!==null)expect(Math.abs(p[1]-Math.sqrt(p[0]))).toBeLessThan(1e-8);}
});
test('absolute dense ranges reach zero and enclose independent values without changing ordinary periods',async({request})=>{
 test.setTimeout(30000); // Several independent requests retain their own 5 s budgets.
 for(const width of [320,930]){
  const d=await plot(request,'abs(x*sin(x))',{min:-340,max:3890,ymin:-1500,ymax:2700,width});expect(d.limited).toBe(false);expect(d.bands).toHaveLength(width);
  for(const [a,b,lo,hi] of d.bands){expect(lo).toBe(0);expect(hi).toBeGreaterThanOrEqual(Math.max(Math.abs(a),Math.abs(b)));for(let j=0;j<=8;j++){const x=a+(b-a)*(j+.381966)/9;expect(Math.abs(x*Math.sin(x))).toBeLessThanOrEqual(hi);}}
 }
 const plain=await plot(request,'x*sin(x)',{min:-340,max:3890});expect(plain.bands).toBeUndefined();
 for(const expression of ['abs(1/x)','abs(sin(x)/(x-1))']){const d=await plot(request,expression,{min:-10,max:10});expect(d.bands).toBeUndefined();}
});
for(const width of [390,1280])test(`absolute graph transitions between actual valleys and dense ranges at ${width}`,async({page})=>{
 await page.setViewportSize({width,height:900});await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('abs(x*sin(x))');await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toBeVisible();
 for(let i=0;i<8;i++)await page.locator('#graph-plot').dispatchEvent('keydown',{key:'-'});
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-2560');await expect(page.locator('path.graph-band')).toBeVisible();await expect(page.locator('#graph-status')).toContainText('approximate range');
 await page.locator('.graph-result').screenshot({path:artifactPath(`graph-absolute-${width}.png`)});
 await page.locator('#graph-reset').click();await expect(page.locator('path.graph-band')).toHaveCount(0);await expect(page.locator('path.graph-line')).toBeVisible();
});
