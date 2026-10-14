const {test,expect}=require('@playwright/test');
const expression='1/ln(ln(sqrt(x)))^2',reference=x=>1/Math.log(Math.log(Math.sqrt(x)))**2,pole=Math.exp(2);
test('nested logarithm preserves both branches, domain and pole with bounded camera work',async({request})=>{
 for(const [min,max,ymin,ymax] of [[-10,10,-10,10],[1.001,20,-10,100],[2,30,-10,10],[7,8,-10,1000]]){
  const q=new URLSearchParams({mode:'fast',min,max,ymin,ymax,width:800,height:440,interpolation:'hermite'});
  const body=await (await request.post('/api/graph/plot?'+q,{data:expression})).json();
  expect(body.ok).toBe(true);expect(body.evaluation).toBe('double');expect(body.screen_clipped).toBe(true);expect(body.count).toBeLessThan(512);
  let left=0,right=0;
  for(let i=0;i<body.points.length;i++){
   const p=body.points[i],a=body.points[i-1];if(p[3]===0){expect(p[0]).toBeGreaterThan(1);expect(p[1]).toBeGreaterThan(0);
    // Binary libm is not an accurate oracle extremely close to the pole.
    // Check values in/near the visible camera, and all connection/domain flags.
    if(p[1]<=ymax*2)expect(Math.abs(p[1]-reference(p[0]))).toBeLessThan(Math.abs(p[1])*1e-8+1e-12);
   }
   if(!a||!p[2])continue;expect(a[3]).toBe(0);expect(a[0]<pole&&p[0]>pole).toBe(false);
   for(const t of [.25,.5,.75]){const x=a[0]+(p[0]-a[0])*t,y=reference(x);if(y<ymin||y>ymax)continue;
    expect(Math.abs(y-(a[1]+(p[1]-a[1])*t))*440/(ymax-ymin)).toBeLessThan(2);if(x<pole)left++;else right++;
   }
  }
  if(min<4&&ymax>=10)expect(left).toBeGreaterThan(5);if(max>=20||min===7)expect(right).toBeGreaterThan(5);
 }
 const recovery=await (await request.post('/api/graph/plot?mode=fast&min=-10&max=10&ymin=-10&ymax=10',{data:'abs(sqrt(x))'})).json();
 expect(recovery.evaluation).toBe('precision');expect(recovery.points.filter(p=>p.kind==='real'&&Number(p.x)<0).length).toBeGreaterThan(5);
});
for(const [lang,width] of [['en',1280],['sk',390]])test(`graph notes distinguish domain, absent real values and off-screen curves ${lang}`,async({page})=>{
 await page.setViewportSize({width,height:844});await page.goto('/graph?lang='+lang);
 const plot=async expr=>{await page.locator('#graph-expression').fill(expr);await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');};
 await plot(expression);await expect(page.locator('#graph-status')).toContainText(lang==='en'?'outside the real domain':'mimo reálnej domény');await expect(page.locator('path.graph-line')).toBeVisible();
 await plot('sqrt(-1)');await expect(page.locator('#graph-status')).toContainText(lang==='en'?'No real-valued points':'nenašli reálne body');await expect(page.locator('#graph-status')).not.toHaveClass(/error/);
 await plot('100+sin(x)');await expect(page.locator('#graph-status')).toContainText(lang==='en'?'outside this view':'mimo tohto pohľadu');
 await page.locator('#graph-add').click();await page.locator('.graph-curve').nth(1).locator('input:not([type=color])').fill('x');await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 await expect(page.locator('.graph-curve').first().locator('.graph-curve-status')).toBeVisible();await expect(page.locator('.graph-curve').first().locator('.graph-curve-status')).toHaveAttribute('data-notice','outside');
 await expect(page.locator('#graph-status')).toContainText(lang==='en'?'1 function plotted':'Zobrazené funkcie: 1');
});
test('vertical pan refines clipped regions and retains the uncut visible sibling',async({page})=>{
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill(expression);await page.locator('#graph-add').click();await page.locator('.graph-curve').nth(1).locator('input:not([type=color])').fill('x');
 await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await page.waitForTimeout(600);
 await page.locator('#graph-plot').press('ArrowRight');await page.waitForTimeout(500);await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 const calls=[];page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))calls.push({body:r.postData(),q:new URL(r.url()).searchParams});});
 await page.locator('#graph-plot').evaluate(svg=>{for(let i=0;i<10;i++)svg.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowUp',bubbles:true,cancelable:true}));});
 await expect.poll(()=>calls.some(r=>r.body===expression&&Number(r.q.get('ymin'))===30)).toBe(true);
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');expect(calls.filter(r=>r.body==='x')).toHaveLength(0);
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymin','30');
 const data=await page.locator('#graph-plot').evaluate(svg=>({low:Number(svg.dataset.xmin),high:Number(svg.dataset.xmax),width:svg.viewBox.baseVal.width,d:svg.querySelector('path.graph-line[data-curve="1"]').getAttribute('d')}));
 // Both visible branches at y=40 must be represented after y-only movement.
 for(const x of [Math.exp(2*Math.exp(-1/Math.sqrt(40))),Math.exp(2*Math.exp(1/Math.sqrt(40)))]){
  const px=64+(x-data.low)/(data.high-data.low)*(data.width-86);expect([...data.d.matchAll(/[MLC]([^MLC]+)/g)].some(m=>Math.abs(Number(m[1].trim().split(/[ ,]+/).at(-2))-px)<12)).toBe(true);
 }
});
test('malformed clipped-camera metadata is rejected',async({page})=>{
 await page.route('**/api/graph/plot?**',async route=>{const response=await route.fetch(),body=await response.json();body.screen_clipped='yes';await route.fulfill({json:body});});
 await page.goto('/graph?lang=en');await page.locator('#graph-submit').click();await expect(page.locator('#graph-status')).toHaveClass(/error/);await expect(page.locator('path.graph-line')).toHaveCount(0);
});
test('quantity and numeric projection limits have distinct non-error notes',async({page})=>{
 let kind=3;
 await page.route('**/api/graph/plot?**',route=>route.fulfill({json:{ok:true,schema_version:3,evaluation:'double',parameter:'x',quality:'refined',width:2048,height:440,count:3,limited:false,y_min:null,y_max:null,points:[[-10,null,0,kind,1],[0,null,0,kind,1],[10,null,0,kind,1]]}}));
 await page.goto('/graph?lang=en');await page.locator('#graph-submit').click();await expect(page.locator('#graph-status')).toContainText('Values with units');await expect(page.locator('#graph-status')).not.toHaveClass(/error/);
 kind=0;await page.locator('#graph-expression').fill('2*x');await page.locator('#graph-submit').click();await expect(page.locator('#graph-status')).toContainText('plotting range');await expect(page.locator('path.graph-line')).toHaveCount(0);
});
