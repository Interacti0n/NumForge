const {test,expect}=require('@playwright/test');

test('guarded rational peaks and valleys retain shifted extrema and accurate neighbouring edges',async({request})=>{
 for(const centre of [.1234,-.43127,.00037,3.78123])for(const width of [.2,.04,.004,.0004])for(const sign of [1,-1]){
  const expression=`${sign}/(1+((x-(${centre}))/${width})^2)`,reference=x=>sign/(1+((x-centre)/width)**2);
  const q=new URLSearchParams({mode:'fast',min:-10,max:10,ymin:-10,ymax:10,width:800,height:440,interpolation:'linear'});
  const body=await (await request.post('/api/graph/plot?'+q,{data:expression})).json();expect(body.ok).toBe(true);expect(body.evaluation).toBe('double');
  // Coordinates are approximate: require a subpixel peak, not exact root digits.
  const peak=body.points.reduce((a,b)=>sign*b[1]>sign*a[1]?b:a);expect(Math.abs(peak[1]-sign)*372/20,expression).toBeLessThan(.01);
  for(const p of body.points)if(p[3]===0)expect(Math.abs(p[1]-reference(p[0]))).toBeLessThan(2e-8);
  let checks=0,error=0;
  for(let i=1;i<body.points.length;i++){const b=body.points[i],a=body.points[i-1];if(!b[2])continue;
   for(const t of [.25,.5,.75]){const x=a[0]+(b[0]-a[0])*t,y=reference(x);error=Math.max(error,Math.abs(y-(a[1]*(1-t)+b[1]*t))*372/20);checks++;}
  }
  expect(checks).toBeGreaterThan(50);expect(error,expression).toBeLessThan(1.5);expect(body.count).toBeLessThan(1024);expect(body.limited,expression).toBe(false);
 }
});

test('actual SVG includes the narrow Lorentzian peak after foreground sampling',async({page})=>{
 await page.goto('/graph?lang=en');const expression='1/(1+((x-0.1234)/0.04)^2)';
 await page.locator('#graph-expression').fill(expression);const response=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&r.request().postData()===expression);
 await page.locator('#graph-submit').click();await (await response).finished();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('path.graph-line')).toBeVisible();
 const error=await page.locator('#graph-plot').evaluate(svg=>{
  const w=svg.viewBox.baseVal.width,h=svg.viewBox.baseVal.height,x=64+(.1234+10)/20*(w-86),line=svg.querySelector('path.graph-line');let low=0,high=line.getTotalLength();
  for(let i=0;i<60;i++){const m=(low+high)/2;if(line.getPointAtLength(m).x<x)low=m;else high=m;}
  return Math.abs(line.getPointAtLength((low+high)/2).y-(h-46-11/20*(h-68)));
 });expect(error).toBeLessThan(1.5);
});

test('guarded narrow features survive offsets, trends, multiple peaks and camera edges',async({request})=>{
 const l=(x,c,w)=>1/(1+((x-c)/w)**2);
 for(const [expression,reference,anchors] of [
  ['3+2/(1+((x-0.1234)/0.004)^2)+x/100',x=>3+2*l(x,.1234,.004)+x/100,[.1234]],
  ['2-1/(1+((x-0.1234)/0.004)^2)',x=>2-l(x,.1234,.004),[.1234]],
  ['1/(1+((x-0.1234)/0.004)^2)+1/(1+((x+0.43127)/0.01)^2)',x=>l(x,.1234,.004)+l(x,-.43127,.01),[.1234,-.43127]],
  ['1/(1+((x-9.999)/0.004)^2)',x=>l(x,9.999,.004),[9.999]]
 ]){
  const body=await (await request.post('/api/graph/plot?mode=fast&min=-10&max=10&ymin=-10&ymax=10&width=800&height=440',{data:expression})).json();expect(body.ok).toBe(true);
  for(const x of anchors){const i=body.points.findIndex(p=>p[0]>=x),a=body.points[i-1],b=body.points[i];expect(b[2],expression).toBe(1);
   const t=(x-a[0])/(b[0]-a[0]);expect(Math.abs(a[1]*(1-t)+b[1]*t-reference(x))*372/20,expression).toBeLessThan(1.5);
  }
  expect(body.limited,expression).toBe(false);
 }
});
