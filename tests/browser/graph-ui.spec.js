const {test,expect}=require('@playwright/test');
test('returning to a cached viewport preserves curve geometry and axis labels',async({page})=>{
 await page.goto('/graph?lang=en');await sample(page,'x*x');
 const snapshot=()=>page.locator('#graph-plot').evaluate(svg=>({
  paths:[...svg.querySelectorAll('path.graph-line')].map(p=>p.getAttribute('d')),
  ticks:[...svg.querySelectorAll('.graph-tick')].map(p=>p.textContent)
 }));
 const original=await snapshot();
 await page.locator('#graph-plot').focus();await page.locator('#graph-plot').press('ArrowRight');
 await expect(page.locator('#graph-min')).toHaveValue('-6');
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 await page.locator('#graph-plot').press('ArrowLeft');
 await expect(page.locator('#graph-min')).toHaveValue('-10');
 await expect.poll(snapshot).toEqual(original);
});
test('zoom stops before coordinate precision becomes unreliable and reset still works',async({page})=>{
 await page.goto('/graph?lang=en');
 await page.evaluate(()=>{for(let i=0;i<80;i++)document.querySelector('#graph-zoom-in').click();});
 const low=Number(await page.locator('#graph-min').inputValue()),high=Number(await page.locator('#graph-max').inputValue());
 expect(high-low).toBeGreaterThanOrEqual(1e-8);
 await page.locator('#graph-zoom-in').click();
 expect(Number(await page.locator('#graph-min').inputValue())).toBe(low);
 expect(Number(await page.locator('#graph-max').inputValue())).toBe(high);
 await page.locator('#graph-reset').click();
 await expect(page.locator('#graph-min')).toHaveValue('-10');await expect(page.locator('#graph-max')).toHaveValue('10');
});
test('deep sine peak zoom has distinct axes and no projection staircase',async({page})=>{
 await page.goto('/graph?lang=en');
 const center=-3*Math.PI/2,low=center+1e-6,high=center+8e-6;
 const data=await sample(page,'sin(x)',String(low),String(high));expect(data.plot_places).toBe(16);
 expect(new Set(data.points.map(p=>p.y)).size).toBeGreaterThan(40);
 for(const p of data.points)expect(Math.abs(Number(p.y)-Math.sin(Number(p.x)))).toBeLessThan(4e-16);
 const ticks=await page.locator('.graph-tick[x="56"]').allTextContents();expect(new Set(ticks).size).toBe(5);
 const left=await page.locator('.graph-tick[x="56"]').evaluateAll(nodes=>Math.min(...nodes.map(n=>n.getBBox().x)));expect(left).toBeGreaterThanOrEqual(0);
 await expect(page.locator('.graph-offset')).toHaveCount(2);
 expect(await page.locator('#graph-plot').evaluate(el=>!/NaN|Infinity/.test(el.innerHTML))).toBe(true);
 await page.locator('.graph-result').screenshot({path:'C:/Incoming/Projekty/NumForge/build/graph-sine-deep-zoom-fixed.png'});
 await sample(page,'sin(x)',String(center+1e-8),String(center+4e-8));
 const height=await page.locator('path.graph-line').evaluate(el=>el.getBBox().height);expect(height).toBeLessThan(1);
});
test('cached tile composition preserves reciprocal gaps',async({page})=>{
 await page.goto('/graph?lang=en');await sample(page,'1/x');
 const ready=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&new URL(r.url()).searchParams.get('min')==='10');
 await page.locator('#graph-plot').focus();await page.locator('#graph-plot').press('ArrowRight');await ready;
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 const paths=await page.locator('path.graph-line').evaluateAll(nodes=>nodes.map(n=>n.getAttribute('d')));
 const width=await page.locator('#graph-plot').evaluate(svg=>svg.viewBox.baseVal.width);
 const zero=64+6/20*(width-86);
 for(const path of paths)for(const chord of path.matchAll(/M([^,]+),([^L]+)L([^,]+),([^M]+)/g)){
  expect(Number(chord[1])<zero&&Number(chord[3])>zero).toBe(false);
 }
});
test('partial cache requests only the missing interval and setting changes recompute',async({page})=>{
 await page.route('**/api/graph/plot?**',route=>{
  const u=new URL(route.request().url());return Number(u.searchParams.get('min')) < -10 || Number(u.searchParams.get('max')) > 14 ? route.abort() : route.continue();
 });
 await page.goto('/graph?lang=en');await sample(page,'sin(x)');
 let ready=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&new URL(r.url()).searchParams.get('min')==='10'&&new URL(r.url()).searchParams.get('max')==='14');
 await page.locator('#graph-plot').focus();await page.locator('#graph-plot').press('ArrowRight');
 let response=await ready;expect(new URL(response.url()).searchParams.get('max')).toBe('14');
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 await page.locator('#graph-precision').fill('100');
 ready=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&new URL(r.url()).searchParams.get('precision')==='100');
 await page.locator('#graph-submit').click();await ready;
 await page.locator('#graph-angle').selectOption('deg');
 ready=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&new URL(r.url()).searchParams.get('angle')==='deg');
 await page.locator('#graph-submit').click();const data=await (await ready).json();
 expect(Number(data.points.at(-1).y)).toBeCloseTo(Math.sin(14*Math.PI/180),10);
});
test('prepared neighboring ranges pan immediately without another foreground calculation',async({page})=>{
 const responses=[];page.on('response',r=>{if(r.url().includes('/api/graph/plot?')&&r.ok())responses.push(new URL(r.url()));});
 await page.goto('/graph?lang=en');await sample(page,'sin(x)');
 await expect.poll(()=>responses.some(u=>u.searchParams.get('min')==='-30')&&responses.some(u=>u.searchParams.get('max')==='30'),{timeout:10000}).toBe(true);
 const before=responses.length;
 await page.locator('#graph-plot').focus();await page.locator('#graph-plot').press('ArrowRight');
 await expect(page.locator('#graph-min')).toHaveValue('-6');await expect(page.locator('#graph-max')).toHaveValue('14');
 await expect(page.locator('path.graph-line')).toHaveCount(2);await expect(page.locator('#graph-status')).not.toHaveText('Sampling…');
 await page.waitForTimeout(450);
 expect(responses.slice(before).some(u=>u.searchParams.get('min')==='-6'&&u.searchParams.get('max')==='14')).toBe(false);
});
test('uncached refinement keeps the previous curve visible and changes invalidate the cache',async({page})=>{
 await page.goto('/graph?lang=en');await sample(page,'sin(x)');
 let release;const held=new Promise(resolve=>release=resolve);
 await page.route('**/api/graph/plot?**',async route=>{
  const u=new URL(route.request().url());
  if(u.searchParams.get('min')==='-5'&&u.searchParams.get('max')==='5'){
   const response=await route.fetch();await held;try{await route.fulfill({response});}catch(_){}
  }else await route.continue();
 });
 await page.locator('#graph-zoom-in').click();await expect(page.locator('path.graph-line')).toBeVisible();
 await expect(page.locator('.graph-workspace')).toHaveAttribute('aria-busy','true');
 await expect(page.locator('path.graph-line')).toBeVisible();release();
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 const response=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&r.request().postData()==='cos(x)');
 await page.locator('#graph-expression').fill('cos(x)');await page.locator('#graph-submit').click();await response;
 await expect(page.locator('path.graph-line')).toBeVisible();
});
test('session cache validates the current revision before reusing results',async({page,request})=>{
 await page.goto('/?lang=en');await page.locator('#expression').fill('a=2');await page.locator('#expression').press('Enter');await expect(page.locator('#result')).toHaveText('2');
 const client=await page.evaluate(()=>sessionStorage.getItem('numforge-client-id'));
 await page.locator('.primary-nav a[href^="/graph"]').click();await page.locator('#graph-session').check();await sample(page,'a*x');
 const state=await (await request.get('/api/session?client='+client)).json();
 const commit=await request.post(`/api/evaluate?precision=full&angle=rad&client=${client}&revision=${Number(state.revision)+1}&action=commit`,{data:'a=3'});expect(commit.ok()).toBe(true);
 const response=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&r.request().postData()==='a*x'&&new URL(r.url()).searchParams.get('min')==='-10');
 await page.locator('#graph-reset').click();const data=await (await response).json();
 expect(Number(data.points.at(-1).y)).toBe(30);
});
test('near-zero axis ticks snap to zero without hiding genuinely tiny values',async({page})=>{
 let low='-1',high='1.00001';
 await page.route('**/api/graph/plot?**',route=>route.fulfill({json:{ok:true,schema_version:2,parameter:'x',limited:false,count:2,y_min:low,y_max:high,
  points:[{x:'-1',y:low,kind:'real',connect:false},{x:'1',y:high,kind:'real',connect:true}]}}));
 await page.goto('/graph?lang=en');await sample(page,'x');
 const ticks=page.locator('.graph-tick[x="56"]');
 await expect(ticks.filter({hasText:/^0$/})).toHaveCount(1);
 expect(await ticks.allTextContents()).not.toContain('-0');
 low='-1e-9';high='1e-9';await sample(page,'x');
 await expect(ticks.filter({hasText:/^0$/})).toHaveCount(1);
 expect((await ticks.allTextContents()).filter(t=>t.includes('E')||t.includes('e')).length).toBeGreaterThan(1);
});
test('wide sine renders equal sampled extrema',async({page})=>{
 await page.setViewportSize({width:1440,height:1000});await page.goto('/graph?lang=sk');
 const data=await sample(page,'sin(x)','-100','611');expect(data.limited).toBe(false);
 expect(Number(data.y_min)).toBe(-1);expect(Number(data.y_max)).toBe(1);
 await page.locator('.graph-settings summary').click();await expect(page.locator('path.graph-line')).toBeVisible();
 await page.locator('.graph-result').screenshot({path:'C:/Incoming/Projekty/NumForge/build/graph-sine-peaks-fixed.png'});
});
test('editing cancels queued view resampling',async({page})=>{
 await page.goto('/graph?lang=en');await sample(page,'x*x');let calls=0;
 page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))++calls;});
 await page.evaluate(()=>{
  document.querySelector('#graph-zoom-in').click();
  const input=document.querySelector('#graph-expression');input.value='sin(x)';input.dispatchEvent(new Event('input',{bubbles:true}));
 });
 await page.waitForTimeout(450);expect(calls).toBe(0);
 await expect(page.locator('#graph-status')).toHaveText('Enter a function and plot it.');
 await expect(page.locator('path.graph-line')).toHaveCount(0);
});
test('zoom, pan, keyboard and reset resample the view and fit y extrema',async({page})=>{
 await page.goto('/graph?lang=en');await sample(page,'x*x');
 await expect(page.locator('.graph-tick').filter({hasText:/^108$/})).toHaveCount(1);
 let ready=page.waitForResponse(r=>r.url().includes('/api/graph/plot?'));
 await page.locator('#graph-zoom-in').click();let data=await (await ready).json();expect(Number(data.y_max)).toBe(25);
 await expect(page.locator('.graph-tick').filter({hasText:/^27$/})).toHaveCount(1);
 await expect(page.locator('#graph-min')).toHaveValue('-5');await expect(page.locator('#graph-max')).toHaveValue('5');
 const svg=page.locator('#graph-plot');await svg.scrollIntoViewIfNeeded();let box=await svg.boundingBox();
 ready=page.waitForResponse(r=>r.url().includes('/api/graph/plot?'));
 await page.mouse.move(box.x+box.width*.6,box.y+box.height*.5);await page.mouse.down();
 await page.mouse.move(box.x+box.width*.4,box.y+box.height*.5,{steps:6});await page.mouse.up();await ready;
 expect(Number(await page.locator('#graph-min').inputValue())).toBeGreaterThan(-5);
 await svg.focus();await svg.press('Home');
 await expect(page.locator('#graph-min')).toHaveValue('-10');
 await svg.press('+');
 await expect(page.locator('#graph-max')).toHaveValue('5');
 let calls=0;page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))++calls;});
 ready=page.waitForResponse(r=>r.url().includes('/api/graph/plot?'));
 for(let i=0;i<3;++i)await svg.dispatchEvent('wheel',{deltaY:-100,clientX:box.x+box.width*.5});
 await ready;expect(calls).toBe(1);expect(Number(await page.locator('#graph-max').inputValue())-Number(await page.locator('#graph-min').inputValue())).toBeLessThan(10);
 await page.locator('#graph-reset').click();
 await expect(page.locator('#graph-min')).toHaveValue('-10');await expect(page.locator('#graph-max')).toHaveValue('10');
 expect(await svg.evaluate(el=>!/NaN|Infinity/.test(el.innerHTML))).toBe(true);
});
test('touch pinch zooms and empty view controls do not evaluate',async({page})=>{
 await page.setViewportSize({width:390,height:844});await page.goto('/graph?lang=sk');await sample(page,'sin(x)');
 const svg=page.locator('#graph-plot');await svg.scrollIntoViewIfNeeded();const box=await svg.boundingBox();
 const cdp=await page.context().newCDPSession(page),x=Math.round(box.x+box.width/2),y=Math.round(box.y+box.height/2);
 const ready=page.waitForResponse(r=>r.url().includes('/api/graph/plot?'));
 await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x:x-40,y,id:1},{x:x+40,y,id:2}]});
 await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x:x-70,y,id:1},{x:x+70,y,id:2}]});
 await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});await ready;await cdp.detach();
 expect(Number(await page.locator('#graph-max').inputValue())-Number(await page.locator('#graph-min').inputValue())).toBeCloseTo(20*80/140,6);
 await page.locator('#graph-expression').fill('');let calls=0;page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))++calls;});
 await page.locator('#graph-zoom-in').click();await expect(page.locator('#graph-values')).toBeHidden();
 await page.locator('#graph-reset').click();await expect(page.locator('#graph-min')).toHaveValue('-10');
 await expect(page.locator('#graph-status')).toHaveText('Zadaj funkciu a vykresli ju.');expect(calls).toBe(0);
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBe(true);
});
for(const lang of ['sk','en'])test(`empty graph retains default axes without requests ${lang}`,async({page})=>{
 let calls=0;page.on('request',r=>{if(r.url().includes('/api/graph/'))++calls;});
 await page.setViewportSize({width:390,height:844});await page.goto('/graph?lang='+lang);
 await expect(page.locator('#graph-plot')).toBeVisible();await expect(page.locator('.graph-axis')).toHaveCount(2);
 await page.locator('#graph-expression').fill('');await page.locator('#graph-submit').click();
 expect(calls).toBe(0);await expect(page.locator('path.graph-line')).toHaveCount(0);
 await expect(page.locator('.graph-tick').filter({hasText:/^-10$/})).toHaveCount(2);
 await expect(page.locator('.graph-tick').filter({hasText:/^10$/})).toHaveCount(2);
 await page.setViewportSize({width:1280,height:900});await expect(page.locator('.graph-axis')).toHaveCount(2);
 await sample(page,'x+y');await expect(page.locator('#graph-status')).toHaveClass(/error/);
 await expect(page.locator('#graph-plot')).toBeVisible();await expect(page.locator('path.graph-line')).toHaveCount(0);
});
async function sample(page,expression,min='-10',max='10') {
 await page.locator('#graph-expression').fill(expression);
 if((await page.locator('.graph-settings').getAttribute('open'))===null)await page.locator('.graph-settings summary').click();
 await page.locator('#graph-min').fill(min);await page.locator('#graph-max').fill(max);
 const ready=page.waitForResponse(r=>r.url().includes('/api/graph/plot?'));
 await page.locator('#graph-submit').click();const response=await ready;
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');return response.json();
}
for(const [lang,width] of [['sk',390],['en',1280]])test(`adaptive graph draws lines and typed gaps ${lang}`,async({page})=>{
 await page.setViewportSize({width,height:844});await page.goto('/graph?lang='+lang);
 await expect(page.locator('#graph-count,#graph-parameter,#graph-connect')).toHaveCount(0);
 const curve=await sample(page,'x*x');await expect(page.locator('#graph-plot')).toBeVisible();
 await expect(page.locator('circle.graph-point')).toHaveCount(0);await expect(page.locator('path.graph-line')).toHaveCount(1);
 expect(curve.points.length).toBeLessThan(1024);
 const reciprocal=await sample(page,'1/x');await expect(page.locator('path.graph-line')).toBeVisible();
 reciprocal.points.forEach((p,i)=>{if(p.connect){expect(p.kind).toBe('real');expect(reciprocal.points[i-1].kind).toBe('real');}});
 expect(reciprocal.points.some(p=>p.kind==='domain_error')).toBe(true);
 await page.locator('#graph-values summary').click();await expect(page.locator('#graph-table-body tr')).toHaveCount(reciprocal.count);
 await sample(page,'sqrt(t)');await expect(page.locator('#graph-status')).toContainText('f(t)');
 await expect(page.locator('#graph-table-body tr').first()).toContainText(lang==='en'?'Complex':'Komplexné');
 await sample(page,'qty(t;"m")');await expect(page.locator('#graph-plot')).toBeVisible();
 await expect(page.locator('#graph-table-body tr').first()).toContainText(lang==='en'?'Quantity':'Veličina');
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBe(true);
 await sample(page,'rand()');await expect(page.locator('#graph-status')).toHaveClass(/error/);
 await expect(page.locator('#graph-values')).toBeHidden();
});
test('adaptive browser numeric boundaries remain finite',async({page})=>{
 await page.goto('/graph?lang=en');await sample(page,'10^400');await expect(page.locator('#graph-plot')).toBeVisible();
 await page.locator('#graph-values summary').click();await expect(page.locator('#graph-table-body tr').first()).toContainText('Outside plotting range');
 await sample(page,'10^(-400)');await expect(page.locator('#graph-plot')).toBeVisible();
 await sample(page,'10^308*x','-1','1');await expect(page.locator('path.graph-line')).toBeVisible();
 expect(await page.locator('#graph-plot').evaluate(svg=>!/NaN|Infinity/.test(svg.innerHTML))).toBe(true);
 await sample(page,'0');await expect(page.locator('#graph-plot')).toBeVisible();await expect(page.locator('path.graph-line')).toHaveCount(1);
});
test('automatic parameter uses read-only session values and detects ambiguity',async({page,request})=>{
 await page.goto('/?lang=en');await page.locator('#expression').fill('a=2');await page.locator('#expression').press('Enter');await expect(page.locator('#result')).toHaveText('2');
 const client=await page.evaluate(()=>sessionStorage.getItem('numforge-client-id'));
 const read=async()=>Promise.all(['/api/session','/api/session/variables','/api/session/history'].map(async p=>(await request.get(p+'?client='+client)).json()));
 const before=await read();await page.evaluate(()=>window.graphHeader=document.querySelector('.page-header'));
 await page.locator('.primary-nav a[href^="/graph"]').click();await expect(page.locator('#graph-form')).toBeVisible();
 await page.locator('#graph-session').check();const result=await sample(page,'a*t');expect(result.parameter).toBe('t');expect(await read()).toEqual(before);
 await page.locator('.language-switch a[lang=sk]').click();await expect(page.locator('#graph-expression')).toHaveValue('a*t');
 await sample(page,'a*t');await expect(page.locator('path.graph-line')).toBeVisible();expect(await page.evaluate(()=>window.graphHeader===document.querySelector('.page-header'))).toBe(true);
 await page.locator('#graph-session').uncheck();await sample(page,'x+y');await expect(page.locator('#graph-status')).toContainText('jednu neznámu');
});
test('editing cancels a pending adaptive plot',async({page})=>{
 let release;const paused=new Promise(resolve=>release=resolve);
 await page.route('**/api/graph/plot?**',async route=>{const response=await route.fetch();await paused;try{await route.fulfill({response});}catch(_) {}});
 await page.goto('/graph?lang=en');await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).toHaveAttribute('aria-busy','true');
 await page.locator('#graph-expression').fill('x*x');release();await expect(page.locator('#graph-status')).toHaveText('Enter a function and plot it.');await expect(page.locator('#graph-plot')).toBeVisible();
});
test('default view, automatic t, degree angles and resize',async({page})=>{
 await page.goto('/graph?lang=en');await expect(page.locator('.graph-settings')).not.toHaveAttribute('open','');
 await expect(page.locator('#graph-min')).toHaveValue('-10');await expect(page.locator('#graph-max')).toHaveValue('10');
 await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toBeVisible();
 await sample(page,'sin(t)','0','90');await page.locator('#graph-angle').selectOption('deg');await page.locator('#graph-submit').click();
 await expect(page.locator('#graph-status')).toContainText('f(t)');await page.locator('#graph-values summary').click();await expect(page.locator('#graph-table-body tr').last()).toContainText('1E+0');
 await page.locator('.graph-settings summary').click();await page.locator('#graph-values summary').click();
 await page.screenshot({path:'C:/Incoming/Projekty/NumForge/build/adaptive-graph-desktop.png',fullPage:true});
 await page.setViewportSize({width:390,height:844});await expect(page.locator('#graph-plot')).toBeVisible();
 await page.screenshot({path:'C:/Incoming/Projekty/NumForge/build/adaptive-graph-mobile.png',fullPage:true});
});
