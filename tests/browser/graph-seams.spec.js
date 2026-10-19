const { artifactPath } = require('./artifacts');
const {test,expect}=require('@playwright/test');
async function returnFromDetail(page){
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('x*x');
 const prepared=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&new URL(r.url()).searchParams.get('min')==='-10');
 await page.locator('#graph-submit').click();await prepared;
 const detailed=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&new URL(r.url()).searchParams.get('min')==='-2.5'&&new URL(r.url()).searchParams.get('quality')==='refined');
 await page.evaluate(()=>{document.querySelector('#graph-plot').dispatchEvent(new KeyboardEvent('keydown',{key:'+',bubbles:true,cancelable:true}));document.querySelector('#graph-plot').dispatchEvent(new KeyboardEvent('keydown',{key:'+',bubbles:true,cancelable:true}));});await detailed;
 await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');
 await page.evaluate(()=>{document.querySelector('#graph-plot').dispatchEvent(new KeyboardEvent('keydown',{key:'-',bubbles:true,cancelable:true}));document.querySelector('#graph-plot').dispatchEvent(new KeyboardEvent('keydown',{key:'-',bubbles:true,cancelable:true}));});
 await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');
}
for(const width of [390,1280])test(`zoomed-out x*x uses one continuous stroke across prepared tiles at width ${width}`,async({page})=>{
 await page.setViewportSize({width,height:900});await returnFromDetail(page);
 await expect(page.locator('path.graph-line')).toHaveCount(1);
 expect((await page.locator('path.graph-line').getAttribute('d')).match(/M/g)).toHaveLength(1);
 await page.locator('.graph-result').screenshot({path:artifactPath(`graph-seams-${width}.png`)});
});
for(const gap of [false,true])test(`tile joins share actual boundary values and preserve rejected edges: ${gap}`,async({page})=>{
 await page.route('**/api/graph/plot?**',route=>{
  const q=new URL(route.request().url()).searchParams,low=Number(q.get('min')),high=Number(q.get('max'));
  const detailed=low===-2.5&&high===2.5;
  const xs=[...new Set([low,high,...[-10,-4,-2,-1,0,1,2,4,10].filter(x=>x>low&&x<high)])].sort((a,b)=>a-b);
  const points=xs.map((x,i)=>[x,x*x,i&&!(gap&&detailed&&i===1)?1:0,0,1]);
  return route.fulfill({json:{ok:true,schema_version:3,evaluation:'double',parameter:'x',quality:q.get('quality'),width:Number(q.get('width')),height:440,count:points.length,limited:gap&&detailed,points,
   ...(detailed?{slopes:xs.map(x=>2*x)}:{})}});
 });
 // Even the rejected-edge case is prepared before it turns limited.
 await returnFromDetail(page);
 const paths=await page.locator('path.graph-line').evaluateAll(nodes=>nodes.map(n=>n.getAttribute('d')));
 if(gap){
  expect(paths.length).toBeGreaterThan(1);
  const crosses=await page.locator('#graph-plot').evaluate(svg=>{
   const w=svg.viewBox.baseVal.width,a=64+7.5/20*(w-86),b=64+8/20*(w-86);
   for(const path of svg.querySelectorAll('.graph-line')){
    const clip=svg.querySelector(path.getAttribute('clip-path').slice(4,-1)+' rect'),left=Number(clip.getAttribute('x')),right=left+Number(clip.getAttribute('width'));let previous;
    for(const command of path.getAttribute('d').matchAll(/([MLC])([^MLC]+)/g)){
     const x=command[2].split(/[ ,]+/).map(Number).at(-2);
     if(command[1]!=='M'&&Math.max(previous,left,a)<Math.min(x,right,b)-1e-8)return true;
     previous=x;
    }
   }return false;
  });expect(crosses).toBe(false);
 }
 else{
  expect(paths).toHaveLength(1);expect(paths[0].match(/M/g)).toHaveLength(1);
  // The old straight tile interpolated y=7 at x=-2.5, while the new C
  // boundary sample is 6.25. The accepted old edge must end at that sample.
  const boundary=await page.locator('#graph-plot').evaluate(svg=>{
   const w=svg.viewBox.baseVal.width,h=svg.viewBox.baseVal.height,p=svg.querySelector('.graph-line'),target=64+7.5/20*(w-86);
   let a=0,b=p.getTotalLength();for(let i=0;i<35;++i){const m=(a+b)/2;if(p.getPointAtLength(m).x<target)a=m;else b=m;}
   return Math.abs(p.getPointAtLength((a+b)/2).y-(h-46-16.25/20*(h-68)));
  });expect(boundary).toBeLessThan(.01);
 }
});
