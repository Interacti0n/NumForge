const {test,expect}=require('@playwright/test');
const defaults={mode:'fast',width:'800',height:'440',min:'-10',max:'10',ymin:'-10',ymax:'10',interpolation:'hermite',dense:'band'};
async function sample(request,expression,options={}){const response=await request.post('/api/graph/plot?'+new URLSearchParams({...defaults,...options}),{data:expression});expect(response.ok(),expression).toBe(true);return response.json();}
test('C dense envelopes contain independently evaluated single-carrier values',async({request})=>{
 const cases=[['sin(1000x)',x=>Math.sin(1000*x)],['2-3*cos(-1000t+π/3)',x=>2-3*Math.cos(-1000*x+Math.PI/3)],
  ['x*sin(1000x)+2',x=>x*Math.sin(1000*x)+2],['sin(1000x)+x',x=>Math.sin(1000*x)+x],
  ['2*x*cos(1000x)+x/2',x=>2*x*Math.cos(1000*x)+x/2]];
 for(const [expression,f] of cases){const data=await sample(request,expression);expect(data.dense_representation).toBe('pixel-envelope');expect(data.bands).toHaveLength(800);expect(data.count).toBe(33);expect(data.limited).toBe(false);expect(data.points.every(p=>p[2]===0)).toBe(true);
  const violations=[];for(let i=0;i<data.bands.length;i+=13){const [a,b,low,high]=data.bands[i];if(!(a<b&&low<=high))violations.push('shape');for(let j=0;j<=8;j++){const y=f(a+(b-a)*j/8);if(y<low-1e-10||y>high+1e-10)violations.push([i,y,low,high]);}}
  expect(violations,expression).toEqual([]);
 }
 const deg=await sample(request,'sin(360000x+37)',{angle:'deg',width:'320'});expect(deg.bands).toHaveLength(320);expect(deg.bands.every(b=>Math.abs(b[2]+1)<1e-12&&Math.abs(b[3]-1)<1e-12)).toBe(true);
});
test('envelopes are opt-in, disappear at resolved scales and preserve precision fallback',async({request})=>{
 const legacy={...defaults};delete legacy.dense;const old=await (await request.post('/api/graph/plot?'+new URLSearchParams(legacy),{data:'sin(1000x)'})).json();expect(old.bands).toBeUndefined();expect(old.limited).toBe(true);
 const zoom=await sample(request,'sin(1000x)',{min:'-.01',max:'.01'});expect(zoom.bands).toBeUndefined();expect(zoom.limited).toBe(false);expect(zoom.points.slice(1).every(p=>p[2]===1)).toBe(true);
 for(const p of zoom.points)expect(p[1]).toBeCloseTo(Math.sin(1000*p[0]),8);
 const fallback=await sample(request,'sin(x)',{precision:'100',min:'-.01',max:'.01'});expect(fallback.schema_version).toBe(2);expect(fallback.bands).toBeUndefined();
 const cancelled=await sample(request,'sin(1000x)-sin(1000x)+cos(1100x)',{min:'-.001',max:'.001'});expect(cancelled.schema_version).toBe(2);expect(cancelled.bands).toBeUndefined();
});
test('variable poles, nonlinear domains and excessive carrier sums never acquire envelopes',async({request})=>{
 for(const expression of ['tan(1000x)','sin(1000x)/(x-0.1)','sin(1000x)+cos(1100x)+sin(1200x)+cos(1300x)+sin(1400x)','sqrt(x)*sin(1000x)','sin(1000x*x)']){
  const data=await sample(request,expression,{min:'0',max:'1',width:'320'});expect(data.bands,expression).toBeUndefined();
  if(expression.includes('/'))data.points.forEach((p,i)=>{if(p[2])expect(data.points[i-1][0]<.1&&p[0]>.1).toBe(false);});
 }
 for(const extra of [{mode:'',dense:'band'},{dense:'curve'},{ymin:'',ymax:''}]){const response=await request.post('/api/graph/plot?'+new URLSearchParams({...defaults,...extra}),{data:'sin(1000x)'});expect(response.ok()).toBe(false);}
});
test('dense sums preserve independent values, repeated terms and resolved zoom',async({request})=>{
 for(const [expression,f] of [
  ['sin(1000x)+sin(1001x)',x=>Math.sin(1000*x)+Math.sin(1001*x)],
  ['-(sin(1000x)-2*cos(-1001x+0.3))/2+x/10',x=>-(Math.sin(1000*x)-2*Math.cos(-1001*x+.3))/2+x/10],
  ['x*(sin(1000x)+cos(1100x))+2',x=>x*(Math.sin(1000*x)+Math.cos(1100*x))+2],
  ['sin(1000x)+cos(1100x)-sin(1200x)+cos(1300x)',x=>Math.sin(1000*x)+Math.cos(1100*x)-Math.sin(1200*x)+Math.cos(1300*x)],
  ['sin(1000x)+sin(1000x)+cos(1100x)',x=>2*Math.sin(1000*x)+Math.cos(1100*x)]]){
  const data=await sample(request,expression);expect(data.bands).toHaveLength(800);expect(data.count).toBe(33);expect(data.limited).toBe(false);
  const bad=[];for(const [a,b,low,high] of data.bands)for(let j=0;j<=32;j++){const y=f(a+(b-a)*j/32);if(y<low-1e-10||y>high+1e-10)bad.push(y);}expect(bad,expression).toEqual([]);
  const zoom=await sample(request,expression,{min:'-.001',max:'.001'});expect(zoom.bands).toBeUndefined();expect(zoom.limited).toBe(false);expect(zoom.points.slice(1).every(p=>p[2]===1)).toBe(true);
  for(const p of zoom.points)expect(p[1]).toBeCloseTo(f(p[0]),8);
 }
});
test('mixed dense curve stays visible as a range beside a resolved function',async({page})=>{
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('sin(1000x)+sin(1001x)');await page.locator('#graph-add').click();await page.locator('.graph-curve').last().locator('input:not([type=color])').fill('x');await page.locator('#graph-submit').click();
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('path.graph-band')).toHaveCount(1);await expect(page.locator('path.graph-line')).toHaveCount(1);await expect(page.locator('#graph-status')).toContainText('2 functions plotted');await expect(page.locator('#graph-status')).toContainText('1 as dense ranges');
});
for(const [lang,width] of [['en',1280],['sk',390]])test(`dense range transitions to an inspected curve after zoom ${lang} ${width}`,async({page})=>{
 await page.setViewportSize({width,height:844});await page.goto('/graph?lang='+lang);await page.locator('#graph-expression').fill('sin(1000x)');await page.locator('#graph-submit').click();await expect(page.locator('path.graph-band')).toHaveCount(1);await expect(page.locator('path.graph-line')).toHaveCount(0);await expect(page.locator('#graph-status')).toContainText(lang==='en'?'approximate range':'približný rozsah');
 const rect=await page.locator('#graph-plot').boundingBox();await page.mouse.move(rect.x+rect.width/2,rect.y+rect.height/2);await expect(page.locator('#graph-inspector')).toBeHidden();
 await page.locator('.graph-result').screenshot({path:`C:/Incoming/Projekty/NumForge/build/graph-dense-${lang}-${width}.png`});
 await page.locator('#graph-plot').evaluate(svg=>{for(let i=0;i<4;i++)svg.dispatchEvent(new KeyboardEvent('keydown',{key:'+',bubbles:true,cancelable:true}));});await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('path.graph-line')).toBeVisible();await expect(page.locator('path.graph-band')).toHaveCount(0);
 await page.locator('#graph-reset').click();await expect(page.locator('path.graph-band')).toHaveCount(1);await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');
});
test('dense bands share normal color and visibility controls without coordinate invention',async({page})=>{
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('sin(1000x)');await page.locator('#graph-add').click();await page.locator('.graph-curve').last().locator('input:not([type=color])').fill('x');await page.locator('#graph-submit').click();await expect(page.locator('#graph-status')).toContainText('2 functions plotted');await expect(page.locator('#graph-status')).toContainText('1 as dense ranges');await expect(page.locator('path.graph-band')).toHaveCount(1);await expect(page.locator('path.graph-line')).toHaveCount(1);await page.waitForTimeout(500);
 const id=await page.locator('.graph-curve').first().getAttribute('data-curve');let calls=0;page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))++calls;});await page.locator('#graph-color').evaluate(e=>{e.value='#ff1234';e.dispatchEvent(new Event('input',{bubbles:true}));});await expect(page.locator('path.graph-band')).toHaveCSS('fill','rgb(255, 18, 52)');
 await page.locator(`.graph-curve[data-curve="${id}"] .graph-visibility`).click();await expect(page.locator('path.graph-band')).toHaveCount(0);await page.locator(`.graph-curve[data-curve="${id}"] .graph-visibility`).click();await expect(page.locator('path.graph-band')).toHaveCount(1);await page.waitForTimeout(300);expect(calls).toBe(0);
});
test('malformed or out-of-coverage band metadata is rejected',async({page})=>{
 let bands;await page.route('**/api/graph/plot?**',async route=>{const response=await route.fetch(),data=await response.json();data.dense_representation='pixel-envelope';data.bands=bands;await route.fulfill({json:data});});await page.goto('/graph?lang=en');
 for(const invalid of [[[0,0,-1,1]],[[0,1,2,1]],[[0,2,-1,1],[1,3,-1,1]],[[0,100,-1,1]],Array.from({length:2049},(_,i)=>[i,i+1,-1,1])]){bands=invalid;await page.locator('#graph-expression').fill('sin(x)');await page.locator('#graph-submit').click();await expect(page.locator('#graph-status')).toHaveClass(/error/);await expect(page.locator('path.graph-band,path.graph-line')).toHaveCount(0);}
});

test('subpixel envelope remains visible and off-screen ranges are not counted',async({page})=>{
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('1e-300*sin(1000x)');await page.locator('#graph-submit').click();await expect(page.locator('path.graph-band')).toHaveCount(1);
 const box=await page.locator('path.graph-band').evaluate(p=>({height:p.getBBox().height,invalid:/NaN|Infinity/.test(p.getAttribute('d'))}));expect(box.height).toBeGreaterThanOrEqual(1);expect(box.invalid).toBe(false);
 await page.locator('#graph-expression').fill('100+sin(1000x)');await page.locator('#graph-add').click();await page.locator('.graph-curve').last().locator('input:not([type=color])').fill('x');await page.locator('#graph-submit').click();await expect(page.locator('path.graph-band')).toHaveCount(0);await expect(page.locator('#graph-status')).toContainText('1 function plotted');
});
