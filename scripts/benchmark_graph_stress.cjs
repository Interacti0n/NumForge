// Sustained browser diagnostics. Instrumentation exists only in routed test JS.
const {chromium}=require('../tests/browser/node_modules/@playwright/test');
const {spawn,execFile}=require('node:child_process'),{once}=require('node:events'),{promisify}=require('node:util');
const fs=require('node:fs/promises'),path=require('node:path'),runFile=promisify(execFile);
const args=process.argv.slice(2),option=n=>args.includes(n)?args[args.indexOf(n)+1]:undefined;
const executable=option('--server'),cycles=Number(option('--cycles')||16),output=option('--output')||'build/graph-stress.json',profileFilter=option('--profile'),caseFilter=option('--case');
if(!executable||!Number.isInteger(cycles)||cycles<1||cycles>100)throw Error('Use --server file.exe [--cycles 1..100] [--profile desktop|portrait|landscape] [--case mixed|poles|steps] [--output file.json]');
const profiles=[{name:'desktop',viewport:{width:1280,height:900}}, {name:'portrait',viewport:{width:390,height:844},hasTouch:true},{name:'landscape',viewport:{width:844,height:390},hasTouch:true}].filter(p=>!profileFilter||p.name===profileFilter);
const cases=[{name:'mixed',functions:['sin(33x)','sin(1000x)','cos(17x)','x/5']},{name:'poles',functions:['tan(3x)','1/(x-0.1)^2','sin(1000x)','sqrt(abs(x))*sin(33x)']},{name:'steps',functions:['floor(x)','ceil(x)','round(x;1)','trunc(x)']}].filter(c=>!caseFilter||c.name===caseFilter);
if(!profiles.length||!cases.length)throw Error('Unknown profile/case');
const pause=ms=>new Promise(r=>setTimeout(r,ms));
async function server(){
 const occupied=await fetch('http://127.0.0.1:19501/graph',{signal:AbortSignal.timeout(500)}).catch(()=>null);
 if(occupied)throw Error('Port 19501 is already in use; leave the existing server untouched.');
 const child=spawn(path.resolve(executable),['--no-browser','--port','19501'],{windowsHide:true,stdio:'ignore'});let failure;child.on('error',e=>failure=e);
 const close=async()=>{if(child.exitCode===null&&!failure){const exit=once(child,'exit');child.kill();await exit;}};
 try{for(let i=0;i<80;i++){if(failure)throw failure;if(child.exitCode!==null)throw Error('Server exited');try{if((await fetch('http://127.0.0.1:19501/graph')).ok)return{child,close};}catch(_){}await pause(100);}throw Error('Server readiness timeout');}catch(e){await close();throw e;}
}
function instrument(source){
 const replace=(a,b)=>{if(source.split(a).length!==2)throw Error('Stress instrumentation anchor changed: '+a);source=source.replace(a,b);};
 replace('state:()=>({visible,samples,',`diagnostics:()=>({id,visible,ranges:cache.length,uniqueRanges:new Set(cache).size,records:cache.reduce((n,e)=>n+e.points.length+e.bands.length,0),bytes:cache.reduce((n,e)=>n+e.cacheBytes,0),attempts:prefetchAttempts.size,retainedPoints:new Set([...cache.flatMap(e=>e.points),...(samples||[])]).size,retainedBands:new Set([...cache.flatMap(e=>e.bands),...(chunks||[]).flatMap(c=>c.bands||[]),...(bands||[])]).size,prefetch:!!prefetchRequest||prefetchTimer!==null,busy,pendingView}),
            state:()=>({visible,samples,`);
 replace('loadLibrary();draw();',`loadLibrary();draw();
    window.__graphStressStats=()=>({curves:curves.map(c=>c.diagnostics()),queue:queue.length,transmitting,foregroundBatches,viewportTimer:viewportTimer!==null});`);
 replace('return ()=>{save();disposed=true;', 'return ()=>{delete window.__graphStressStats;save();disposed=true;');return source;
}
async function memory(cdp,page,pid){
 // Inspector console entries can retain DOM nodes independently of the app.
 // Clear those roots before checking the app's own lifetime and forced-GC heap.
 await cdp.send('Runtime.discardConsoleEntries');await cdp.send('Log.clear');await cdp.send('HeapProfiler.collectGarbage');await page.evaluate(()=>new Promise(r=>requestAnimationFrame(r)));await cdp.send('HeapProfiler.collectGarbage');
 const heap=await cdp.send('Runtime.getHeapUsage'),dom=await cdp.send('Memory.getDOMCounters');
 return{heap_used:heap.usedSize,heap_total:heap.totalSize,dom_nodes:dom.nodes,listeners:dom.jsEventListeners,...(pid?{server_working_set:Number((await runFile('powershell.exe',['-NoProfile','-Command',`(Get-Process -Id ${pid}).WorkingSet64`],{windowsHide:true})).stdout.trim())}:{})};
}
async function run(browser,owned,profile,scenario){
 const context=await browser.newContext({viewport:profile.viewport,hasTouch:!!profile.hasTouch,reducedMotion:'reduce'}),page=await context.newPage(),cdp=await context.newCDPSession(page);
 let active=0,lastNetwork=performance.now(),requests=0,aborted=0,bytes=0,phase='setup';const outstanding=new Set(),observations=[],errors=[],responseJobs=new Set();
 page.on('pageerror',e=>errors.push(String(e)));
 page.on('request',r=>{if(r.url().includes('/api/graph/plot?')){outstanding.add(r);++active;++requests;lastNetwork=performance.now();}});
 const done=r=>{if(outstanding.delete(r)){--active;lastNetwork=performance.now();}};page.on('requestfinished',done);page.on('requestfailed',r=>{if(outstanding.has(r))++aborted;done(r);});
 page.on('response',r=>{if(!r.url().includes('/api/graph/plot?'))return;const job=r.body().then(b=>bytes+=b.length).catch(()=>{});responseJobs.add(job);job.finally(()=>responseJobs.delete(job));});
 await page.route('**/assets/graph.js',async route=>{const response=await route.fetch();await route.fulfill({response,body:instrument(await response.text())});});
 const frame=()=>page.evaluate(()=>new Promise(requestAnimationFrame));
 const navigate=async href=>{const menu=page.locator('.nav-toggle');if(await menu.isVisible())await menu.click();await page.locator(`.primary-nav a[href="${href}"]`).click();};
 const idle=async()=>{const deadline=performance.now()+35000;while(performance.now()<deadline){const s=await page.evaluate(()=>window.__graphStressStats?.());if(!active&&performance.now()-lastNetwork>350&&s&&!s.queue&&!s.transmitting&&!s.foregroundBatches&&!s.viewportTimer&&s.curves.every(c=>!c.busy&&!c.prefetch))return;await pause(35);}throw Error('Work did not settle: '+phase);};
 const camera=()=>page.locator('#graph-plot').evaluate(svg=>({low:Number(svg.dataset.xmin),high:Number(svg.dataset.xmax),bottom:Number(svg.dataset.ymin),top:Number(svg.dataset.ymax),width:svg.viewBox.baseVal.width,height:svg.viewBox.baseVal.height}));
 const check=async()=>{const s=await page.evaluate(()=>window.__graphStressStats());if(s.curves.length!==4||s.curves.some(c=>c.ranges>12||c.uniqueRanges!==c.ranges||c.records>12288||c.bytes>8*1024*1024||c.attempts>48||c.retainedPoints+c.retainedBands>20480))throw Error('Cache bound or identity invariant failed: '+JSON.stringify(s));if(s.curves.some(c=>c.pendingView))throw Error('Settled view still pending: '+JSON.stringify(s));const messages=await page.locator('.graph-curve-status.error:not([hidden])').allTextContents();if(messages.length||errors.length)throw Error(JSON.stringify({messages,errors}));if(await page.locator('#graph-plot').evaluate(svg=>/NaN|Infinity/.test(svg.innerHTML)))throw Error('Invalid SVG projection');return s;};
 const snapshot=async(cycle,withProcess=false)=>{
  await idle();
  const telemetry=await page.evaluate(()=>{const b=window.__stressFrames;b.paused=true;return{frames:b.frames.splice(0),longTasks:b.longTasks.splice(0),draws:b.draws};});
  const stats=await check(),mem=await memory(cdp,page,withProcess?owned.child.pid:null);
  observations.push({cycle,phase,requests,aborted,bytes,camera:await camera(),stats,...mem,telemetry});
  await frame();await frame();await page.evaluate(()=>{const b=window.__stressFrames;b.measuringSince=performance.now();b.paused=false;});
 };
 let result;
 try{
  await page.goto('http://127.0.0.1:19501/graph?lang=en');for(let i=0;i<4;i++){if(i)await page.locator('#graph-add').click();await page.locator('.graph-curve').nth(i).locator('input:not([type=color])').fill(scenario.functions[i]);}
  await page.evaluate(()=>{window.__stressFrames={frames:[],longTasks:[],draws:0,paused:false,measuringSince:performance.now()};let last;const loop=t=>{const b=window.__stressFrames;if(last!==undefined&&!b.paused&&b.frames.length<4096)b.frames.push(t-last);last=t;requestAnimationFrame(loop);};requestAnimationFrame(loop);try{new PerformanceObserver(l=>{const b=window.__stressFrames;if(b.paused)return;b.longTasks.push(...l.getEntries().filter(e=>e.startTime>=b.measuringSince).map(e=>e.duration));if(b.longTasks.length>512)b.longTasks.splice(0,b.longTasks.length-512);}).observe({type:'longtask'});}catch(_){}window.__stressObserver=new MutationObserver(()=>++window.__stressFrames.draws);window.__stressObserve=()=>{window.__stressObserver.disconnect();const svg=document.querySelector('#graph-plot');if(svg)window.__stressObserver.observe(svg,{childList:true});};window.__stressObserve();window.__stressOldRoots=[];});
  await page.locator('#graph-submit').click();await snapshot(0,true);
  for(let cycle=1;cycle<=cycles;cycle++){
   phase='reset';await page.locator('#graph-plot').press('Home');await idle();
   phase='pan';await page.locator('#graph-plot').evaluate((svg,count)=>{for(let i=0;i<count;i++)svg.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowRight',bubbles:true,cancelable:true}));},8+3*(Math.floor((cycle-1)/4)%4));await idle();
   phase='zoom-roundtrip';await page.locator('#graph-plot').scrollIntoViewIfNeeded();const box=await page.locator('#graph-plot').boundingBox(),anchor={clientX:box.x+box.width*.5,clientY:box.y+box.height*.5};for(const deltaY of [-1,-1,-1,1,1,1]){await page.locator('#graph-plot').dispatchEvent('wheel',{deltaY,...anchor});await frame();}await idle();
   phase='toggle';const eye=page.locator('.graph-curve').nth(2).locator('.graph-visibility'),beforeToggle=requests;await eye.click();await eye.click();await idle();if(requests!==beforeToggle)throw Error('Warm visibility toggle recalculated');
   if(cycle%4===0){phase='expand';await page.locator('#graph-expand').click();await idle();await page.locator('#graph-expand').click();await idle();}
   if(profile.hasTouch&&cycle%4===0){
    phase='pinch';await page.locator('#graph-plot').scrollIntoViewIfNeeded();await frame();
    const before=await camera(),b=await page.locator('#graph-plot').boundingBox(),y=b.y+b.height*.5,x=b.x+b.width*.55;
    await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x:x-25,y,id:1},{x:x+25,y,id:2}]});await frame();
    await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x:x-40,y,id:1},{x:x+40,y,id:2}]});await frame();
    await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});await idle();
    const after=await camera();if(Math.abs((after.high-after.low)/(before.high-before.low)-.625)>1e-8||Math.abs((after.top-after.bottom)/(before.top-before.bottom)-.625)>1e-8)throw Error('Touch pinch did not zoom both axes');
   }
   if(cycle%4===0){
    phase='cancel';let release,started;const held=new Promise(r=>release=r),waiting=new Promise(r=>started=r);
    const handler=async route=>{if(route.request().postData()!=='x+0.125'){await route.fallback();return;}const response=await route.fetch();started();await held;try{await route.fulfill({response});}catch(_) {}};
    await page.route('**/api/graph/plot?**',handler);
    try{await page.locator('#graph-expression').fill('x+0.125');await page.locator('#graph-submit').click();await Promise.race([waiting,pause(10000).then(()=>{throw Error('Held request did not start');})]);await page.locator('#graph-expression').fill(scenario.functions[0]);await page.locator('#graph-submit').click();}finally{release();await page.unroute('**/api/graph/plot?**',handler);}
    await idle();
   }
   phase='cycle';await snapshot(cycle,cycle===cycles);process.stdout.write(JSON.stringify({profile:profile.name,case:scenario.name,cycle,requests,heap_MiB:+(observations.at(-1).heap_used/1024/1024).toFixed(2),records:observations.at(-1).stats.curves.map(c=>c.records)})+'\n');
  }
  const lifecycle=[];
  for(let i=0;i<6;i++){phase='navigate-away';await page.evaluate(()=>{window.__stressObserver.disconnect();window.__stressOldRoots.push(new WeakRef(document.querySelector('.graph-workspace')));});await navigate('/units?lang=en');await page.locator('#unit-converter').waitFor();await pause(400);const mem=await memory(cdp,page);const alive=await page.evaluate(()=>window.__stressOldRoots.filter(r=>r.deref()).length);lifecycle.push({i,alive,...mem});if(alive){const chunks=[],listener=e=>chunks.push(e.chunk);cdp.on('HeapProfiler.addHeapSnapshotChunk',listener);await cdp.send('HeapProfiler.takeHeapSnapshot');cdp.off('HeapProfiler.addHeapSnapshotChunk',listener);await fs.mkdir(path.dirname(output),{recursive:true});await fs.writeFile(output+'.heapsnapshot',chunks.join(''));throw Error('Disposed graph DOM retained after GC: '+alive);}await navigate('/graph?lang=en');await page.locator('#graph-form').waitFor();await page.evaluate(()=>window.__stressObserve());await page.locator('#graph-submit').click();await idle();await check();}
  result={profile:profile.name,case:scenario.name,cycles,observations,lifecycle,error:null};
 }catch(error){result={profile:profile.name,case:scenario.name,cycles_completed:observations.length-1,phase,observations,error:String(error),pageErrors:errors,stats:await page.evaluate(()=>window.__graphStressStats?.()).catch(()=>null),camera:await camera().catch(()=>null)};}
 finally{await Promise.allSettled([...responseJobs]);await context.close();}
 return result;
}
(async()=>{
 const rows=[],browser=await chromium.launch();let owned;
 try{owned=await server();outer:for(const profile of profiles)for(const scenario of cases){
  const row=await run(browser,owned,profile,scenario);rows.push(row);
  if(row.error){process.stdout.write(JSON.stringify({profile:row.profile,case:row.case,error:row.error,phase:row.phase})+'\n');break outer;}
  process.stdout.write(JSON.stringify({profile:row.profile,case:row.case,cycles:row.cycles,navigation_roundtrips:row.lifecycle.length,complete:true})+'\n');
 }}finally{await browser.close();await owned?.close();}
 await fs.mkdir(path.dirname(output),{recursive:true});
 await fs.writeFile(output,JSON.stringify({generated:new Date().toISOString(),server:path.resolve(executable),cycles,note:'Headless Chromium on this PC; mobile layouts are emulated, not physical phones. Private cache instrumentation is injected only into test responses. Heap follows forced GC after clearing inspector console roots. Timing excludes cache-stat sampling and forced GC. Process working set is an OS measurement, not a leak proof. No fixed timing or heap-growth thresholds.',rows},null,2)+'\n');
 if(rows.some(r=>r.error))process.exitCode=1;
})().catch(error=>{process.stderr.write(error.stack+'\n');process.exitCode=1;});
