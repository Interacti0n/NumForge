const { artifactPath } = require('./artifacts');
const {test,expect}=require('@playwright/test');
const {assess}=require('../../scripts/benchmark_graph_audit.cjs');
const query={mode:'fast',width:800,height:440,min:-100,max:100,ymin:-10,ymax:10,interpolation:'hermite',dense:'band'};
async function sample(request,expression,extra={}){
 const response=await request.post('/api/graph/plot?'+new URLSearchParams({...query,...extra}),{data:expression});expect(response.ok(),expression).toBe(true);return response.json();
}
test('quadratic phases mix bounded ranges with accurate sampled turns and resolved zoom',async({request})=>{
 for(const [width,height] of [[800,440],[320,300]])for(const centre of [0,.1234])for(const factor of [1,-10])for(const kind of ['sin','cos']){
  const expression=`${kind}(${factor}*(x-(${centre}))^2)`,f=x=>Math[kind](factor*(x-centre)**2);
  const data=await sample(request,expression,{width,height});expect(data.evaluation).toBe('double');expect(data.limited).toBe(false);expect(data.count).toBeLessThan(4096);
  expect(data.bands.length).toBeGreaterThan(0);expect(data.bands.length).toBeLessThan(width);expect(data.bands.every(b=>!(b[0]<=centre&&b[1]>=centre))).toBe(true);
  expect(data.points.some(p=>Math.abs(p[0]-centre)<1e-14)).toBe(true);expect(data.points.some(p=>p[2])).toBe(true);
  const checked=assess({f,width,height,min:-100,max:100,ymin:-10,ymax:10,anchors:[centre]},data);expect(checked.visible_coverage).toBe(1);expect(checked.max_render_error_px).toBeLessThan(.5);
  const zoom=await sample(request,expression,{width,height,min:centre-1,max:centre+1});expect(zoom.bands).toBeUndefined();expect(zoom.limited).toBe(false);
  const resolved=assess({f,width,height,min:centre-1,max:centre+1,ymin:-10,ymax:10,anchors:[centre]},zoom);expect(resolved.visible_coverage).toBe(1);expect(resolved.max_render_error_px).toBeLessThan(.1);
 }
});
test('quadratic ranges retain arithmetic, degrees, phase limits and precision fallback',async({request})=>{
 for(const expression of ['sin(x*x)','sin(x^2)','sin(x²)']){const d=await sample(request,expression);expect(d.limited).toBe(false);expect(d.bands.length).toBeGreaterThan(0);}
 const degrees=await sample(request,'cos(180*(x-0.1234)^2/π)',{angle:'deg'});expect(degrees.bands.length).toBeGreaterThan(0);expect(degrees.points.some(p=>Math.abs(p[0]-.1234)<1e-14)).toBe(true);
 for(const p of degrees.points)expect(p[1]).toBeCloseTo(Math.cos((p[0]-.1234)**2),7);
 const limit=await sample(request,'sin(x*x)',{min:-1000,max:1000});expect(limit.limited).toBe(false);expect(limit.count).toBeLessThan(1024);expect(limit.bands.length).toBeGreaterThan(700);
 const precision=await sample(request,'sin(x*x)',{precision:35,min:-1,max:1});expect(precision.schema_version).toBe(2);expect(precision.bands).toBeUndefined();
 const cancelled=await sample(request,'sin((x-1000000)^2)',{min:999999.99,max:1000000.01,width:320});expect(cancelled.bands).toBeUndefined();
 for(const expression of ['sin(x^3)','sin(x*x)/x','sin(x*x)+x*x','x*x*sin(x*x)']){const d=await sample(request,expression,{min:-1,max:1});expect(d.bands,expression).toBeUndefined();}
});
for(const width of [390,1280])test(`actual quadratic SVG survives range transitions and reset at width ${width}`,async({page})=>{
 test.setTimeout(30000);await page.setViewportSize({width,height:844});await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('sin(x*x)');await page.locator('#graph-submit').click();
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 for(let i=0;i<4;i++){await page.locator('#graph-plot').press('-');await page.waitForTimeout(220);await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');}
 await expect(page.locator('path.graph-band')).toHaveCount(1);await expect(page.locator('path.graph-line')).toHaveCount(1);await expect(page.locator('#graph-status')).not.toContainText('unresolved');
 await page.locator('.graph-result').screenshot({path:artifactPath(`graph-quadratic-${width}.png`)});
 await page.locator('#graph-plot').press('Home');await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('path.graph-band')).toHaveCount(0);
 const checked=await page.locator('#graph-plot').evaluate(svg=>{
  const w=svg.viewBox.baseVal.width,h=svg.viewBox.baseVal.height;let error=0,checks=0;
  for(const line of svg.querySelectorAll('path.graph-line')){const length=line.getTotalLength();for(let i=0;i<=3000;i++){
   const p=line.getPointAtLength(length*i/3000),x=-10+(p.x-64)/(w-86)*20,y=10-(p.y-22)/(h-68)*20;
   if(x>=-10&&x<=10){error=Math.max(error,Math.abs(y-Math.sin(x*x))*(h-68)/20);checks++;}
  }}return{error,checks};
 });expect(checked.checks).toBeGreaterThan(100);expect(checked.error).toBeLessThan(1);
});
