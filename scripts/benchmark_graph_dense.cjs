// Isolated HTTP diagnostics with independent numeric checks and owned servers.
const {spawn}=require('node:child_process'),{once}=require('node:events'),fs=require('node:fs/promises'),path=require('node:path');
const args=process.argv.slice(2),option=name=>args.includes(name)?args[args.indexOf(name)+1]:undefined;
const before=option('--before'),after=option('--after'),output=option('--output')||'build/graph-dense-http.json',repeats=Number(option('--repeats')||3);
const suite=option('--suite')||'dense';if(!['dense','domains'].includes(suite))throw Error('Unknown --suite; use dense or domains');
if(!before||!after||!Number.isInteger(repeats)||repeats<1||repeats>10)throw Error('Use --before old.exe --after new.exe [--repeats 1..10] [--output file.json]');
const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
const cases=suite==='domains'?[
 {name:'nested-log-default',expression:'1/ln(ln(sqrt(x)))^2',width:800,band:false,f:x=>1/Math.log(Math.log(Math.sqrt(x)))**2},
 {name:'nested-log-both-branches',expression:'1/ln(ln(sqrt(x)))^2',min:1.001,max:20,ymin:-10,ymax:100,width:800,band:false,f:x=>1/Math.log(Math.log(Math.sqrt(x)))**2},
 {name:'nested-log-wide',expression:'1/ln(ln(sqrt(x)))^2',min:2,max:30,width:800,band:false,f:x=>1/Math.log(Math.log(Math.sqrt(x)))**2},
 {name:'nested-log-pole',expression:'1/ln(ln(sqrt(x)))^2',min:7,max:8,ymin:-10,ymax:1000,width:800,band:false,f:x=>1/Math.log(Math.log(Math.sqrt(x)))**2},
 {name:'tangent-default',expression:'tan(x)',width:800,band:false,f:x=>Math.tan(x)},
 {name:'reciprocal-square',expression:'1/(x-0.1)^2',width:800,band:false,f:x=>1/(x-.1)**2},
 {name:'smooth-cubic-control',expression:'x^3-3x',width:800,band:false,f:x=>x*x*x-3*x},
 {name:'sine-33-control',expression:'sin(33x)',width:800,band:false,f:x=>Math.sin(33*x)},
 {name:'sine-1000-envelope-control',expression:'sin(1000x)',width:800,band:true,f:x=>Math.sin(1000*x)}
]:[
 {name:'sine-1000-desktop',expression:'sin(1000x)',width:800,band:true,f:x=>Math.sin(1000*x)},
 {name:'sine-1000-mobile-layout',expression:'sin(1000x)',width:320,band:true,f:x=>Math.sin(1000*x)},
 {name:'cosine-20000-shift',expression:'2-3*cos(20000x+0.3)',width:800,band:true,f:x=>2-3*Math.cos(20000*x+.3)},
 {name:'linear-amplitude',expression:'x*sin(1000x)+2',width:800,band:true,f:x=>x*Math.sin(1000*x)+2},
 {name:'linear-trend',expression:'sin(1000x)+x',width:800,band:true,f:x=>Math.sin(1000*x)+x},
 {name:'wide-sine-33',expression:'sin(33x)',width:800,min:-1000,max:1000,band:true,f:x=>Math.sin(33*x)},
 {name:'resolved-sine-1000',expression:'sin(1000x)',width:800,min:-.01,max:.01,band:false,f:x=>Math.sin(1000*x)},
 {name:'ordinary-sine-33',expression:'sin(33x)',width:800,band:false,f:x=>Math.sin(33*x)},
 {name:'tangent-poles',expression:'tan(1000x)',width:800,band:false},
 {name:'variable-denominator',expression:'sin(1000x)/(x-0.1)',width:800,band:false},
 {name:'multiple-carriers',expression:'sin(1000x)+sin(1001x)',width:800,band:false}
];
async function server(executable,port){
 const child=spawn(path.resolve(executable),['--no-browser','--port',String(port)],{windowsHide:true,stdio:'ignore'});let launchError;child.on('error',e=>launchError=e);
 const close=async()=>{if(child.exitCode===null&&!launchError){const exit=once(child,'exit');child.kill();await exit;}};
 try{for(let i=0;i<60;i++){if(launchError)throw launchError;if(child.exitCode!==null)throw Error('Server exited');try{const r=await fetch(`http://127.0.0.1:${port}/graph`);if(r.ok)return{url:`http://127.0.0.1:${port}`,close};}catch(_){}await pause(100);}throw Error('Readiness timeout');}catch(e){await close();throw e;}
}
(async()=>{
 const rows=[];
 for(const [version,executable,port] of [['before',before,19493],['after',after,19494]]){
  const owned=await server(executable,port);
  try{for(const item of cases)for(let repeat=1;repeat<=repeats;repeat++){
   const query=new URLSearchParams({mode:'fast',min:String(item.min??-10),max:String(item.max??10),width:String(item.width),height:'440',ymin:String(item.ymin??-10),ymax:String(item.ymax??10),interpolation:'hermite',...(version==='after'||suite==='domains'?{dense:'band'}:{})});
   const start=performance.now(),response=await fetch(owned.url+'/api/graph/plot?'+query,{method:'POST',headers:{'Content-Type':'text/plain; charset=utf-8'},body:item.expression}),payload=await response.text(),elapsed=performance.now()-start,data=JSON.parse(payload);
   if(!response.ok||!data.ok)throw Error(item.name+': '+payload);
   const bands=data.bands||[];if(version==='after'&&!!bands.length!==item.band)throw Error(item.name+': unexpected representation');
   let probes=0;
   if(item.f)for(let i=0;i<bands.length;i+=Math.max(1,Math.floor(bands.length/64))){const [a,b,low,high]=bands[i];for(let j=0;j<=8;j++){const y=item.f(a+(b-a)*j/8);if(y<low-1e-8||y>high+1e-8)throw Error(item.name+': envelope reference mismatch');++probes;}}
   if(suite==='domains'&&item.f)for(const p of data.points){const compact=Array.isArray(p),x=Number(compact?p[0]:p.x),y=Number(compact?p[1]:p.y),real=compact?p[3]===0:p.kind==='real';if(!real||!Number.isFinite(y)||y<(item.ymin??-10)||y>(item.ymax??10))continue;const ref=item.f(x);if(!Number.isFinite(ref)||Math.abs(ref-y)>2e-8*Math.max(1,Math.abs(ref)))throw Error(item.name+': visible point reference mismatch');++probes;}
   const row={version,case:item.name,expression:item.expression,width:item.width,min:item.min??-10,max:item.max??10,ymin:item.ymin??-10,ymax:item.ymax??10,repeat,http_ms:+elapsed.toFixed(3),response_bytes:Buffer.byteLength(payload),count:data.count,bands:bands.length,accepted_edges:data.points.slice(1).filter(p=>Array.isArray(p)?p[2]===1:p.connect).length,limited:data.limited,evaluation:data.evaluation,screen_clipped:!!data.screen_clipped,exact_probes:data.exact_probes,reference_checks:probes,...(suite==='domains'?{payload}:{})};rows.push(row);
  }}finally{await owned.close();}
 }
 await fs.mkdir(path.dirname(output),{recursive:true});await fs.writeFile(output,JSON.stringify({generated:new Date().toISOString(),before:path.resolve(before),after:path.resolve(after),suite,repeats,note:'Loopback HTTP wall time includes evaluation, serialization, transfer and JSON text receipt; not isolated arithmetic or WAN latency. Values are independent approximate JS references; domains suite checks visible points and retains raw JSON for decimal oracles. No timing thresholds.',rows},null,2)+'\n');
 for(const version of ['before','after'])for(const item of cases){const selected=rows.filter(r=>r.version===version&&r.case===item.name),median=key=>selected.map(r=>r[key]).sort((a,b)=>a-b)[Math.floor(selected.length/2)];process.stdout.write(JSON.stringify({version,case:item.name,http_ms:median('http_ms'),bytes:median('response_bytes'),points:median('count'),bands:median('bands'),limited:selected[0].limited})+'\n');}
})().catch(error=>{process.stderr.write(error.stack+'\n');process.exitCode=1;});
