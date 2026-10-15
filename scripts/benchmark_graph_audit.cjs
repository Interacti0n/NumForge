// Broad graph audit: independent values, rendered chords, missed features and work.
const {spawn}=require('node:child_process'),{once}=require('node:events');
const fs=require('node:fs/promises'),path=require('node:path');
const args=process.argv.slice(2),option=n=>args.includes(n)?args[args.indexOf(n)+1]:undefined;
const executable=option('--server'),before=option('--before'),output=option('--output')||'build/graph-audit.json',repeats=Number(option('--repeats')||3);
const suite=option('--suite')||'default';if(!['default','narrow','exponential','steps','dense'].includes(suite))throw Error('Unknown suite');
const pause=ms=>new Promise(r=>setTimeout(r,ms)),pi=Math.PI;
const cases=[
 ['linear','3x+2',x=>3*x+2],['parabola','x*x',x=>x*x],['cubic','x^3-3x',x=>x*x*x-3*x],['degree10','x^10',x=>x**10],
 ['gaussian','exp(-(x-0.37)^2)',x=>Math.exp(-((x-.37)**2)),{anchors:[.37]}],
 ['gaussian-narrow','exp(-((x-0.1234)/0.04)^2)',x=>Math.exp(-(((x-.1234)/.04)**2)),{anchors:[.1234]}],
 ['gaussian-very-narrow','exp(-((x-0.1234)/0.005)^2)',x=>Math.exp(-(((x-.1234)/.005)**2)),{anchors:[.1234]}],
 ['lorentz-narrow','1/(1+((x-0.1234)/0.04)^2)',x=>1/(1+((x-.1234)/.04)**2),{anchors:[.1234]}],
 ['exponential','exp(x)',Math.exp],['log-quadratic','ln(1+x*x)',x=>Math.log(1+x*x)],['absolute-cusp','abs(x-0.1234)',x=>Math.abs(x-.1234)],
 ['sqrt','sqrt(x)',x=>x>=0?Math.sqrt(x):NaN],['cbrt','cbrt(x)',Math.cbrt],['fractional-power','x^(1/3)',x=>x>=0?x**(1/3):NaN],
 ['log','ln(x)',x=>x>0?Math.log(x):NaN],['shifted-log','ln(x-0.1234)',x=>x>.1234?Math.log(x-.1234):NaN],
 ['nested-log','1/ln(ln(sqrt(x)))^2',x=>x>1?1/Math.log(Math.log(Math.sqrt(x)))**2:NaN,{poles:[Math.exp(2)]}],
 ['nested-log-wide','1/ln(ln(sqrt(x)))^2',x=>x>1?1/Math.log(Math.log(Math.sqrt(x)))**2:NaN,{min:1.001,max:20,ymax:100,poles:[Math.exp(2)]}],
 ['reciprocal','1/x',x=>x===0?NaN:1/x,{poles:[0]}],['shifted-pole','1/(x-0.1234)',x=>1/(x-.1234),{poles:[.1234]}],
 ['squared-pole','1/(x-0.1234)^2',x=>1/(x-.1234)**2,{poles:[.1234]}],
 ['paired-poles','1/((x-0.1234)*(x-0.12341))',x=>1/((x-.1234)*(x-.12341)),{min:-1,max:1,ymin:-1000,ymax:1000,poles:[.1234,.12341]}],
 ['removable-hole','sin(x)/x',x=>x===0?NaN:Math.sin(x)/x,{poles:[0]}],
 ['tangent','tan(x)',Math.tan,{poles:Array.from({length:7},(_,i)=>(i-3)*pi+pi/2)}],
 ['sine','sin(x)',Math.sin],['sine33','sin(33x)',x=>Math.sin(33*x)],['sine1000','sin(1000x)',x=>Math.sin(1000*x)],
 ['multi-carrier','sin(1000x)+sin(1001x)',x=>Math.sin(1000*x)+Math.sin(1001*x)],
 ['chirp','sin(x*x)',x=>Math.sin(x*x)],['chirp-wide','sin(x*x)',x=>Math.sin(x*x),{min:-100,max:100}],
 ['log-phase','sin(ln(x))',x=>x>0?Math.sin(Math.log(x)):NaN],['inverse-phase','sin(1/x)',x=>x===0?NaN:Math.sin(1/x),{poles:[0]}],
 ['hyperbolic','sinh(x)',Math.sinh],['hyperbolic-steep','sinh(10x)',x=>Math.sinh(10*x)],['cosh-steep','cosh(10x)',x=>Math.cosh(10*x),{anchors:[0]}],
 ['atan','atan(100x)',x=>Math.atan(100*x)],['asin','asin(x/10)',x=>Math.abs(x)<=10?Math.asin(x/10):NaN],['tanh','tanh(10x)',x=>Math.tanh(10*x)],
 ['floor','floor(x)',Math.floor,{breaks:Array.from({length:21},(_,i)=>i-10)}],
 ['floor-wide','floor(x)',Math.floor,{min:-100,max:100,breaks:Array.from({length:201},(_,i)=>i-100)}],
 ['ceil','ceil(x)',Math.ceil,{breaks:Array.from({length:21},(_,i)=>i-10)}],
 ['complex-abs','abs(sqrt(x))',x=>Math.sqrt(Math.abs(x))],['complex-real','re(sqrt(x))',x=>x<0?0:Math.sqrt(x)],['complex-imag','im(sqrt(x))',x=>x<0?Math.sqrt(-x):0],
 ['inverse-sqrt','1/sqrt(x)',x=>x>0?1/Math.sqrt(x):NaN,{poles:[0]}],
 ['negative-sqrt','sqrt(-x)',x=>x<=0?Math.sqrt(-x):NaN],
 ['huge-sine','sin(x)',Math.sin,{min:1e12,max:1e12+20}],
 ['deep-sine','sin(x)',Math.sin,{min:-pi/2-1e-5,max:-pi/2+1e-5,ymin:-1.000000001,ymax:-.999999999}],
 ['tiny-linear','x',x=>x,{min:-1e-8,max:1e-8,ymin:-1e-8,ymax:1e-8}],
 ['tiny-log','ln(1+x)',x=>Math.log1p(x),{min:-1e-8,max:1e-8,ymin:-1e-8,ymax:1e-8}],
 ['broad-sine','sin(x)',Math.sin,{min:-1000,max:1000}],['broad-polynomial','x*x',x=>x*x,{min:-1e6,max:1e6}]
].map(([name,expression,f,opts={}])=>({name,expression,f,min:-10,max:10,ymin:-10,ymax:10,width:800,height:440,...opts}));
const narrowCases=[];
for(const [profile,width,height] of [['desktop',800,440],['small',320,300]])
 for(const centre of [.1234,-.43127,.00037,3.78123])for(const spread of [.2,.04,.004,.0004])for(const sign of [1,-1]){
  narrowCases.push({name:`narrow-${profile}-${centre}-${spread}-${sign}`,expression:`${sign}/(1+((x-(${centre}))/${spread})^2)`,
   f:x=>sign/(1+((x-centre)/spread)**2),min:-10,max:10,ymin:-10,ymax:10,width,height,
   anchors:[centre,...[-2,-1,-.5,.5,1,2].map(v=>centre+v*spread)]});
 }
