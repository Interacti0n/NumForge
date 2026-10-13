const {test,expect}=require('@playwright/test');
async function draw(page,input='sin(x)'){
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill(input);
 const response=page.waitForResponse(r=>r.url().includes('/api/graph/plot?'));
 await page.locator('#graph-submit').click();await response;
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 await expect(page.locator('path.graph-line')).toBeVisible();
}
test('fixed origin view and integer ticks stay independent of function extrema',async({page})=>{
 await draw(page);const svg=page.locator('#graph-plot');
 for(const [name,value] of [['xmin','-10'],['xmax','10'],['ymin','-10'],['ymax','10']])await expect(svg).toHaveAttribute('data-'+name,value);
 for(const axis of ['x','y']){
  const labels=await page.locator(`.graph-tick[data-axis="${axis}"]`).allTextContents();
  expect(labels).toContain('0');expect(labels.every(t=>/^-?\d+$/.test(t))).toBe(true);
 }
 await page.locator('.graph-result').screenshot({path:'C:/Incoming/Projekty/NumForge/build/graph-sine-origin.png'});
 await page.locator('#graph-expression').fill('1000*x');await page.locator('#graph-submit').click();
 await expect(svg).toHaveAttribute('data-ymin','-10');await expect(svg).toHaveAttribute('data-ymax','10');
 await page.locator('.graph-result').screenshot({path:'C:/Incoming/Projekty/NumForge/build/graph-fixed-camera.png'});
});
test('initial buffered interval supports immediate horizontal and vertical movement',async({page})=>{
 const calls=[];page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))calls.push(new URL(r.url()));});
 await draw(page);expect(calls[0].searchParams.get('min')).toBe('-30');expect(calls[0].searchParams.get('max')).toBe('30');
 expect(calls[0].searchParams.get('ymin')).toBe('-10');expect(calls[0].searchParams.get('ymax')).toBe('10');
 await page.locator('#graph-plot').focus();
 for(let i=0;i<3;++i)await page.locator('#graph-plot').press('ArrowRight');
 await page.locator('#graph-plot').press('ArrowUp');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','2');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymin','-6');
 await expect(page.locator('path.graph-line')).toBeVisible();await page.waitForTimeout(450);
 expect(calls.some(u=>u.searchParams.get('min')==='2'&&u.searchParams.get('max')==='22'&&Number(u.searchParams.get('ymax'))-Number(u.searchParams.get('ymin'))===20)).toBe(false);
 await page.locator('#graph-reset').click();
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymin','-10');
});
test('zoom grows decimal precision and always retains an exact zero tick',async({page})=>{
 await draw(page,'x');
 for(let i=0;i<4;++i)await page.locator('#graph-zoom-in').click();
 let labels=await page.locator('.graph-tick[data-axis="x"]').allTextContents();
 expect(labels).toContain('0');expect(labels.some(t=>/\.\d$/.test(t))).toBe(true);
 for(let i=0;i<4;++i)await page.locator('#graph-zoom-in').click();
 labels=await page.locator('.graph-tick[data-axis="x"]').allTextContents();
 expect(labels).toContain('0');expect(labels.some(t=>/\.\d{2}$/.test(t))).toBe(true);expect(labels).not.toContain('-0');
});
test('mouse drag moves both axes and a fresh page starts at the origin',async({page})=>{
 await draw(page);const rect=await page.locator('#graph-plot').boundingBox(),x=rect.x+rect.width/2,y=rect.y+rect.height/2;
 await page.mouse.move(x,y);await page.mouse.down();await page.mouse.move(x+90,y-50,{steps:5});await page.mouse.up();
 expect(Number(await page.locator('#graph-plot').getAttribute('data-ymin'))).toBeLessThan(-10);
 expect(Number(await page.locator('#graph-plot').getAttribute('data-xmin'))).toBeLessThan(-10);
 await page.reload();await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymin','-10');
});

test('pure vertical pan reprojects the buffered curve without a graph request',async({page})=>{
 const calls=[];page.on('request',r=>{if(r.url().includes('/api/graph/plot?'))calls.push(r.url());});
 const prepared=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&new URL(r.url()).searchParams.get('ymin')==='-5');
 await draw(page);await prepared;const before=calls.length;
 await page.locator('#graph-plot').focus();
 for(let i=0;i<3;++i)await page.locator('#graph-plot').press('ArrowUp');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymin','2');
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');
 await page.waitForTimeout(450);expect(calls.length).toBe(before);
 await page.locator('#graph-plot').press('Home');await expect(page.locator('#graph-plot')).toHaveAttribute('data-ymin','-10');
});

test('limited warning follows visible gaps rather than the whole prepared region',async({page})=>{
 await page.route('**/api/graph/plot?**',route=>route.fulfill({json:{ok:true,schema_version:3,evaluation:'double',
  parameter:'x',quality:'refined',width:2048,height:440,count:9,limited:true,y_min:-.3,y_max:.3,
  points:[-30,-20,-10,-5,0,5,10,20,30].map((x,i)=>[x,x/100,i&&x!==20?1:0,0,1])}}));
 await draw(page);await expect(page.locator('#graph-status')).not.toContainText('could not be resolved');
 await page.locator('#graph-plot').focus();await page.locator('#graph-plot').press('ArrowRight');
 await expect(page.locator('#graph-status')).toContainText('Some visible intervals could not be resolved');
 // Moving the same unresolved edge outside the vertical camera also hides the warning.
 for(let i=0;i<3;++i)await page.locator('#graph-plot').press('ArrowUp');
 await expect(page.locator('#graph-status')).not.toContainText('could not be resolved');
});
