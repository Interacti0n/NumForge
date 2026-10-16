const {test,expect}=require('@playwright/test');
const expressions=['sin(33*x)','cos(31*x)','sin(17*x)','cos(13*x)'];
async function markerData(page){
 return page.locator('.graph-feature').evaluateAll(nodes=>nodes.map(node=>{
  const p=new DOMPoint(Number(node.getAttribute('cx')),Number(node.getAttribute('cy'))).matrixTransform(node.getScreenCTM());
  return{x:Number(node.dataset.x),y:Number(node.dataset.y),curve:node.dataset.curve,screenX:p.x,screenY:p.y};
 }));
}
function spaced(points){for(let i=0;i<points.length;i++)for(let j=0;j<i;j++)expect(Math.hypot(points[i].screenX-points[j].screenX,points[i].screenY-points[j].screenY)).toBeGreaterThanOrEqual(23.99);}
for(const [width,height] of [[390,844],[844,390],[1280,900]])test(`marker density uses CSS spacing and keeps verified coordinates at ${width}x${height}`,async({browser,baseURL})=>{
 test.setTimeout(30000);
 const context=await browser.newContext({baseURL,viewport:{width,height},hasTouch:width<900}),page=await context.newPage(),pending=[],verified=[];
 try{
  page.on('response',response=>{if(response.url().endsWith('/api/graph/verify'))pending.push(response.json().then(body=>verified.push(...(body.points||[]).filter(Boolean))).catch(()=>{}));});
  await page.goto('/graph?lang=en');
  for(let i=0;i<4;i++){if(i)await page.locator('#graph-add').click();await page.locator('.graph-curve').nth(i).locator('input:not([type=color])').fill(expressions[i]);}
  await page.locator('#graph-submit').click();const svg=page.locator('#graph-plot');
  await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(svg).toHaveAttribute('data-features-state','ready');
  await page.waitForTimeout(350);await expect(svg).toHaveAttribute('data-features-state','ready');await Promise.allSettled(pending);
  let dots=await markerData(page);const count=Number(await svg.getAttribute('data-feature-count'));
  expect(count).toBeGreaterThan(40);expect(dots.length).toBeGreaterThan(3);
  if(width<900){expect(dots.length).toBeLessThan(count);expect(new Set(dots.map(p=>p.curve)).size).toBe(4);await expect(svg).toHaveAttribute('data-marker-spacing','24');spaced(dots);}
  else{expect(dots.length).toBe(count);await expect(svg).toHaveAttribute('data-marker-spacing','0');}
  let requests=0;page.on('request',r=>{if(r.url().includes('/api/graph/'))requests++;});
  const coordinates=dots.map(p=>[p.x,p.y]);await page.locator('#graph-color').evaluate(input=>{input.value='#ff2233';input.dispatchEvent(new Event('input',{bubbles:true}));});
  await page.evaluate(()=>new Promise(requestAnimationFrame));expect((await markerData(page)).map(p=>[p.x,p.y])).toEqual(coordinates);
  if(width<900){
   await svg.scrollIntoViewIfNeeded();dots=await markerData(page);
   const visible=dots.find(point=>point.curve!=='1');expect(visible).toBeDefined();await page.touchscreen.tap(visible.screenX,visible.screenY);
   await expect(page.locator('#graph-inspector')).toHaveAttribute('data-curve',visible.curve);
   // Pick a confirmed coordinate with no persistent visible dot. It must still
   // snap to its actual C-verified value, without another graph request.
   const hidden=await svg.evaluate((node,{candidates,shown})=>{
    const box=node.viewBox.baseVal,matrix=node.getScreenCTM();
    for(const point of candidates){if(Math.abs(point.x)>9||Math.abs(point.y)>9)continue;
     const screen=new DOMPoint(64+(point.x-Number(node.dataset.xmin))/(Number(node.dataset.xmax)-Number(node.dataset.xmin))*(box.width-86),box.height-46-(point.y-Number(node.dataset.ymin))/(Number(node.dataset.ymax)-Number(node.dataset.ymin))*(box.height-68)).matrixTransform(matrix);
     if(screen.y<5||screen.y>innerHeight-5)continue;
     if(shown.every(p=>Math.hypot(p.screenX-screen.x,p.screenY-screen.y)>2))return {...point,screenX:screen.x,screenY:screen.y};
    }return null;
   },{candidates:verified,shown:dots});
   expect(hidden).not.toBeNull();await page.touchscreen.tap(hidden.screenX,hidden.screenY);
   await expect(page.locator('#graph-inspector')).toBeVisible();expect(Number(await page.locator('#graph-inspector').getAttribute('data-x'))).toBeCloseTo(hidden.x,5);expect(Number(await page.locator('#graph-inspector').getAttribute('data-y'))).toBeCloseTo(hidden.y,5);
   await expect(page.locator('.graph-inspection')).toHaveAttribute('visibility','visible');
  }
  await page.waitForTimeout(200);expect(requests).toBe(0);
  await page.locator('#graph-expand').click();await expect(svg).toHaveAttribute('data-features-state','ready');
  if(width<900){await expect(svg).toHaveAttribute('data-marker-spacing','24');spaced(await markerData(page));}
  else await expect(svg).toHaveAttribute('data-marker-spacing','0');
  await page.locator('.graph-result').screenshot({path:test.info().outputPath('marker-density.png')});
 }finally{await context.close();}
});