const lorentz=(x,c,w)=>1/(1+((x-c)/w)**2);
for(const [name,expression,f,anchors] of [
 ['trend','3+2/(1+((x-0.1234)/0.004)^2)+x/100',x=>3+2*lorentz(x,.1234,.004)+x/100,[.1234]],
 ['offset-valley','2-1/(1+((x-0.1234)/0.004)^2)',x=>2-lorentz(x,.1234,.004),[.1234]],
 ['two-peaks','1/(1+((x-0.1234)/0.004)^2)+1/(1+((x+0.43127)/0.01)^2)',x=>lorentz(x,.1234,.004)+lorentz(x,-.43127,.01),[.1234,-.43127]],
 ['edge','1/(1+((x-9.999)/0.004)^2)',x=>lorentz(x,9.999,.004),[9.999]]
])narrowCases.push({name:'narrow-'+name,expression,f,anchors,min:-10,max:10,ymin:-10,ymax:10,width:800,height:440});
const exponentialCases=[];
for(const scale of [10,1000])for(const [profile,width,height] of [['desktop',800,440],['small',320,300]])
 for(const centre of [0,.1234,-.43127])for(const spread of [.04,.005,.0004])for(const sign of [1,-1])
  exponentialCases.push({name:`exp-${scale}-${profile}-${centre}-${spread}-${sign}`,expression:`${sign}*exp(-((x-(${centre}))/${spread})^2)`,
   f:x=>sign*Math.exp(-(((x-centre)/spread)**2)),min:-scale,max:scale,ymin:-10,ymax:10,width,height,
   anchors:[centre,...[-2,-1,-.5,.5,1,2].map(v=>centre+v*spread)]});
