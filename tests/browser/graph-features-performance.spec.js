const {test,expect}=require('@playwright/test');
for(const expressions of [['sin(x)'],['sin(33*x)'],['x','2-x','sin(3*x)','cos(5*x)'],['abs(x*sin(x))','sin(33*x)','cos(17*x)','x*x']]){
 test(`cached redraw with intersection markers: ${expressions.join(', ')}`,async({page})=>{
  await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill(expressions[0]);
  for(const expression of expressions.slice(1)){await page.locator('#graph-add').click();await page.locator('.graph-curve').last().locator('input:not([type=color])').fill(expression);}
  await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('path.graph-line').first()).toBeVisible();await page.waitForTimeout(500);
  // Older comparison snapshots have no asynchronous C confirmation state.
  if(await page.locator('#graph-plot').getAttribute('data-features-state'))await expect(page.locator('#graph-plot')).toHaveAttribute('data-features-state',/ready|limited/);
  const diagnostic=await page.evaluate(async()=>{
   const times=[],frames=[],color=document.querySelector('#graph-color'),svg=document.querySelector('#graph-plot'),query=svg.querySelectorAll.bind(svg);let drawStart=0;
   // Both versions begin a redraw by selecting the previous drawing. Time the
   // synchronous draw separately from waiting for the next animation frame.
   svg.querySelectorAll=function(selector){if(selector==='.graph-drawing')drawStart=performance.now();return query(selector);};
   for(let i=0;i<25;i++){
    const elapsed=await new Promise(resolve=>requestAnimationFrame(()=>{
     const start=performance.now(),observer=new MutationObserver(()=>{observer.disconnect();resolve({draw:performance.now()-drawStart,frame:performance.now()-start});});
     observer.observe(svg,{childList:true});color.dispatchEvent(new Event('input',{bubbles:true}));
    }));if(i>=5){times.push(elapsed.draw);frames.push(elapsed.frame);}
   }
   times.sort((a,b)=>a-b);frames.sort((a,b)=>a-b);svg.querySelectorAll=query;return {redraws:times.length,median_ms:times[10],p95_ms:times[18],max_ms:times[19],frame_median_ms:frames[10],markers:query('.graph-feature').length};
  });
  expect(diagnostic.markers).toBeLessThanOrEqual(128);expect(diagnostic.max_ms).toBeLessThan(2000);
  await test.info().attach('feature-redraw.json',{body:JSON.stringify({expressions,...diagnostic}),contentType:'application/json'});console.log('Feature redraw:',JSON.stringify({expressions,...diagnostic}));
 });
}
