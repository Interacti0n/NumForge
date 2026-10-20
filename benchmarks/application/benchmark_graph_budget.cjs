// Bounded graph work and HTTP head-of-line blocking, including client abort.
const {server}=require('./benchmark_graph_audit.cjs'),fs=require('node:fs/promises'),path=require('node:path');
const args=process.argv.slice(2),option=n=>args.includes(n)?args[args.indexOf(n)+1]:undefined;
const executable=option('--server'),terms=Number(option('--terms')||120),output=option('--output')||'build/graph-budget-audit.json';
if(!executable||!Number.isInteger(terms)||terms<1||terms>200)throw Error('Use --server executable [--terms 1..200] [--output file.json]');
const pause=ms=>new Promise(r=>setTimeout(r,ms)),q=new URLSearchParams({mode:'fast',min:'-10',max:'10',ymin:'-10',ymax:'10',width:'800',height:'440',interpolation:'hermite',dense:'band'});
(async()=>{
 const owned=await server(executable,19508),rows=[];
 const request=async(expression,signal)=>{const start=performance.now();try{const r=await fetch(owned.base+'/api/graph/plot?'+q,{method:'POST',headers:{'Content-Type':'text/plain'},body:expression,signal:signal||AbortSignal.timeout(15000)}),payload=await r.text();return{ms:performance.now()-start,http_status:r.status,bytes:Buffer.byteLength(payload),...JSON.parse(payload)};}catch(e){return{ms:performance.now()-start,transport_error:e.name+': '+e.message};}};
 const heavy='sum('+Array(terms).fill('atan(x)').join(';')+')';
 try{
  for(let repeat=1;repeat<=3;repeat++){
   const baseline=await request('x');let slow=request(heavy);await pause(100);const fast=await request('x');const completed=await slow;
   const recovery=await request('x');if(!baseline.ok||!fast.ok||!recovery.ok)throw Error('Server did not recover');
   const row={repeat,baseline,slow:completed,queued:fast,recovery};rows.push(row);process.stdout.write(JSON.stringify({repeat,baseline_ms:baseline.ms,slow_ms:completed.ms,slow_ok:completed.ok,slow_status:completed.error||'ok',queued_ms:fast.ms,recovery_ms:recovery.ms})+'\n');
  }
  const cancel=new AbortController(),pending=request(heavy,cancel.signal);await pause(100);cancel.abort();const aborted=await pending,following=await request('x');if(!following.ok)throw Error('Server did not recover after abort');
  await fs.mkdir(path.dirname(output),{recursive:true});await fs.writeFile(output,JSON.stringify({generated:new Date().toISOString(),expression:heavy,terms,rows,client_abort:{aborted,following},note:'Own loopback server; each queued x is sent 100 ms after a heavy graph. Timing includes the single server queue, computation and transfer. Client abort does not assert server-side preemption. No fixed timing thresholds.'},null,2)+'\n');
  process.stdout.write(JSON.stringify({client_abort_ms:aborted.ms,following_ms:following.ms})+'\n');
 }finally{await owned.close();}
})().catch(e=>{process.stderr.write(e.stack+'\n');process.exitCode=1;});