const gaussian=(x,c,w)=>Math.exp(-(((x-c)/w)**2));
for(const [name,expression,f,anchors,poles=[]] of [
 ['trend','3-2*exp(-((x-.1234)/.005)^2)+x/100',x=>3-2*gaussian(x,.1234,.005)+x/100,[.1234]],
 ['two-peaks','exp(-((x-.1234)/.005)^2)+exp(-((x+.43127)/.0004)^2)',x=>gaussian(x,.1234,.005)+gaussian(x,-.43127,.0004),[.1234,-.43127]],
 ['edge','exp(-((x-9.999)/.005)^2)',x=>gaussian(x,9.999,.005),[9.999]],
 ['hole','exp(-1/x^2)',x=>x===0?NaN:Math.exp(-1/(x*x)),[],[0]]
])exponentialCases.push({name:'exp-'+name,expression,f,anchors,poles,min:-10,max:10,ymin:-10,ymax:10,width:800,height:440});
const controls=cases.filter(c=>['linear','parabola','sine','sine33','tangent','paired-poles'].includes(c.name));
const even=x=>{const n=Math.floor(x),t=x-n;return t<.5?n:t>.5?n+1:n%2===0?n:n+1;};
const boundaries=(scale,shift,min,max,kind)=>{
 const low=Math.floor(Math.min(scale*min+shift,scale*max+shift))-1,high=Math.ceil(Math.max(scale*min+shift,scale*max+shift))+1,result=[];
 for(let k=low;k<=high;k++){
  if(kind==='trunc'&&k===0)continue;
  const argument=kind==='round'?k+.5:k,x=(argument-shift)/scale;
  if(x<min||x>max)continue;
  const left=kind==='ceil'||(kind==='trunc'&&k<0)||(kind==='round'&&k%2===0);
  result.push({x,side:(scale>0?left:!left)?'left':'right'});
 }return result;
};
const stepCases=[];
for(const [profile,width,height] of [['desktop',800,440],['small',320,300]])for(const span of [10,100])
 for(const [kind,f] of [['floor',Math.floor],['ceil',Math.ceil],['trunc',Math.trunc],['round',even]])
  stepCases.push({name:`step-${profile}-${span}-${kind}`,expression:`${kind}(x)`,f,min:-span,max:span,ymin:-10,ymax:10,width,height,breaks:boundaries(1,0,-span,span,kind)});
