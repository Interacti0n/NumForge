const { artifactPath } = require('./artifacts');
const {test,expect}=require('@playwright/test');
const {assess}=require('../../benchmarks/application/benchmark_graph_audit.cjs');
const defaults={mode:'fast',width:800,height:440,min:-10.001,max:9.999,ymin:-10,ymax:10,interpolation:'hermite',dense:'band'};
async function sample(request,expression,extra={}){const response=await request.post('/api/graph/plot?'+new URLSearchParams({...defaults,...extra}),{data:expression});expect(response.ok(),expression).toBe(true);return response.json();}
test('mixed ranges enclose independent values through phase and amplitude turns and resolve on zoom',async({request})=>{
 for(const [expression,f] of [
  ['sin(x*x)+cos(1000x)',x=>Math.sin(x*x)+Math.cos(1000*x)],
  ['cos(1000x)+sin(x)',x=>Math.cos(1000*x)+Math.sin(x)],
  ['-(x-0.1234)*cos(x*x)+2*cos(1000x)+x/10',x=>-(x-.1234)*Math.cos(x*x)+2*Math.cos(1000*x)+x/10],
  ['cos(1000x)+sin(-1000*(x-0.1234)^2)',x=>Math.cos(1000*x)+Math.sin(-1000*(x-.1234)**2)],
  ['sin(x)+cos(x*x)+cos(1000x)',x=>Math.sin(x)+Math.cos(x*x)+Math.cos(1000*x)]
 ])for(const [width,height] of [[800,440],[320,300]]){
  const d=await sample(request,expression,{width,height});expect(d.limited).toBe(false);expect(d.bands).toHaveLength(width);expect(d.count).toBeLessThanOrEqual(37);expect(d.points.every(p=>!p[2])).toBe(true);
  const bad=[];for(const [a,b,lo,hi] of d.bands)for(let j=0;j<=64;j++){const y=f(a+(b-a)*j/64);if(y<lo-1e-9||y>hi+1e-9)bad.push(y);}expect(bad,expression).toEqual([]);
  const z=await sample(request,expression,{width,height,min:.1224,max:.1244});expect(z.bands).toBeUndefined();expect(z.limited).toBe(false);
  const quality=assess({f,width,height,min:.1224,max:.1244,ymin:-10,ymax:10},z);expect(quality.visible_coverage).toBe(1);expect(quality.max_render_error_px).toBeLessThan(.1);
 }
});
test('slow trig extrema give tight ranges, degrees and unresolved non-dense sums keep their contracts',async({request})=>{
 for(const expression of ['cos(1000x)+sin(x)','cos(1000x)+sin(x+π/2)']){
  const d=await sample(request,expression);expect(d.bands.every(b=>b[3]-b[2]<2.026)).toBe(true);
  const x=expression.includes('π')?0:Math.PI/2,band=d.bands.find(b=>b[0]<=x&&b[1]>=x);expect(band[3]).toBeGreaterThanOrEqual(2);
 }
 const degrees=await sample(request,'cos(360000x)+sin(10x)',{angle:'deg'});expect(degrees.bands).toHaveLength(800);
 const bad=[];for(const [a,b,lo,hi] of degrees.bands)for(let j=0;j<=32;j++){const x=a+(b-a)*j/32,y=Math.cos(2000*Math.PI*x)+Math.sin(Math.PI*x/18);if(y<lo-1e-9||y>hi+1e-9)bad.push(y);}expect(bad).toEqual([]);
 const slow=await sample(request,'sin(x)+cos(x*x)',{min:-1,max:1});expect(slow.bands).toBeUndefined();expect(slow.limited).toBe(false);
 const precise=await sample(request,'sin(x*x)+cos(1000x)',{precision:35,min:-.001,max:.001});expect(precise.schema_version).toBe(2);expect(precise.bands).toBeUndefined();
});
for(const width of [390,1280])test(`mixed range becomes an inspected curve after zoom at width ${width}`,async({page})=>{
 await page.setViewportSize({width,height:844});await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('sin(x*x)+cos(1000x)');await page.locator('#graph-submit').click();
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('path.graph-band')).toHaveCount(1);await expect(page.locator('path.graph-line')).toHaveCount(0);await expect(page.locator('#graph-status')).not.toContainText('unresolved');
 await page.locator('.graph-result').screenshot({path:artifactPath(`graph-mixed-${width}.png`)});
 await page.locator('#graph-plot').evaluate(svg=>{for(let i=0;i<5;i++)svg.dispatchEvent(new KeyboardEvent('keydown',{key:'+',bubbles:true,cancelable:true}));});
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('path.graph-band')).toHaveCount(0);await expect(page.locator('path.graph-line')).toHaveCount(1);
 await page.locator('#graph-plot').press('Home');await expect(page.locator('path.graph-band')).toHaveCount(1);
});
