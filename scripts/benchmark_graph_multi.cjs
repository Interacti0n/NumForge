// Reproducible real-browser diagnostics; times are measurements, not CI thresholds.
// npm dependencies: tests/browser/node_modules. Each spawned server is privately owned.
const {chromium}=require('../tests/browser/node_modules/@playwright/test');
const {spawn}=require('node:child_process');
const {once}=require('node:events');
const fs=require('node:fs/promises'),path=require('node:path');
const args=process.argv.slice(2),option=name=>args.includes(name)?args[args.indexOf(name)+1]:undefined;
const before=option('--before'),after=option('--after'),output=args.includes('--output')?option('--output'):'build/graph-multi-benchmark.json';
const repeats=Number(args.includes('--repeats')?option('--repeats'):3);
if(!before||!after||!Number.isInteger(repeats)||repeats<1||repeats>10)throw Error('Use --before old.exe --after new.exe [--suite default|precision] [--output file.json] [--repeats 1..10]');
const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms)),now=()=>performance.now();
const percentile=(values,p)=>values.length?[...values].sort((a,b)=>a-b)[Math.min(values.length-1,Math.floor(values.length*p))]:null;
const suite=option('--suite')||'default';if(!['default','precision'].includes(suite))throw Error('Unknown suite');
const scenarios=suite==='precision'?[{name:'precision-camera',functions:['sinh(10x)','cosh(10x)','tanh(10x)','atan(100x)']}]:[
 {name:'mixed',functions:['sin(33x)','cos(17x)','exp(-x*x/4)','x^3-3x']},
 {name:'dense-and-poles',functions:['sin(1000x)','tan(3x)','1/(x-0.1)^2','sqrt(abs(x))*sin(33x)']}
];
async function server(executable,port){
 const occupied=await fetch(`http://127.0.0.1:${port}`,{signal:AbortSignal.timeout(500)}).catch(()=>null);if(occupied){await occupied.arrayBuffer();throw Error('Benchmark port occupied');}
 const child=spawn(path.resolve(executable),['--no-browser','--port',String(port)],{windowsHide:true,stdio:'ignore'});
 let launchError;child.on('error',error=>launchError=error);
 const close=async()=>{if(child.exitCode===null&&!launchError){const exit=once(child,'exit');child.kill();await exit;}};
 try{for(let i=0;i<60;i++){if(launchError)throw launchError;if(child.exitCode!==null)throw Error('Server exited');try{const r=await fetch(`http://127.0.0.1:${port}/graph?lang=en`);await r.arrayBuffer();if(r.ok)return{url:`http://127.0.0.1:${port}`,close};}catch(_){}await pause(100);}throw Error('Server readiness timeout');}catch(error){await close();throw error;}
}
async function run(browser,base,version,profile,scenario,repeat){
 const context=await browser.newContext({viewport:profile.viewport}),page=await context.newPage(),cdp=await context.newCDPSession(page);
 const requests=new Map();let lastActivity=now(),phase='initial';const phases={};let active=0,maxActive=0;
 page.on('request',request=>{if(!request.url().includes('/api/graph/plot?'))return;requests.set(request,{phase,expression:request.postData(),query:Object.fromEntries(new URL(request.url()).searchParams),start:now(),count:0,bytes:0});++active;maxActive=Math.max(maxActive,active);lastActivity=now();});
 const done=request=>{if(!requests.has(request))return;const record=requests.get(request);if(record.end)return;record.end=now();record.duration_ms=record.end-record.start;--active;lastActivity=now();};
 page.on('requestfinished',done);page.on('requestfailed',request=>{const record=requests.get(request);if(record)record.aborted=true;done(request);});
 const responseJobs=[];page.on('response',response=>{const record=requests.get(response.request());if(!record)return;responseJobs.push((async()=>{try{const body=await response.body();record.bytes=body.length;const data=JSON.parse(body);record.count=data.count||0;record.bands=data.bands?.length||0;record.limited=data.limited;record.cache_weight_bytes=body.length*2+record.count*256+record.bands*128;}catch(_) {}})());});
 try{
  await page.goto(base+'/graph?lang=en');const rows=page.locator('.graph-curve');await rows.first().locator('input:not([type=color])').fill(scenario.functions[0]);
  for(const expression of scenario.functions.slice(1)){await page.locator('#graph-add').click();await rows.last().locator('input:not([type=color])').fill(expression);}
  await page.locator('#graph-plot').scrollIntoViewIfNeeded();
  await page.evaluate(()=>{
   window.bench={frames:[],longTasks:[],draws:[],renderFrames:[],pendingInput:null,last:null,origin:performance.now()};
   const frame=time=>{const b=window.bench;if(b.last!==null)b.frames.push(time-b.last);b.last=time;requestAnimationFrame(frame);};requestAnimationFrame(frame);
   try{new PerformanceObserver(list=>window.bench.longTasks.push(...list.getEntries().map(e=>e.duration))).observe({type:'longtask',buffered:false});}catch(_){}
   for(const type of ['wheel','pointermove','click'])document.addEventListener(type,event=>{if(type==='pointermove'&&!event.buttons)return;if(type==='click'&&!event.target.closest('#graph-submit,#graph-expand,.graph-visibility,.graph-legend-toggle'))return;if(type!=='click'&&event.target.closest('#graph-plot')===null)return;window.bench.pendingInput=performance.now();},{capture:true});
   new MutationObserver(()=>{if(document.querySelector('#graph-plot .graph-drawing')){const b=window.bench;b.draws.push(performance.now());if(b.pendingInput!==null){const input=b.pendingInput;b.pendingInput=null;requestAnimationFrame(()=>b.renderFrames.push(performance.now()-input));}}}).observe(document.querySelector('#graph-plot'),{childList:true});
  });
  const idle=async()=>{
   const deadline=now()+35000;
   while(now()<deadline){if(active===0&&now()-lastActivity>350&&!await page.locator('.graph-workspace').getAttribute('aria-busy'))return;await pause(40);}throw Error('Graph work did not settle');
  };
  const begin=async name=>{phase=name;lastActivity=now();maxActive=active;await page.evaluate(()=>{const b=window.bench;b.frames=[];b.longTasks=[];b.draws=[];b.renderFrames=[];b.pendingInput=null;b.last=null;b.origin=performance.now();});};
  const finish=async(first=null)=>{
   await idle();await Promise.all(responseJobs);const telemetry=await page.evaluate(()=>{const b=window.bench;return{camera:{...document.querySelector('#graph-plot').dataset,width:document.querySelector('#graph-plot').viewBox.baseVal.width,height:document.querySelector('#graph-plot').viewBox.baseVal.height},elapsed:performance.now()-b.origin,frames:b.frames,longTasks:b.longTasks,draws:b.draws.length,renderFrames:b.renderFrames,unresolved:[...document.querySelectorAll('.graph-curve-status:not([hidden]):not(.error)')].filter(p=>p.textContent.includes('Some visible intervals could not be resolved')).length,dense:new Set([...document.querySelectorAll('path.graph-band')].map(p=>p.dataset.curve)).size,rendered:new Set([...document.querySelectorAll('path.graph-line,path.graph-band')].map(p=>p.dataset.curve)).size,errors:[...document.querySelectorAll('.graph-curve-status.error:not([hidden])')].map(p=>p.textContent)};});
   const work=[...requests.values()].filter(r=>r.phase===phase);
   const initialWork=phase==='initial'?work.slice(0,4):[];
   phases[phase]={camera:telemetry.camera,ranges:work.map(r=>({expression:r.expression,query:r.query,bytes:r.bytes,points:r.count,bands:r.bands||0,aborted:!!r.aborted})),initial_foreground_bytes:initialWork.reduce((s,r)=>s+r.bytes,0),initial_foreground_points:initialWork.reduce((s,r)=>s+r.count,0),initial_background_requests:phase==='initial'?Math.max(0,work.length-4):null,settled_including_idle_ms:+telemetry.elapsed.toFixed(2),first_visible_frame_ms:first===null?null:+first.toFixed(2),frame_p95_ms:percentile(telemetry.frames,.95),max_frame_ms:Math.max(0,...telemetry.frames),long_tasks:telemetry.longTasks.length,max_long_task_ms:Math.max(0,...telemetry.longTasks),draws:telemetry.draws,input_to_frame_p95_ms:percentile(telemetry.renderFrames,.95),max_input_to_frame_ms:Math.max(0,...telemetry.renderFrames),visible_unresolved_functions:telemetry.unresolved,visible_dense_functions:telemetry.dense,rendered_functions:telemetry.rendered,errors:telemetry.errors,requests:work.length,aborted_requests:work.filter(r=>r.aborted).length,response_bytes:work.reduce((s,r)=>s+r.bytes,0),received_points:work.reduce((s,r)=>s+r.count,0),received_bands:work.reduce((s,r)=>s+(r.bands||0),0),estimated_response_weight_bytes:work.reduce((s,r)=>s+(r.cache_weight_bytes||0),0),max_active_requests:maxActive,http_p50_ms:percentile(work.filter(r=>!r.aborted).map(r=>r.duration_ms),.5)};
  };
  await begin('initial');await page.locator('#graph-submit').click();await page.waitForFunction(()=>!document.querySelector('.graph-workspace').hasAttribute('aria-busy'));
  const first=await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>resolve(performance.now()-window.bench.origin))));await finish(first);
  await begin('expanded');await page.locator('#graph-expand').click();await page.waitForFunction(()=>document.querySelector('#graph-expanded').open);await finish();
  await page.locator('#graph-plot').scrollIntoViewIfNeeded();let box=await page.locator('#graph-plot').boundingBox(),x=box.x+box.width*.5,y=box.y+box.height*.5;
  await begin('pan');await page.mouse.move(x,y);await page.mouse.down();for(let i=1;i<=8;i++){await page.mouse.move(x+box.width*.012*i,y);await page.evaluate(()=>new Promise(requestAnimationFrame));}await page.mouse.up();await finish();
  await begin('zoom');for(let i=0;i<6;i++){await page.locator('#graph-plot').dispatchEvent('wheel',{deltaY:-100,clientX:x,clientY:y});await page.evaluate(()=>new Promise(requestAnimationFrame));}await finish();
  if(version==='after'){
   await begin('toggle');const id=await rows.nth(suite==='precision'?3:1).getAttribute('data-curve');await page.locator(`#graph-legend button[data-curve="${id}"]`).click();await page.waitForFunction(id=>!document.querySelector(`path.graph-line[data-curve="${id}"]`),id);await page.locator(`#graph-legend button[data-curve="${id}"]`).click();await page.waitForFunction(id=>!!document.querySelector(`path.graph-line[data-curve="${id}"]`),id);const visible=await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>resolve(performance.now()-window.bench.origin))));await finish(visible);
   if(phases.toggle.requests!==0)throw Error('Warm visibility toggle made a graph request');
  }
  const heap=await cdp.send('Runtime.getHeapUsage');const row={version,profile:profile.name,scenario:scenario.name,repeat,functions:scenario.functions,js_heap_used_bytes:heap.usedSize,js_heap_total_bytes:heap.totalSize,phases};process.stdout.write(JSON.stringify({version,profile:profile.name,scenario:scenario.name,repeat,first_frame_ms:phases.initial.first_visible_frame_ms,initial_curves:phases.initial.rendered_functions,initial_unresolved:phases.initial.visible_unresolved_functions,zoom_requests:phases.zoom.requests,toggle_requests:phases.toggle?.requests,errors:phases.initial.errors})+'\n');return row;
 }finally{await context.close();}
}
(async()=>{
 const rows=[],browser=await chromium.launch();
 try{for(const [version,executable,port] of [['before',before,19491],['after',after,19492]]){
  const owned=await server(executable,port);
  try{for(const profile of [{name:'desktop',viewport:{width:1280,height:900}},{name:'mobile-layout',viewport:{width:390,height:844}}])for(const scenario of scenarios)for(let repeat=1;repeat<=repeats;repeat++)rows.push(await run(browser,owned.url,version,profile,scenario,repeat));}finally{await owned.close();}
 }}finally{await browser.close();}
 await fs.mkdir(path.dirname(output),{recursive:true});await fs.writeFile(output,JSON.stringify({generated:new Date().toISOString(),before:path.resolve(before),after:path.resolve(after),repeats,suite,note:'Headless Chromium on this PC; mobile viewport is not a physical phone. Settling includes a fixed 350ms quiet interval. Frame times include tool interaction overhead. Heap is sampled without forcing GC and includes the whole page. Cache weight is a conservative response estimate, not heap usage.',rows},null,2)+'\n');
})().catch(error=>{process.stderr.write(error.stack+'\n');process.exitCode=1;});
