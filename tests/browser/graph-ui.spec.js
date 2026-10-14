const {test,expect}=require('@playwright/test');
const normalize=data=>data.schema_version===3?{...data,points:data.points.map(p=>({x:String(p[0]),y:p[1]===null?null:String(p[1]),connect:!!p[2],kind:['real','domain_error','complex','quantity','unresolved'][p[3]],source_approximate:!!p[4]}))}:data;
test('dense background preparation preserves the active cached viewport',async({page})=>{
 let calls=0;
 await page.route('**/api/graph/plot?**',route=>{
  const q=new URL(route.request().url()).searchParams,low=Number(q.get('min')),high=Number(q.get('max'));
  ++calls;
  return route.fulfill({json:{ok:true,schema_version:3,evaluation:'double',parameter:'x',quality:'refined',width:Number(q.get('width')),height:440,
   limited:false,count:8192,y_min:-1,y_max:1,points:Array.from({length:8192},(_,i)=>{const x=low+(high-low)*i/8191;return [x,Math.sin(x),i?1:0,0,1];})}});
 });
 await page.goto('/graph?lang=en');await sample(page,'sin(x)');
 await expect.poll(()=>calls).toBe(2);
 const original=await page.locator('path.graph-line').getAttribute('d');
 await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toBeVisible();
 await page.waitForTimeout(350);expect(calls).toBe(2);expect(await page.locator('path.graph-line').getAttribute('d')).toBe(original);
});
test('dense cached camera motion keeps the curve visible across animation frames',async({page})=>{
 await page.goto('/graph?lang=en');await sample(page,'sin(33x)');
 const gaps=await page.evaluate(async()=>{
  const svg=document.querySelector('#graph-plot'),rect=svg.getBoundingClientRect(),frames=[];
  let last=await new Promise(resolve=>requestAnimationFrame(resolve));
  for(let i=0;i<20;++i){svg.dispatchEvent(new WheelEvent('wheel',{deltaY:i%2?1:-1,clientX:rect.left+rect.width/2,bubbles:true,cancelable:true}));
   const now=await new Promise(resolve=>requestAnimationFrame(resolve));frames.push(now-last);last=now;
   if(!svg.querySelector('path.graph-line'))throw Error('curve disappeared during camera movement');
  }return frames;
 });
 require('node:fs').writeFileSync('C:/Incoming/Projekty/NumForge/build/graph-motion-frames.json',JSON.stringify(gaps));
 expect(gaps).toHaveLength(20);await expect(page.locator('path.graph-line')).toBeVisible();
});
test('covered viewport reuses accepted cached preview before refinement and cancels obsolete work',async({page})=>{
 await page.goto('/graph?lang=en');await sample(page,'sin(33x)');
 let release,waiting=false;const calls=[],held=new Promise(resolve=>release=resolve);
 await page.route('**/api/graph/plot?**',async route=>{
  const q=new URL(route.request().url()).searchParams;
  if(q.get('min')==='-2.5'&&q.get('max')==='2.5')calls.push(q.get('quality'));
  if(q.get('min')==='-2.5'&&q.get('quality')==='refined'){
   const response=await route.fetch();waiting=true;await held;try{await route.fulfill({response});}catch(_){}
  }else await route.continue();
 });
 await page.evaluate(()=>{document.querySelector('#graph-plot').dispatchEvent(new KeyboardEvent('keydown',{key:'+',bubbles:true,cancelable:true}));document.querySelector('#graph-plot').dispatchEvent(new KeyboardEvent('keydown',{key:'+',bubbles:true,cancelable:true}));});await expect.poll(()=>waiting).toBe(true);expect(calls).toEqual(['refined']);
 await expect(page.locator('.graph-workspace')).toHaveAttribute('aria-busy','true');
 await expect(page.locator('path.graph-line')).toBeVisible();
 await page.locator('#graph-expression').fill('x*x');release();
 await expect(page.locator('#graph-status')).toHaveText('Enter a function and plot it.');
 await expect(page.locator('path.graph-line')).toHaveCount(0);
});
test('dense screen samples render without a point table',async({page})=>{
 await page.goto('/graph?lang=en');const data=await sample(page,'tan(x)');expect(data.count).toBeGreaterThan(32);expect(data.count).toBeLessThanOrEqual(8192);
 if(data.screen_clipped)expect(data.count).toBeLessThan(1024);
 await expect(page.locator('path.graph-line')).toBeVisible();
 expect(await page.locator('#graph-plot').evaluate(svg=>/NaN|Infinity/.test(svg.innerHTML))).toBe(false);
 await expect(page.locator('#graph-values,#graph-table-body')).toHaveCount(0);
});
test('dense range cache evicts distant samples before returning home',async({page})=>{
 const calls=[];
 await page.route('**/api/graph/plot?**',route=>{
  const q=new URL(route.request().url()).searchParams,low=Number(q.get('min')),high=Number(q.get('max'));calls.push([low,high]);
  return route.fulfill({json:{ok:true,schema_version:3,evaluation:'double',parameter:'x',limited:true,quality:'refined',width:Number(q.get('width')),height:440,count:2048,y_min:low,y_max:high,
   points:Array.from({length:2048},(_,i)=>{const x=low+(high-low)*i/2047;return [x,x,i?1:0,0,1];})}});
 });
 await page.goto('/graph?lang=en');await sample(page,'x');await page.locator('#graph-plot').focus();
 for(let i=0;i<8;++i){
  for(let j=0;j<10;++j)await page.locator('#graph-plot').press('ArrowRight');
  await page.waitForTimeout(350);await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 }
 const before=calls.length;await page.locator('#graph-plot').press('Home');
 await expect.poll(()=>calls.length).toBeGreaterThan(before);
 await expect(page.locator('path.graph-line')).toBeVisible();
});
test('returning to a cached viewport preserves curve geometry and axis labels',async({page})=>{
 await page.goto('/graph?lang=en');await sample(page,'x*x');
 const snapshot=()=>page.locator('#graph-plot').evaluate(svg=>({
  paths:[...svg.querySelectorAll('path.graph-line')].map(p=>p.getAttribute('d')),
  ticks:[...svg.querySelectorAll('.graph-tick')].map(p=>p.textContent)
 }));
 const original=await snapshot();
 await page.locator('#graph-plot').focus();await page.locator('#graph-plot').press('ArrowRight');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-6');
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 await page.locator('#graph-plot').press('ArrowLeft');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');
 await expect.poll(snapshot).toEqual(original);
});
test('zoom stops before coordinate precision becomes unreliable and reset still works',async({page})=>{
 await page.goto('/graph?lang=en');
 await page.evaluate(()=>{for(let i=0;i<80;i++)document.querySelector('#graph-plot').dispatchEvent(new KeyboardEvent('keydown',{key:'+',bubbles:true,cancelable:true}));});
 const low=Number(await page.locator('#graph-plot').getAttribute('data-xmin')),high=Number(await page.locator('#graph-plot').getAttribute('data-xmax'));
 expect(high-low).toBeGreaterThanOrEqual(1e-8);
 await page.locator('#graph-plot').dispatchEvent('keydown',{key:'+'});
 expect(Number(await page.locator('#graph-plot').getAttribute('data-xmin'))).toBe(low);
 expect(Number(await page.locator('#graph-plot').getAttribute('data-xmax'))).toBe(high);
 await page.locator('#graph-reset').click();
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmax','10');
});
test('off-center deep zoom has distinct axes and no invalid projection',async({page})=>{
 await page.goto('/graph?lang=en');await sample(page,'sin(x)');
 await page.evaluate(()=>{
  const svg=document.querySelector('#graph-plot'),rect=svg.getBoundingClientRect(),w=svg.viewBox.baseVal.width,h=svg.viewBox.baseVal.height;
  const clientX=rect.left+rect.width*(64+(w-86)*((10-Math.PI*1.5)/20))/w;
  const clientY=rect.top+rect.height*(h-46-(h-68)*.55)/h;
  for(let i=0;i<60;++i)svg.dispatchEvent(new WheelEvent('wheel',{deltaY:-1,clientX,clientY,bubbles:true,cancelable:true}));
 });
 await expect.poll(()=>page.locator('.graph-tick[data-axis=x]').allTextContents()).not.toEqual([]);
 for(const axis of ['x','y']){const labels=await page.locator(`.graph-tick[data-axis=${axis}]`).allTextContents();expect(new Set(labels).size).toBe(labels.length);}
 expect(await page.locator('#graph-plot').evaluate(el=>!/NaN|Infinity/.test(el.innerHTML))).toBe(true);
});
test('cached tile composition preserves reciprocal gaps',async({page})=>{
 await page.goto('/graph?lang=en');await sample(page,'1/x');
 await page.locator('#graph-plot').focus();await page.locator('#graph-plot').press('ArrowRight');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-6');
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 const paths=await page.locator('path.graph-line').evaluateAll(nodes=>nodes.map(n=>n.getAttribute('d')));
 const width=await page.locator('#graph-plot').evaluate(svg=>svg.viewBox.baseVal.width);
 const zero=64+6/20*(width-86);
 for(const path of paths){let previous;
  for(const command of path.matchAll(/([MLC])([^MLC]+)/g)){
   const numbers=command[2].split(/[ ,]+/).map(Number),x=numbers.at(-2);if(command[1]!=='M')expect(previous<zero&&x>zero).toBe(false);previous=x;
  }
 }
});
test('partial cache requests only the missing interval and expression changes recompute',async({page})=>{
 await page.goto('/graph?lang=en');await sample(page,'sin(x)');
 // Wait for optional buffering, then disable work to expose the missing tail.
 await page.waitForTimeout(350);
 await page.route('**/api/graph/plot?**',route=>{
  const q=new URL(route.request().url()).searchParams;
  return Number(q.get('min'))>=30&&Number(q.get('max'))<=34?route.continue():route.abort();
 });
 let ready=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&new URL(r.url()).searchParams.get('min')==='30'&&new URL(r.url()).searchParams.get('max')==='34');
 await page.locator('#graph-plot').focus();
 for(let i=0;i<6;++i)await page.locator('#graph-plot').press('ArrowRight');
 const response=await ready;expect(new URL(response.url()).searchParams.get('max')).toBe('34');
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 await page.unroute('**/api/graph/plot?**');
 ready=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&r.request().postData()==='cos(x)');
 await page.locator('#graph-expression').fill('cos(x)');await page.locator('#graph-submit').click();await ready;
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');

});
test('prepared neighboring ranges pan immediately without another foreground calculation',async({page})=>{
 const responses=[];page.on('response',r=>{if(r.url().includes('/api/graph/plot?')&&r.ok())responses.push(new URL(r.url()));});
 await page.goto('/graph?lang=en');await sample(page,'sin(x)');
 await expect.poll(()=>responses.some(u=>u.searchParams.get('min')==='-30')&&responses.some(u=>u.searchParams.get('max')==='30'),{timeout:10000}).toBe(true);
 const before=responses.length;
 await page.locator('#graph-plot').focus();await page.locator('#graph-plot').press('ArrowRight');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-6');await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmax','14');
 await expect(page.locator('path.graph-line')).toHaveCount(1);await expect(page.locator('#graph-status')).not.toHaveText('Sampling…');
 await page.waitForTimeout(450);
 expect(responses.slice(before).some(u=>u.searchParams.get('min')==='-6'&&u.searchParams.get('max')==='14'&&Number(u.searchParams.get('ymax'))-Number(u.searchParams.get('ymin'))===20)).toBe(false);
});
test('uncached refinement keeps the previous curve visible and changes invalidate the cache',async({page})=>{
 await page.goto('/graph?lang=en');await sample(page,'sin(x)');
 let release;const held=new Promise(resolve=>release=resolve);
 await page.route('**/api/graph/plot?**',async route=>{
  const u=new URL(route.request().url());
  if(u.searchParams.get('min')==='-2.5'&&u.searchParams.get('max')==='2.5'){
   const response=await route.fetch();await held;try{await route.fulfill({response});}catch(_){}
  }else await route.continue();
 });
 await page.evaluate(()=>{document.querySelector('#graph-plot').dispatchEvent(new KeyboardEvent('keydown',{key:'+',bubbles:true,cancelable:true}));document.querySelector('#graph-plot').dispatchEvent(new KeyboardEvent('keydown',{key:'+',bubbles:true,cancelable:true}));});await expect(page.locator('path.graph-line')).toBeVisible();
 await expect(page.locator('.graph-workspace')).toHaveAttribute('aria-busy','true');
 await expect(page.locator('path.graph-line')).toBeVisible();release();
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 const response=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&r.request().postData()==='cos(x)');
 await page.locator('#graph-expression').fill('cos(x)');await page.locator('#graph-submit').click();await response;
 await expect(page.locator('path.graph-line')).toBeVisible();
});
test('zoomed-out sine retains its sampled extrema',async({page})=>{
 await page.setViewportSize({width:1440,height:1000});await page.goto('/graph?lang=sk');await sample(page,'sin(x)');
 const ready=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&new URL(r.url()).searchParams.get('min')==='-640'&&new URL(r.url()).searchParams.get('quality')==='refined');
 for(let i=0;i<6;++i)await page.locator('#graph-plot').dispatchEvent('keydown',{key:'-'});
 const data=await (await ready).json();expect(data.limited).toBe(false);expect(Number(data.y_min)).toBe(-1);expect(Number(data.y_max)).toBe(1);
});
test('editing cancels queued view resampling',async({page})=>{
 await page.goto('/graph?lang=en');await sample(page,'x*x');let calls=0;
 page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))++calls;});
 await page.evaluate(()=>{
  document.querySelector('#graph-plot').dispatchEvent(new KeyboardEvent('keydown',{key:'+',bubbles:true,cancelable:true}));
  const input=document.querySelector('#graph-expression');input.value='sin(x)';input.dispatchEvent(new Event('input',{bubbles:true}));
 });
 await page.waitForTimeout(450);expect(calls).toBe(0);
 await expect(page.locator('#graph-status')).toHaveText('Enter a function and plot it.');
 await expect(page.locator('path.graph-line')).toHaveCount(0);
});
test('zoom, pan, keyboard and reset preserve the chosen two-dimensional camera',async({page})=>{
 await page.goto('/graph?lang=en');await sample(page,'x*x');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymin','-10');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymax','10');
 let ready=page.waitForResponse(r=>r.url().includes('/api/graph/plot?'));
 await page.locator('#graph-plot').dispatchEvent('keydown',{key:'+'});await ready;
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymin','-5');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymax','5');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-5');await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmax','5');
 const svg=page.locator('#graph-plot');await svg.scrollIntoViewIfNeeded();let box=await svg.boundingBox();
 await page.mouse.move(box.x+box.width*.6,box.y+box.height*.5);await page.mouse.down();
 await page.mouse.move(box.x+box.width*.4,box.y+box.height*.5,{steps:6});await page.mouse.up();
 await expect(page.locator('#graph-plot')).not.toHaveAttribute('data-xmin','-5');
 expect(Number(await page.locator('#graph-plot').getAttribute('data-xmin'))).toBeGreaterThan(-5);
 await svg.focus();await svg.press('Home');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');
 await svg.press('+');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmax','5');
 let calls=0;page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))++calls;});
 ready=page.waitForResponse(r=>r.url().includes('/api/graph/plot?'));
 for(let i=0;i<3;++i)await svg.dispatchEvent('wheel',{deltaY:-100,clientX:box.x+box.width*.5});
 await ready;expect(calls).toBe(1);expect(Number(await page.locator('#graph-plot').getAttribute('data-xmax'))-Number(await page.locator('#graph-plot').getAttribute('data-xmin'))).toBeLessThan(10);
 await page.locator('#graph-reset').click();
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmax','10');
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
 expect(Number(await page.locator('#graph-plot').getAttribute('data-xmax'))-Number(await page.locator('#graph-plot').getAttribute('data-xmin'))).toBeCloseTo(20*80/140,6);
 await page.locator('#graph-expression').fill('');let calls=0;page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))++calls;});
 await page.locator('#graph-plot').dispatchEvent('keydown',{key:'+'});await expect(page.locator('#graph-values')).toHaveCount(0);
 await page.locator('#graph-reset').click();await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');
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
async function sample(page,expression) {
 await page.locator('#graph-expression').fill(expression);
 const ready=page.waitForResponse(r=>r.url().includes('/api/graph/plot?'));
 await page.locator('#graph-submit').click();const response=await ready;
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');return normalize(await response.json());
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
 const root=await sample(page,'sqrt(t)');await expect(page.locator('#graph-status')).toContainText('f(t)');
 expect(root.points.some(p=>p.kind==='complex')).toBe(true);
 const quantity=await sample(page,'qty(t;"m")');await expect(page.locator('#graph-plot')).toBeVisible();
 expect(quantity.points.every(p=>p.kind==='quantity')).toBe(true);await expect(page.locator('path.graph-line')).toHaveCount(0);
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBe(true);
 await sample(page,'rand()');await expect(page.locator('#graph-status')).toHaveClass(/error/);
 await expect(page.locator('#graph-values')).toHaveCount(0);
});
test('adaptive browser numeric boundaries remain finite',async({page})=>{
 await page.goto('/graph?lang=en');await sample(page,'10^400');await expect(page.locator('#graph-plot')).toBeVisible();
 await expect(page.locator('path.graph-line')).toHaveCount(0);
 await sample(page,'10^(-400)');await expect(page.locator('#graph-plot')).toBeVisible();
 await sample(page,'10^308*x');await expect(page.locator('#graph-plot')).toBeVisible();
 expect(await page.locator('#graph-plot').evaluate(svg=>!/NaN|Infinity/.test(svg.innerHTML))).toBe(true);
 await sample(page,'0');await expect(page.locator('#graph-plot')).toBeVisible();await expect(page.locator('path.graph-line')).toHaveCount(1);
});
test('graphs infer one parameter independently of calculator variables and ans',async({page,request})=>{
 await page.goto('/?lang=en');await page.locator('#expression').fill('a=2');await page.locator('#expression').press('Enter');await expect(page.locator('#result')).toHaveText('2');
 const client=await page.evaluate(()=>sessionStorage.getItem('numforge-client-id'));
 const read=async()=>Promise.all(['/api/session','/api/session/variables','/api/session/history'].map(async p=>(await request.get(p+'?client='+client)).json()));
 const before=await read();await page.locator('.primary-nav a[href^="/graph"]').click();
 const calls=[];page.on('request',r=>{if(r.url().includes('/api/graph/'))calls.push(new URL(r.url()));});
 await sample(page,'a*t');await expect(page.locator('#graph-status')).toContainText('one variable');
 await sample(page,'ans*x');await expect(page.locator('#graph-status')).toHaveClass(/error/);
 const result=await sample(page,'sin(t)');expect(result.parameter).toBe('t');expect(await read()).toEqual(before);
 expect(calls.every(u=>!u.searchParams.has('client')&&!u.searchParams.has('at')&&u.searchParams.get('angle')==='rad'&&!u.searchParams.has('precision'))).toBe(true);
});
test('editing cancels a pending adaptive plot',async({page})=>{
 let release;const paused=new Promise(resolve=>release=resolve);
 await page.route('**/api/graph/plot?**',async route=>{const response=await route.fetch();await paused;try{await route.fulfill({response});}catch(_) {}});
 await page.goto('/graph?lang=en');await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).toHaveAttribute('aria-busy','true');
 await page.locator('#graph-expression').fill('x*x');release();await expect(page.locator('#graph-status')).toHaveText('Enter a function and plot it.');await expect(page.locator('#graph-plot')).toBeVisible();
});
test('automatic radians, parameter and resize retain a simple graph layout',async({page})=>{
 await page.goto('/graph?lang=en');await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');
 const data=await sample(page,'sin(t)');expect(data.parameter).toBe('t');
 for(const p of data.points)if(p.kind==='real')expect(Number(p.y)).toBeCloseTo(Math.sin(Number(p.x)),9);
 await expect(page.locator('#graph-settings,.graph-settings,#graph-session,#graph-precision,#graph-angle,#graph-min,#graph-max,#graph-ymin,#graph-ymax,#graph-values')).toHaveCount(0);
 await page.screenshot({path:'C:/Incoming/Projekty/NumForge/build/simple-graph-desktop.png',fullPage:true});
 await page.setViewportSize({width:390,height:844});await expect(page.locator('#graph-plot')).toBeVisible();
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBe(true);
 await page.screenshot({path:'C:/Incoming/Projekty/NumForge/build/simple-graph-mobile.png',fullPage:true});
});
