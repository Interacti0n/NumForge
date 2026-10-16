const {test,expect}=require('@playwright/test');
for(const expressions of [['x!'],['ncr(x;3)'],['sin(33*x)','cos(31*x)','sin(17*x)','cos(13*x)'],['1/ln(ln(sqrt(x)))^2']]){
 test(`initial plot and C confirmation: ${expressions.join(', ')}`,async({page})=>{
  const timings=new Map(),bodies=[],pending=[];
  page.on('request',request=>{if(request.url().includes('/api/graph/'))timings.set(request,{kind:request.url().includes('/verify')?'verify':'plot',start:performance.now()});});
  page.on('requestfinished',request=>{const record=timings.get(request);if(record)record.ms=performance.now()-record.start;});
  page.on('response',response=>{if(response.url().includes('/api/graph/'))pending.push(response.json().then(body=>bodies.push({kind:response.url().includes('/verify')?'verify':'plot',body})).catch(()=>{}));});
  await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill(expressions[0]);
  for(const expression of expressions.slice(1)){await page.locator('#graph-add').click();await page.locator('.graph-curve').last().locator('input:not([type=color])').fill(expression);}
  const start=performance.now();await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('#graph-plot')).toHaveAttribute('data-features-state',/ready|limited/);const elapsed=performance.now()-start;await Promise.allSettled(pending);
  const records=[...timings.values()],diagnostic={expressions,time_to_confirmation_ms:elapsed,plot_requests:records.filter(r=>r.kind==='plot').length,verification_requests:records.filter(r=>r.kind==='verify').length,max_finished_http_ms:Math.max(0,...records.map(r=>r.ms||0)),markers:await page.locator('.graph-feature').count(),discrete_dots:await page.locator('.graph-discrete-point').count(),feature_state:await page.locator('#graph-plot').getAttribute('data-features-state'),graph_points_transferred:bodies.filter(r=>r.kind==='plot').reduce((sum,r)=>sum+(r.body.count||0),0)};
  expect(bodies.every(r=>r.body.ok)).toBe(true);expect(diagnostic.markers).toBeLessThanOrEqual(128);
  await test.info().attach('graph-analysis-performance.json',{body:JSON.stringify(diagnostic),contentType:'application/json'});console.log('Graph analysis:',JSON.stringify(diagnostic));
 });
}
