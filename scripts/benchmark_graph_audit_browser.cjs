// Real SVG checks complement the independent HTTP corpus; no production hooks.
const {chromium}=require('../tests/browser/node_modules/@playwright/test');
const {spawn}=require('node:child_process'),{once}=require('node:events');
const fs=require('node:fs/promises'),path=require('node:path');
const args=process.argv.slice(2),option=n=>args.includes(n)?args[args.indexOf(n)+1]:undefined;
const executable=option('--server'),channel=option('--channel')||'chromium',output=option('--output')||'build/graph-svg-audit.json';
if(!executable||!['chromium','chrome','msedge'].includes(channel))throw Error('Use --server executable [--channel chromium|chrome|msedge] [--output file.json]');
const pause=ms=>new Promise(r=>setTimeout(r,ms));
const cases=[
 {name:'lorentz-narrow',expression:'1/(1+((x-0.1234)/0.04)^2)',x:.1234,expected:1},
 {name:'hyperbolic-steep',expression:'sinh(10x)',x:-.1953125,expected:Math.sinh(-1.953125)},
 {name:'cosh-steep',expression:'cosh(10x)',x:-.1904296875,expected:Math.cosh(-1.904296875)},
 {name:'gaussian-narrow',expression:'exp(-((x-0.1234)/0.04)^2)',x:.1318359375,expected:Math.exp(-(((.1318359375-.1234)/.04)**2))},
 {name:'gaussian-very-narrow',expression:'exp(-((x-0.1234)/0.005)^2)',x:.1234,expected:1},
 {name:'floor-wide',expression:'floor(x)',x:-.048828125,expected:-1,zoomOut:3}
];
async function measure(page,item){
 return page.locator('#graph-plot').evaluate((svg,item)=>{
  const camera={xmin:Number(svg.dataset.xmin),xmax:Number(svg.dataset.xmax),ymin:Number(svg.dataset.ymin),ymax:Number(svg.dataset.ymax)},w=svg.viewBox.baseVal.width,h=svg.viewBox.baseVal.height;
  const paths=[...svg.querySelectorAll('path.graph-line')];let drawn=null,errorPx=null;
  if(paths.length&&item.x!==undefined){const x=64+(item.x-camera.xmin)/(camera.xmax-camera.xmin)*(w-86),line=paths.find(p=>{const b=p.getBBox();return b.x<=x&&b.x+b.width>=x;});if(!line)return{camera,width:w,height:h,status:document.querySelector('#graph-status').textContent,error:document.querySelector('#graph-status').classList.contains('error'),paths:paths.length,drawn,error_px:errorPx};let low=0,high=line.getTotalLength();
   for(let i=0;i<60;i++){const m=(low+high)/2;if(line.getPointAtLength(m).x<x)low=m;else high=m;}
   const p=line.getPointAtLength((low+high)/2);drawn=camera.ymax-(p.y-22)/(h-68)*(camera.ymax-camera.ymin);errorPx=Math.abs(drawn-item.expected)*(h-68)/(camera.ymax-camera.ymin);
  }
  return{camera,width:w,height:h,status:document.querySelector('#graph-status').textContent,error:document.querySelector('#graph-status').classList.contains('error'),paths:paths.length,drawn,error_px:errorPx};
 },item);
}
(async()=>{
 const base='http://127.0.0.1:19507';const occupied=await fetch(base,{signal:AbortSignal.timeout(500)}).catch(()=>null);if(occupied){await occupied.arrayBuffer();throw Error('Audit port occupied');}
 const owned=spawn(path.resolve(executable),['--no-browser','--port','19507'],{windowsHide:true,stdio:'ignore'});let launchError;owned.on('error',e=>launchError=e);let browser;
 try{
  for(let i=0;i<80;i++){if(launchError)throw launchError;if(owned.exitCode!==null)throw Error('Server exited');try{const response=await fetch(base);await response.arrayBuffer();if(response.ok)break;}catch(_){}if(i===79)throw Error('Readiness timeout');await pause(100);}
  browser=await chromium.launch(channel==='chromium'?{}:{channel});
  const context=await browser.newContext({viewport:{width:1280,height:900}}),page=await context.newPage(),rows=[],pageErrors=[],requests=[];
  let active=0,last=performance.now();page.on('pageerror',e=>pageErrors.push(e.message));
  page.on('request',r=>{if(r.url().includes('/api/graph/plot?')){active++;requests.push({expression:r.postData(),query:Object.fromEntries(new URL(r.url()).searchParams)});last=performance.now();}});
  for(const event of ['requestfinished','requestfailed'])page.on(event,r=>{if(r.url().includes('/api/graph/plot?')){active--;last=performance.now();}});
  const idle=async()=>{await pause(250);const deadline=performance.now()+20000;while(active||performance.now()-last<500||await page.locator('.graph-workspace').getAttribute('aria-busy')){if(performance.now()>deadline)throw Error('Graph did not settle');await pause(40);}await page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));};
  await page.goto(base+'/graph?lang=en');
  for(const item of cases){await page.locator('#graph-expression').fill(item.expression);const response=page.waitForResponse(r=>r.url().includes('/api/graph/plot?')&&r.request().postData()===item.expression);await page.locator('#graph-submit').click();await (await response).finished();await page.waitForFunction(()=>!document.querySelector('.graph-workspace').hasAttribute('aria-busy'));await page.evaluate(()=>new Promise(requestAnimationFrame));
   const initial=await measure(page,item);await idle();
   for(let i=0;i<(item.zoomOut||0);i++){await page.locator('#graph-plot').press('-');await idle();}
   const settled=await measure(page,item),row={channel,...item,initial,settled};rows.push(row);process.stdout.write(JSON.stringify(row)+'\n');
   if(channel==='chromium'){await page.locator('#graph-plot').screenshot({path:path.join(path.dirname(output),'graph-audit-'+item.name+'.png')});}
  }
  // Basic installed-browser smoke: multiple curves and reset,
  // expanded view/focus return and error recovery.
  await page.locator('#graph-expression').fill('sin(x)');await page.locator('#graph-add').click();await page.locator('.graph-curve').nth(1).locator('input:not([type=color])').fill('x*x');await page.locator('#graph-submit').click();await idle();
  if(new Set(await page.locator('path.graph-line').evaluateAll(ps=>ps.map(p=>p.dataset.curve))).size!==2)throw Error('Missing multi-curve strokes');
  await page.locator('#graph-expand').click();await idle();if(!await page.locator('#graph-expanded').evaluate(d=>d.open))throw Error('Expand failed');await page.keyboard.press('Escape');await idle();
  await page.locator('#graph-plot').press('ArrowUp');await page.locator('#graph-plot').press('ArrowRight');await idle();await page.locator('#graph-plot').press('Home');await idle();
  const camera=await measure(page,{});if(camera.camera.xmin!==-10||camera.camera.ymin!==-10)throw Error('Reset failed');
  await page.locator('#graph-expression').fill('sin(');await page.locator('#graph-submit').click();await idle();if(!await page.locator('.graph-curve-status.error').count())throw Error('Syntax error missing');
  await page.locator('#graph-expression').fill('sin(x)');await page.locator('#graph-submit').click();await idle();if(await page.locator('.graph-curve-status.error:not([hidden])').count())throw Error('Error recovery failed');
  if(pageErrors.length)throw Error(pageErrors.join('; '));
  await fs.mkdir(path.dirname(output),{recursive:true});await fs.writeFile(output,JSON.stringify({generated:new Date().toISOString(),browser:await browser.version(),channel,rows,requests,pageErrors,smoke:'Two functions, expand/Escape, horizontal/vertical keyboard pan, reset, syntax error and recovery passed. Desktop only; all selected channels use Chromium.',note:'Actual SVG getPointAtLength at independent feature coordinates, first completed foreground and settled background. Diagnostics intentionally record known failures without passing them off as assertions of correctness.'},null,2)+'\n');await context.close();
 }finally{await browser?.close();if(owned.exitCode===null&&!launchError){const exited=once(owned,'exit');owned.kill();await exited;}}
})().catch(e=>{process.stderr.write(e.stack+'\n');process.exitCode=1;});
