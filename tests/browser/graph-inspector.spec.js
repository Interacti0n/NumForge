const { artifactPath } = require('./artifacts');
const {test,expect}=require('@playwright/test');
test.use({hasTouch:true});
async function point(page,x,y){await page.locator('#graph-plot').scrollIntoViewIfNeeded();return page.locator('#graph-plot').evaluate((svg,{x,y})=>{const w=svg.viewBox.baseVal.width,h=svg.viewBox.baseVal.height,px=64+(x-Number(svg.dataset.xmin))/(Number(svg.dataset.xmax)-Number(svg.dataset.xmin))*(w-86),py=h-46-(y-Number(svg.dataset.ymin))/(Number(svg.dataset.ymax)-Number(svg.dataset.ymin))*(h-68),p=new DOMPoint(px,py).matrixTransform(svg.getScreenCTM());return{x:p.x,y:p.y};},{x,y});}
async function draw(page,expression='sin(x)',lang='en'){await page.goto('/graph?lang='+lang);await page.locator('#graph-expression').fill(expression);await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('path.graph-line')).toBeAttached();await expect(page.locator('#graph-plot')).toBeVisible();await expect(page.locator('#graph-plot')).toHaveAttribute('data-features-state',/ready|limited/);await page.waitForTimeout(100);}
async function hover(page,x,y){
 const p=await point(page,x,y);
 await page.locator('#graph-plot').evaluate(svg=>svg.addEventListener('pointermove',event=>{
  const q=new DOMPoint(event.clientX,event.clientY).matrixTransform(svg.getScreenCTM().inverse()),span=Number(svg.dataset.xmax)-Number(svg.dataset.xmin);
  svg.testPointer={x:Number(svg.dataset.xmin)+(q.x-64)/(svg.viewBox.baseVal.width-86)*span,clientX:event.clientX,clientY:event.clientY};
 },{once:true}));
 await page.mouse.move(p.x,p.y);
 const actual=await page.locator('#graph-plot').evaluate(svg=>svg.testPointer);
 expect(Math.abs(actual.clientX-p.x)).toBeLessThanOrEqual(1);
 expect(Math.abs(actual.clientY-p.y)).toBeLessThanOrEqual(1);
 return actual.x;
}
async function exactHover(page,x,y){const p=await point(page,x,y);await page.locator('#graph-plot').dispatchEvent('pointermove',{pointerType:'mouse',clientX:p.x,clientY:p.y});}
async function value(page){return page.locator('#graph-inspector').evaluate(e=>({x:Number(e.dataset.x),y:Number(e.dataset.y),curve:e.dataset.curve}));}
for(const lang of ['en','sk'])for(const width of [390,1280])test(`cached mouse coordinates follow the rendered sine ${lang} ${width}`,async({page})=>{
 await page.setViewportSize({width,height:844});await draw(page,'sin(x)',lang);const readout=page.locator('#graph-inspector');let calls=0;page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))++calls;});
 await page.locator('#graph-plot').evaluate(e=>window.originalDrawing=e.querySelector('.graph-drawing'));
 for(const x of [-2.1,0,1.25,Math.PI/2]){const actualX=await hover(page,x,Math.sin(x));await expect(readout).toBeVisible();if(x===0){await expect.poll(async()=>(await value(page)).x).toBeCloseTo(0,5);expect(Math.abs(actualX)).toBeLessThan(.1);await expect(readout).toHaveAttribute('data-feature',/axis/);}else{await expect.poll(async()=>(await value(page)).x).toBeCloseTo(actualX,5);await expect(page.locator('#graph-inspector-name')).toHaveText('1. sin(x)');}const data=await value(page);expect(data.y).toBeCloseTo(Math.sin(data.x),3);await expect(page.locator('.graph-inspection')).toHaveAttribute('visibility','visible');}
 await exactHover(page,1.25,1);await expect(page.locator('#graph-inspector-coordinates')).toContainText(lang==='sk'?'1,25':'1.25');
 expect(calls).toBe(0);expect(await page.locator('#graph-plot').evaluate(e=>window.originalDrawing===e.querySelector('.graph-drawing'))).toBe(true);
 await page.locator('.graph-result').screenshot({path:artifactPath(`graph-inspector-${lang}-${width}.png`)});
 await exactHover(page,0,0);await expect(page.locator('#graph-inspector-coordinates')).toHaveText('≈ x = 0, y = 0');
 await page.mouse.move(0,0);await expect(readout).toBeHidden();expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBe(true);
});
test('the closest visible function owns the marker and color; edits and hiding remove stale inspection',async({page})=>{
 await draw(page,'0');await page.locator('#graph-add').click();await page.locator('.graph-curve').last().locator('input:not([type=color])').fill('2');await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await page.waitForTimeout(400);
 const id=await page.locator('.graph-curve').last().getAttribute('data-curve');await hover(page,1,1.9);await expect(page.locator('#graph-inspector')).toHaveAttribute('data-curve',id);expect((await value(page)).y).toBe(2);await expect(page.locator('#graph-inspector-name')).toHaveText('2. 2');
 await page.locator('.graph-curve').last().locator('input[type=color]').evaluate(e=>{e.value='#ff2233';e.dispatchEvent(new Event('input',{bubbles:true}));});await expect(page.locator('.graph-inspection-dot')).toHaveCSS('fill','rgb(255, 34, 51)');
 const p=await point(page,1,2);await page.touchscreen.tap(p.x,p.y);await page.locator('#graph-legend button').last().click();await expect(page.locator('#graph-inspector')).toBeHidden();
 await page.locator('#graph-legend button').last().click();await hover(page,1,2);await expect(page.locator('#graph-inspector')).toHaveAttribute('data-curve',id);await page.locator('.graph-curve').last().locator('input:not([type=color])').fill('x+y');await expect(page.locator('#graph-inspector')).toHaveAttribute('data-curve',await page.locator('.graph-curve').first().getAttribute('data-curve'));expect((await value(page)).y).toBe(0);
});
for(const expression of ['1/x','sqrt(x)'])test(`coordinates do not bridge domain gaps in ${expression}`,async({page})=>{
 await draw(page,expression);await hover(page,expression==='1/x'?0:-2,0);await expect(page.locator('#graph-inspector')).toBeHidden();await hover(page,2,expression==='1/x'?.5:Math.sqrt(2));await expect(page.locator('#graph-inspector')).toBeVisible();const data=await value(page);expect(data.y).toBeCloseTo(expression==='1/x'?1/data.x:Math.sqrt(data.x),3);
});
test('a touch-selected point follows its curve without panning or graph requests',async({page,browserName})=>{
 test.skip(browserName!=='chromium','Native multi-touch injection uses the Chromium DevTools protocol.');
 await page.setViewportSize({width:390,height:844});await draw(page);let calls=0;page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))++calls;});
 const camera=await page.locator('#graph-plot').evaluate(e=>[e.dataset.xmin,e.dataset.xmax,e.dataset.ymin,e.dataset.ymax]);const cdp=await page.context().newCDPSession(page),a=await point(page,1,Math.sin(1)),b=await point(page,2,Math.sin(2));
 await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{id:1,x:a.x,y:a.y}]});await expect(page.locator('#graph-inspector')).toBeVisible();await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{id:1,x:b.x,y:b.y}]});await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
 await expect.poll(async()=>Math.abs((await value(page)).x-2)).toBeLessThan(.02);const data=await value(page);expect(data.y).toBeCloseTo(Math.sin(data.x),3);await expect(page.locator('#graph-inspector')).toBeVisible();expect(await page.locator('#graph-plot').evaluate(e=>[e.dataset.xmin,e.dataset.xmax,e.dataset.ymin,e.dataset.ymax])).toEqual(camera);expect(calls).toBe(0);
 await page.locator('#graph-expand').click();await expect(page.locator('#graph-inspector')).toBeVisible();await expect(page.locator('#graph-expanded .graph-inspection')).toHaveAttribute('visibility','visible');await page.keyboard.press('Escape');await expect(page.locator('#graph-expanded')).not.toHaveAttribute('open','');
});
test('touch inspection yields to a second finger and an empty-area drag still pans',async({page,browserName})=>{
 test.skip(browserName!=='chromium','Native multi-touch injection uses the Chromium DevTools protocol.');
 await page.setViewportSize({width:390,height:844});await draw(page);const cdp=await page.context().newCDPSession(page),a=await point(page,0,0),b=await point(page,5,0);
 await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{id:1,x:a.x,y:a.y}]});await expect(page.locator('#graph-inspector')).toBeVisible();await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{id:1,x:a.x,y:a.y},{id:2,x:b.x,y:b.y}]});await expect(page.locator('#graph-inspector')).toBeHidden();await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{id:1,x:a.x-15,y:a.y},{id:2,x:b.x+15,y:b.y}]});await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});await expect.poll(async()=>Number(await page.locator('#graph-plot').getAttribute('data-xmax'))-Number(await page.locator('#graph-plot').getAttribute('data-xmin'))).toBeLessThan(20);
 const before=await page.locator('#graph-plot').getAttribute('data-xmin'),empty=await point(page,0,4);await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{id:3,x:empty.x,y:empty.y}]});await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{id:3,x:empty.x+30,y:empty.y}]});await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});await expect(page.locator('#graph-plot')).not.toHaveAttribute('data-xmin',before);
});
test('rapid inspection is frame-coalesced and leaves the plot geometry and network untouched',async({page})=>{
 await draw(page);let calls=0;page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))++calls;});const p=await point(page,1,1);
 const diagnostic=await page.locator('#graph-plot').evaluate(async(svg,p)=>{const original=svg.querySelector('.graph-drawing'),begin=performance.now();let mutations=0;const observer=new MutationObserver(()=>++mutations);observer.observe(document.querySelector('#graph-inspector-coordinates'),{childList:true});for(let i=0;i<200;i++)svg.dispatchEvent(new PointerEvent('pointermove',{pointerType:'mouse',clientX:p.x+i*.01,clientY:p.y,bubbles:true}));await new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)));observer.disconnect();return{elapsed_ms:performance.now()-begin,readout_updates:mutations,same_drawing:svg.querySelector('.graph-drawing')===original};},p);
 expect(diagnostic.same_drawing).toBe(true);expect(diagnostic.readout_updates).toBe(1);expect(calls).toBe(0);await expect(page.locator('#graph-inspector')).toBeVisible();test.info().annotations.push({type:'inspection diagnostic',description:JSON.stringify(diagnostic)});await test.info().attach('inspection-performance.json',{body:JSON.stringify(diagnostic),contentType:'application/json'});console.log('Inspection burst:',JSON.stringify(diagnostic));
 await page.locator('#graph-plot').press('Escape');await expect(page.locator('#graph-inspector')).toBeHidden();
});
