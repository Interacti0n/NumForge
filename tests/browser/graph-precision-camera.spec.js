const {test,expect}=require('@playwright/test');

test('precision camera refines visible hyperbolic chords instead of seeded off-screen extrema',async({request})=>{
 for(const [expression,reference,min,max,ymin,ymax] of [
  ['sinh(10x)',x=>Math.sinh(10*x),-10,10,-10,10],
  ['cosh(10x)',x=>Math.cosh(10*x),-10,10,-10,10],
  ['sinh(10x)',x=>Math.sinh(10*x),-10,10,90,110],
  ['cosh(10x)',x=>Math.cosh(10*x),-10,10,90,110]
 ])for(const [width,height] of [[800,440],[320,300]]){
  const q=new URLSearchParams({mode:'fast',min,max,ymin,ymax,width,height,interpolation:'hermite'});
  const body=await (await request.post('/api/graph/plot?'+q,{data:expression})).json();expect(body.ok).toBe(true);expect(body.evaluation).toBe('precision');
  let checks=0,worst=0;
  for(let i=1;i<body.points.length;i++){const b=body.points[i],a=body.points[i-1];if(!b.connect)continue;expect(a.kind).toBe('real');expect(b.kind).toBe('real');
   for(const t of [.125,.25,.5,.75,.875]){const x=Number(a.x)+(Number(b.x)-Number(a.x))*t,y=reference(x);if(y<ymin||y>ymax)continue;
    worst=Math.max(worst,Math.abs(y-(Number(a.y)*(1-t)+Number(b.y)*t))*(height-68)/(ymax-ymin));checks++;
   }
  }
  expect(worst,expression+' visible chord error').toBeLessThan(1.5);expect(checks).toBeGreaterThan(8);
  expect(body.screen_clipped).toBe(true);expect(body.count).toBeLessThan(1024);expect(body.width).toBe(width);expect(body.height).toBe(height);
 }
});

test('precision camera preserves tiny seeded signals that reveal narrow smooth peaks',async({request})=>{
 const body=await (await request.post('/api/graph/plot?mode=fast&precision=35&min=-10&max=10&ymin=-10&ymax=10&width=800&height=440',{data:'exp(-((x-0.1234)/0.04)^2)'})).json();
 expect(body.ok).toBe(true);expect(body.evaluation).toBe('precision');
 expect(Math.max(...body.points.filter(p=>p.kind==='real').map(p=>Number(p.y)))).toBeGreaterThan(.98);
 const right=body.points.findIndex(p=>Number(p.x)>=.1234),b=body.points[right],a=body.points[right-1];expect(b.connect).toBe(true);
 const t=(.1234-Number(a.x))/(Number(b.x)-Number(a.x)),drawn=Number(a.y)*(1-t)+Number(b.y)*t;
 expect(Math.abs(drawn-1)*372/20).toBeLessThan(1.5);
});

test('precision camera SVG follows sinh and cosh at independently chosen coordinates',async({page})=>{
 await page.goto('/graph?lang=en');
 for(const [expression,x,reference] of [['sinh(10x)',-.1953125,Math.sinh(-1.953125)],['cosh(10x)',-.1904296875,Math.cosh(-1.904296875)]]){
  await page.locator('#graph-expression').fill(expression);const response=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&r.request().postData()===expression);
  await page.locator('#graph-submit').click();await (await response).finished();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('path.graph-line')).toBeVisible();
  const error=await page.locator('#graph-plot').evaluate((svg,{x,reference})=>{
   const w=svg.viewBox.baseVal.width,h=svg.viewBox.baseVal.height,target=64+(x+10)/20*(w-86),line=svg.querySelector('path.graph-line');let low=0,high=line.getTotalLength();
   for(let i=0;i<60;i++){const m=(low+high)/2;if(line.getPointAtLength(m).x<target)low=m;else high=m;}
   const drawn=line.getPointAtLength((low+high)/2);return Math.abs(drawn.y-(h-46-(reference+10)/20*(h-68)));
  },{x,reference});expect(error).toBeLessThan(1.5);
 }
});

test('precision camera vertical movement resamples deferred y regions and reuses a complete sibling',async({page})=>{
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('sinh(10x)');await page.locator('#graph-add').click();await page.locator('.graph-curve').nth(1).locator('input:not([type=color])').fill('x');
 await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await page.waitForTimeout(600);
 const calls=[];page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))calls.push({body:r.postData(),q:new URL(r.url()).searchParams});});
 await page.locator('#graph-plot').evaluate(svg=>{for(let i=0;i<25;i++)svg.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowUp',bubbles:true,cancelable:true}));});
 await expect.poll(()=>calls.some(r=>r.body==='sinh(10x)'&&Number(r.q.get('ymin'))===90)).toBe(true);await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 expect(calls.filter(r=>r.body==='x')).toHaveLength(0);await expect(page.locator('path.graph-line[data-curve="1"]')).toBeVisible();
 await expect(page.locator('#graph-status')).not.toHaveClass(/error/);
});

test('precision camera metadata rejects malformed deferral flags',async({page})=>{
 await page.route('**/api/graph/plot?**',async route=>{const response=await route.fetch(),body=await response.json();body.screen_clipped='yes';await route.fulfill({json:body});});
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('sinh(10x)');await page.locator('#graph-submit').click();
 await expect(page.locator('#graph-status')).toHaveClass(/error/);await expect(page.locator('path.graph-line')).toHaveCount(0);
});
