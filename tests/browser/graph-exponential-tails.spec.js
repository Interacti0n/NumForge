const {test,expect}=require('@playwright/test');

test('screen Gaussian tails retain peaks, valleys and shoulders at different scales',async({request})=>{
 for(const [min,max] of [[-10,10],[-1000,1000]])for(const [width,height] of [[800,440],[320,300]])
 for(const centre of [0,.1234,-.43127])for(const spread of [.04,.005,.0004])for(const sign of [1,-1]){
  const expression=`${sign}*exp(-((x-(${centre}))/${spread})^2)`,f=x=>sign*Math.exp(-(((x-centre)/spread)**2));
  const q=new URLSearchParams({mode:'fast',min,max,ymin:-10,ymax:10,width,height,interpolation:'linear'});
  const body=await (await request.post('/api/graph/plot?'+q,{data:expression})).json();
  expect(body.ok,expression).toBe(true);expect(body.evaluation).toBe('double');expect(body.limited,expression).toBe(false);
  expect(body.count).toBeLessThan(512);let worst=0;
  for(const p of body.points)if(p[3]===0)expect(Math.abs(p[1]-f(p[0])),expression).toBeLessThan(2e-8);
  for(const x of [centre,...[-2,-1,-.5,.5,1,2].map(t=>centre+t*spread)]){
   const i=body.points.findIndex(p=>p[0]>=x),a=body.points[i-1],b=body.points[i];expect(b[2],expression).toBe(1);
   const t=(x-a[0])/(b[0]-a[0]);worst=Math.max(worst,Math.abs(a[1]*(1-t)+b[1]*t-f(x))*(height-68)/20);
  }
  expect(worst,expression).toBeLessThan(1.5);
 }
});

test('screen exponential seeds survive composition without joining domain gaps',async({request})=>{
 const g=(x,c,w)=>Math.exp(-(((x-c)/w)**2));
 for(const [expression,f,anchors] of [
  ['3-2*exp(-((x-.1234)/.005)^2)+x/100',x=>3-2*g(x,.1234,.005)+x/100,[.1234]],
  ['exp(-((x-.1234)/.005)^2)+exp(-((x+.43127)/.0004)^2)',x=>g(x,.1234,.005)+g(x,-.43127,.0004),[.1234,-.43127]],
  ['exp(-((x-9.999)/.005)^2)',x=>g(x,9.999,.005),[9.999]]
 ]){
  const b=await (await request.post('/api/graph/plot?mode=fast&min=-10&max=10&ymin=-10&ymax=10&width=800&height=440',{data:expression})).json();
  expect(b.ok,expression).toBe(true);expect(b.limited).toBe(false);
  for(const x of anchors){const i=b.points.findIndex(p=>p[0]>=x),a=b.points[i-1],c=b.points[i];expect(c[2],expression).toBe(1);
   const t=(x-a[0])/(c[0]-a[0]);expect(Math.abs(a[1]*(1-t)+c[1]*t-f(x))*372/20,expression).toBeLessThan(1.5);}
 }
 const b=await (await request.post('/api/graph/plot?mode=fast&min=-10&max=10&ymin=-10&ymax=10',{data:'exp(-1/x^2)'})).json();
 expect(b.ok).toBe(true);
 for(let i=1;i<b.points.length;i++)if(b.points[i][2])expect(b.points[i-1][0]<0&&b.points[i][0]>0).toBe(false);
});

test('actual SVG retains the very narrow Gaussian maximum',async({page})=>{
 await page.goto('/graph?lang=en');const expression='exp(-((x-0.1234)/0.005)^2)';
 await page.locator('#graph-expression').fill(expression);const response=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&r.request().postData()===expression);
 await page.locator('#graph-submit').click();await (await response).finished();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 await expect(page.locator('path.graph-line')).toBeVisible();
 const error=await page.locator('#graph-plot').evaluate(svg=>{
  const w=svg.viewBox.baseVal.width,h=svg.viewBox.baseVal.height,x=64+(.1234+10)/20*(w-86),line=svg.querySelector('path.graph-line');let lo=0,hi=line.getTotalLength();
  for(let i=0;i<60;i++){const m=(lo+hi)/2;if(line.getPointAtLength(m).x<x)lo=m;else hi=m;}
  return Math.abs(line.getPointAtLength((lo+hi)/2).y-(h-46-11/20*(h-68)));
 });expect(error).toBeLessThan(1.5);
});