for(const [name,expression,f,opts] of [
 ['shifted','floor(3*x+0.37)',x=>Math.floor(3*x+.37),{breaks:boundaries(3,.37,-100,100,'floor')}],
 ['reversed','ceil(-2*x+0.37)',x=>Math.ceil(-2*x+.37),{breaks:boundaries(-2,.37,-100,100,'ceil')}],
 ['trend','floor(x)+x/10',x=>Math.floor(x)+x/10,{breaks:boundaries(1,0,-100,100,'floor')}],
 ['tiny','0.000000001*floor(x)',x=>1e-9*Math.floor(x),{breaks:boundaries(1,0,-100,100,'floor')}],
 ['composition','floor(x)+ceil(2*x+0.37)',x=>Math.floor(x)+Math.ceil(2*x+.37),{breaks:[...boundaries(1,0,-100,100,'floor'),...boundaries(2,.37,-100,100,'ceil')]}],
 ['periodic-floor','floor(sin(3*x))',x=>Math.floor(Math.sin(3*x)),{min:-10,max:10}],
 ['periodic-ceil','ceil(sin(3*x))',x=>Math.ceil(Math.sin(3*x)),{min:-10,max:10}],
 ['decimal-round','round(x;1)',x=>even(x*10)/10,{min:-2,max:2,breaks:boundaries(10,0,-2,2,'round')}],
 ['sparse','floor(x/100)',x=>Math.floor(x/100),{min:-10000,max:10000,breaks:boundaries(.01,0,-10000,10000,'floor')}]
])stepCases.push({name:'step-'+name,expression,f,min:-100,max:100,ymin:-10,ymax:10,width:800,height:440,...opts});
const denseCases=[];
for(const [profile,width,height] of [['desktop',800,440],['small',320,300]])for(const [name,expression,f] of [
 ['beats','sin(1000x)+sin(1001x)',x=>Math.sin(1000*x)+Math.sin(1001*x)],
 ['affine','x*(sin(1000x)+cos(1100x))+2',x=>x*(Math.sin(1000*x)+Math.cos(1100*x))+2],
 ['scaled','-(sin(1000x)-2*cos(-1001x+0.3))/2+x/10',x=>-(Math.sin(1000*x)-2*Math.cos(-1001*x+.3))/2+x/10],
 ['four','sin(1000x)+cos(1100x)-sin(1200x)+cos(1300x)',x=>Math.sin(1000*x)+Math.cos(1100*x)-Math.sin(1200*x)+Math.cos(1300*x)],
 ['repeated','sin(1000x)+sin(1000x)+cos(1100x)',x=>2*Math.sin(1000*x)+Math.cos(1100*x)]
])for(const [scale,min,max] of [['wide',-10,10],['resolved',-.001,.001]])denseCases.push({name:`dense-${profile}-${name}-${scale}`,expression,f,min,max,ymin:-10,ymax:10,width,height});
const selectedCases=suite==='narrow'?[...narrowCases,...controls]:suite==='exponential'?[...exponentialCases,...controls]:suite==='steps'?[...stepCases,...controls]:suite==='dense'?[...denseCases,...cases.filter(c=>['chirp-wide','inverse-phase'].includes(c.name)),...controls]:cases;
async function server(binary,port){
 const base='http://127.0.0.1:'+port;const occupied=await fetch(base,{signal:AbortSignal.timeout(500)}).catch(()=>null);if(occupied){await occupied.arrayBuffer();throw Error('Audit port occupied');}
 const child=spawn(path.resolve(binary),['--no-browser','--port',String(port)],{windowsHide:true,stdio:'ignore'});let failure;child.on('error',e=>failure=e);
 const close=async()=>{if(child.exitCode===null&&!failure){const exit=once(child,'exit');child.kill();await exit;}};
 try{for(let i=0;i<80;i++){if(failure)throw failure;if(child.exitCode!==null)throw Error('Server exited');try{const r=await fetch(base+'/graph');await r.arrayBuffer();if(r.ok)return{base,close};}catch(_){}await pause(100);}throw Error('Readiness timeout');}catch(e){await close();throw e;}
}
function assess(item,data){
 const points=data.points.map((p,i)=>Array.isArray(p)?{x:p[0],y:p[1],kind:p[3]===0?'real':p[3],connect:!!p[2],slope:data.slopes?.[i]}:{x:Number(p.x),y:p.y===null?null:Number(p.y),kind:p.kind,connect:p.connect,slope:null});
 let pointError=0,pointChecks=0,invalidEdges=0,poleCrossings=0,jumpCrossings=0;
 for(let i=0;i<points.length;i++){const p=points[i];if(p.kind==='real'&&p.y!==null&&p.y>=item.ymin&&p.y<=item.ymax){const y=item.f(p.x);if(Number.isFinite(y)){pointError=Math.max(pointError,Math.abs(p.y-y)/Math.max(1,Math.abs(y)));pointChecks++;}}
  if(!p.connect)continue;const a=points[i-1];if(!a||a.kind!=='real'||p.kind!=='real'||a.y===null||p.y===null){invalidEdges++;continue;}
  poleCrossings+=(item.poles||[]).filter(x=>a.x<x&&x<p.x).length;jumpCrossings+=(item.breaks||[]).filter(boundary=>{
   const x=typeof boundary==='number'?boundary:boundary.x;
   return a.y!==p.y&&(boundary.side==='left'?a.x<=x&&x<p.x:a.x<x&&x<=p.x);
  }).length;
 }
 const bands=data.bands||[];let visible=0,covered=0,bandCovered=0,missing=0,error=0,worst=null;
 function rendered(x){let lo=0,hi=points.length;while(lo<hi){const m=(lo+hi)>>>1;if(points[m].x<x)lo=m+1;else hi=m;}const b=points[lo],a=points[lo-1];
  if(b?.x===x&&b.kind==='real'&&b.y!==null&&((a?.kind==='real'&&b.connect)||(points[lo+1]?.kind==='real'&&points[lo+1].connect)))return b.y;
  if(!a||!b||!b.connect||a.kind!=='real'||b.kind!=='real'||a.y===null||b.y===null||b.x<=a.x)return null;
  const t=(x-a.x)/(b.x-a.x),dy=b.y-a.y,dx=b.x-a.x;let y=a.y+dy*t;
  if(dx*(item.width-86)/(item.max-item.min)>1&&dy&&Number.isFinite(a.slope)&&Number.isFinite(b.slope)){
   let alpha=Math.max(0,a.slope/(dy/dx)),beta=Math.max(0,b.slope/(dy/dx));const norm=Math.hypot(alpha,beta);if(norm>3){alpha*=3/norm;beta*=3/norm;}
   const c1=a.y+dy*alpha/3,c2=b.y-dy*beta/3;y=(1-t)**3*a.y+3*(1-t)**2*t*c1+3*(1-t)*t*t*c2+t**3*b.y;
  }return y;
 }
 const coordinates=Array.from({length:4097},(_,i)=>item.min+(item.max-item.min)*i/4096).concat(item.anchors||[]);
 for(const x of coordinates){const y=item.f(x);if(!Number.isFinite(y)||y<item.ymin||y>item.ymax)continue;visible++;
  if(bands.some(b=>b[0]<=x&&b[1]>=x&&b[2]<=y&&b[3]>=y)){bandCovered++;continue;}
  const drawn=rendered(x);if(drawn===null){missing++;continue;}covered++;const px=Math.abs(drawn-y)*(item.height-68)/(item.ymax-item.ymin);if(px>error){error=px;worst={x,expected:y,drawn,error_px:px};}
 }
 return{visible_checks:visible,visible_connected_checks:covered,visible_band_checks:bandCovered,missing_visible_checks:missing,visible_coverage:visible?(covered+bandCovered)/visible:null,max_render_error_px:error,worst,point_reference_checks:pointChecks,max_visible_point_relative_error:pointError,invalid_edges:invalidEdges,pole_crossings:poleCrossings,jump_crossings:jumpCrossings};
}
async function main(){
 async function save(audit){
  await fs.mkdir(path.dirname(output),{recursive:true});await fs.writeFile(output,JSON.stringify(audit,null,2)+'\n');
  if(option('--csv')){
   const fields=['version','case','expression','http_median_ms','http_min_ms','http_max_ms','response_bytes','ok','evaluation','count','bands','limited','screen_clipped','exact_probes','max_render_error_px','visible_coverage','missing_visible_checks','visible_checks','pole_crossings','jump_crossings','max_visible_point_relative_error'];
   const csv=[fields.join(',')],seen=new Set(),escape=v=>'"'+String(v??'').replaceAll('"','""')+'"';
   for(const row of audit.rows){const key=row.version+':'+row.case;if(seen.has(key))continue;seen.add(key);const samples=audit.rows.filter(r=>r.version===row.version&&r.case===row.case).map(r=>r.http_ms).sort((a,b)=>a-b),summary={...row,http_median_ms:samples[Math.floor(samples.length/2)],http_min_ms:samples[0],http_max_ms:samples.at(-1)};csv.push(fields.map(k=>escape(summary[k])).join(','));}
   await fs.mkdir(path.dirname(option('--csv')),{recursive:true});await fs.writeFile(option('--csv'),csv.join('\n')+'\n');
  }
 }
 if(option('--reassess')){
  const audit=JSON.parse(await fs.readFile(option('--reassess'),'utf8'));
  for(const row of audit.rows){const original=audit.rows.find(r=>r.version===row.version&&r.case===row.case&&r.payload);if(row.ok)Object.assign(row,assess([...cases,...narrowCases,...exponentialCases,...stepCases,...denseCases].find(c=>c.name===row.case),JSON.parse(original.payload)));}
  await save(audit);return;
 }
 if(!executable||!Number.isInteger(repeats)||repeats<1||repeats>5)throw Error('Use --server new.exe [--before old.exe] [--repeats 1..5] [--output file.json], or --reassess input.json --output file.json');
 const rows=[];for(const [version,binary,port] of [...(before?[['before',before,19505]]:[]),['current',executable,19506]]){
  const owned=await server(binary,port);
  try{for(const item of selectedCases){let last=null;for(let repeat=1;repeat<=repeats;repeat++){
   const q=new URLSearchParams({mode:'fast',min:String(item.min),max:String(item.max),ymin:String(item.ymin),ymax:String(item.ymax),width:String(item.width),height:String(item.height),interpolation:'hermite',dense:'band'});
   const start=performance.now(),response=await fetch(owned.base+'/api/graph/plot?'+q,{method:'POST',headers:{'Content-Type':'text/plain'},body:item.expression,signal:AbortSignal.timeout(12000)}),payload=await response.text(),ms=performance.now()-start,data=JSON.parse(payload);
   const row={version,case:item.name,expression:item.expression,camera:{min:item.min,max:item.max,ymin:item.ymin,ymax:item.ymax,width:item.width,height:item.height},repeat,http_ms:ms,response_bytes:Buffer.byteLength(payload),status:response.status,ok:!!data.ok,evaluation:data.evaluation,count:data.count,limited:data.limited,screen_clipped:!!data.screen_clipped,bands:data.bands?.length||0,exact_probes:data.exact_probes,...(data.ok?assess(item,data):{error:data.status||data.error}),...(repeat===1?{payload}:{})};rows.push(row);last=row;
  }process.stdout.write(JSON.stringify({version,case:item.name,ok:last.ok,ms:+last.http_ms.toFixed(2),points:last.count,limited:last.limited,coverage:last.visible_coverage,error_px:+(last.max_render_error_px||0).toFixed(3),pole_crossings:last.pole_crossings})+'\n');}}
  finally{await owned.close();}
 }
 await save({generated:new Date().toISOString(),current:path.resolve(executable),before:before?path.resolve(before):null,repeats,suite,note:'Owned loopback HTTP; 4097 independent reference coordinates plus specified feature anchors. Drawing reproduces the browser Hermite limiter and screen chord threshold; no timing thresholds. Conditional errors/coverage are diagnostics, not continuity proofs. JSON payloads retained for first repetition only.',rows});
}
module.exports={cases,assess,server};
if(require.main===module)main().catch(e=>{process.stderr.write(e.stack+'\n');process.exitCode=1;});
