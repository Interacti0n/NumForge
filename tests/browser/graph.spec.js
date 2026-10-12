const {test,expect}=require('@playwright/test');
const {randomBytes}=require('node:crypto');
test('domain boundaries and poles stay disconnected on shifted and broad ranges',async({request})=>{
 test.setTimeout(30000);
 for(const [expression,poles,reference,domain] of [
  ['tan(x)',[-3*Math.PI/2,-Math.PI/2,Math.PI/2,3*Math.PI/2],Math.tan,()=>true],
  ['1/x',[0],x=>1/x,x=>x!==0],['1/(x-2)',[2],x=>1/(x-2),x=>x!==2],
  ['sqrt(x)',[],Math.sqrt,x=>x>=0],['ln(x)',[0],Math.log,x=>x>0]]){
  for(const [min,max] of [['-10','10'],['-3.7','5.3']]){
   const response=await request.post('/api/graph/plot?'+new URLSearchParams({min,max}),{data:expression});
   expect(response.ok()).toBe(true);const data=await response.json();expect(data.count).toBeLessThanOrEqual(1024);
   expect(data.points.some(p=>p.connect)).toBe(true);
   const violations=[];let maximumError=0;
   data.points.forEach((p,i)=>{
    const x=Number(p.x);if(p.kind==='real'){
     if(!domain(x))violations.push(`real point outside domain at ${x}`);
     maximumError=Math.max(maximumError,Math.abs(Number(p.y)-reference(x))/Math.max(1,Math.abs(reference(x))));
    }
    if(p.connect){const previous=data.points[i-1];
     if(!previous||previous.kind!=='real'||p.kind!=='real')violations.push(`invalid edge at ${x}`);
     for(const pole of poles)if(previous&&Number(previous.x)<pole&&x>pole)violations.push(`edge across pole ${pole}`);
    }
   });
   expect(violations,`${expression} on ${min}..${max}`).toEqual([]);expect(maximumError).toBeLessThan(1e-9);
  }
 }
});
test('adaptive projection retains close sine values and uniform projection remains twelve places',async({request})=>{
 const response=await request.post('/api/graph/plot?min=-4.71238798038469&max=-4.71238098038469',{data:'sin(x)'});
 expect(response.ok()).toBe(true);const data=await response.json();expect(data.plot_places).toBe(16);
 expect(new Set(data.points.map(p=>p.y)).size).toBeGreaterThan(40);
 for(const p of data.points)expect(Math.abs(Number(p.y)-Math.sin(Number(p.x)))).toBeLessThan(4e-16);
 const uniform=await request.post('/api/graph/sample?min=-1&max=1&points=3',{data:'sin(x)'});
 expect((await uniform.json()).plot_places).toBe(12);
});
test('sin and cos preserve every periodic peak on coarse wide views',async({request})=>{
 test.setTimeout(30000);
 for(const [expression,offset,angle,period] of [['sin(x)',Math.PI/2,'rad',Math.PI],['cos(-x)',0,'rad',Math.PI],['sin(33x)',Math.PI/2,'rad',Math.PI],['sin(x)',90,'deg',180]]) {
  const factor=expression.includes('33')?33:1,min=expression.includes('33')?-10:-100,max=expression.includes('33')?10:611;
  const response=await request.post('/api/graph/plot?'+new URLSearchParams({min:String(min),max:String(max),angle}),{data:expression});
  expect(response.ok()).toBe(true);const data=await response.json();expect(data.ok).toBe(true);expect(data.limited).toBe(false);
  expect(data.count).toBeLessThanOrEqual(1024);expect(data.points.slice(1).every(p=>p.connect)).toBe(true);
  for(let k=Math.ceil((min*factor-offset)/period);offset+k*period<max*factor;++k){
   const x=(offset+k*period)/factor,point=data.points.find(p=>Math.abs(Number(p.x)-x)<1e-7);
   expect(point,`${expression} missing extremum ${x}`).toBeTruthy();expect(Math.abs(Number(point.y))).toBeCloseTo(1,9);
  }
 }
});
test('adaptive y view fits discovered extrema and keeps pole overview bounded',async({request})=>{
 const sample=async expression=>(await request.post('/api/graph/plot?min=-10&max=10',{data:expression})).json();
 const peak=await sample('-(x-0.3125)^2');expect(peak.ok).toBe(true);expect(Number(peak.y_max)).toBe(0);
 for(const p of peak.points){expect(Number(p.y)).toBeGreaterThanOrEqual(Number(peak.y_min));expect(Number(p.y)).toBeLessThanOrEqual(Number(peak.y_max));}
 const pole=await sample('1/(x-0.1)');expect(pole.ok).toBe(true);expect(pole.limited).toBe(true);
 expect(Math.max(Math.abs(Number(pole.y_min)),Math.abs(Number(pole.y_max)))).toBeLessThan(20);
 expect(pole.points.some(p=>Math.abs(Number(p.y))>100)).toBe(true);
});
test('adaptive harder functions, broad ranges and oscillation safeguards',async({request})=>{
 test.setTimeout(30000);
 const sample=async(expression,min,max,precision='34')=>{
  const response=await request.post('/api/graph/plot?'+new URLSearchParams({min,max,precision}),{data:expression});
  expect(response.ok()).toBe(true);const data=await response.json();expect(data.ok).toBe(true);
  expect(data.count).toBeLessThanOrEqual(1024);return data;
 };
 const sine=await sample('sin(3x)','-10','10');expect(sine.limited).toBe(false);
 expect(sine.points.slice(1).every(p=>p.connect)).toBe(true);
 const dense=await sample('sin(33x)','-10','10');expect(dense.limited).toBe(false);
 expect(dense.count).toBeLessThan(900);expect(dense.points.slice(1).every(p=>p.connect)).toBe(true);
 dense.points.forEach((p,i)=>{
  const x=Number(p.x),y=Number(p.y);expect(Math.abs(y-Math.sin(33*x))).toBeLessThan(1e-10);
  if(i){const a=dense.points[i-1],ax=Number(a.x),ay=Number(a.y);
   for(const t of [.25,.5,.75])expect(Math.abs(ay+(y-ay)*t-Math.sin(33*(ax+(x-ax)*t)))).toBeLessThan(.21);
  }
 });
 for(const [expression,reference] of [
  ['sin(3x)',x=>Math.sin(3*x)],['sin(3x)+cos(2x)',x=>Math.sin(3*x)+Math.cos(2*x)],
  ['sin(3x)*exp(-x*x/20)',x=>Math.sin(3*x)*Math.exp(-x*x/20)]]){
  const data=await sample(expression,'-10','10');let edges=0;expect(data.limited).toBe(false);
  const span=Number(data.y_max)-Number(data.y_min);
  data.points.forEach((p,i)=>{
   const x=Number(p.x),y=Number(p.y);expect(Math.abs(y-reference(x))).toBeLessThan(1e-10);
   if(p.connect){++edges;const a=data.points[i-1],ax=Number(a.x),ay=Number(a.y);
    for(const t of [.25,.5,.75])expect(Math.abs(ay+(y-ay)*t-reference(ax+(x-ax)*t))/span).toBeLessThan(.0055);
   }
  });expect(edges).toBe(data.count-1);
 }
 for(const expression of ['3*x+2','x*x','ln(1+x*x)']){
  const data=await sample(expression,'-1000000000000','1000000000000');expect(data.points.some(p=>p.connect)).toBe(true);
 }
 const precise=await sample('sin(3x)','-1','1','100');expect(precise.points.some(p=>p.connect)).toBe(true);
 for(const expression of ['sin(x)','sin(sin(1000000*x))']){
  const data=await sample(expression,'-1000000','1000000');expect(data.limited).toBe(true);
  expect(data.points.every(p=>!p.connect)).toBe(true);
 }
});
const route=(extra={})=>'/api/graph/sample?'+new URLSearchParams({min:'-1',max:'1',points:'3',...extra});
async function graph(request,input,extra={}) {
    const response=await request.post(route(extra),{data:input});
    return {response,data:await response.json()};
}
test('graph exact grid projections, domains and classifications',async({request})=>{
    const square=await graph(request,'x*x',{min:'0',points:'4'});
    expect(square.response.ok()).toBe(true);expect(square.data.count).toBe(4);
    const expected=[0,1/3,2/3,1];
    square.data.points.forEach((p,i)=>{
        expect(p.kind).toBe('real');expect(Number(p.x)).toBeCloseTo(expected[i],11);
        expect(Number(p.y)).toBeCloseTo(expected[i]**2,11);expect(p.source_approximate).toBe(false);
    });
    const reciprocal=(await graph(request,'1/x')).data;
    expect(reciprocal.points.map(p=>p.kind)).toEqual(['real','domain_error','real']);
    expect(reciprocal.points[1].y).toBeNull();expect(reciprocal.points[1].status).toBe('division by zero');
    expect(reciprocal.points[1].column).toBeGreaterThan(0);
    expect((await graph(request,'sqrt(x)')).data.points[0].kind).toBe('complex');
    expect((await graph(request,'qty(x;"m")')).data.points.every(p=>p.kind==='quantity'&&p.y===null)).toBe(true);
    const degrees=(await graph(request,'sin(t)',{parameter:'t',min:'0',max:'90',angle:'deg'})).data;
    expect(Number(degrees.points[2].y)).toBe(1);
    const maximum=await graph(request,'x',{points:'1024'});
    expect(maximum.response.ok()).toBe(true);expect(maximum.data.points).toHaveLength(1024);
});
test('graph reads session snapshots without mutations or reviving clients',async({request})=>{
    const client=randomBytes(16).toString('hex'), missing=randomBytes(16).toString('hex');
    const base=`/api/evaluate?precision=full&angle=rad&client=${client}`;
    expect((await request.post(base+'&revision=1&action=start',{data:''})).ok()).toBe(true);
    expect((await request.post(base+'&revision=2&action=commit',{data:'y=1/3'})).ok()).toBe(true);
    expect((await request.post(base+'&revision=3&action=preview',{data:'rand()'})).ok()).toBe(true);
    const read=async()=>Promise.all(['/api/session','/api/session/variables','/api/session/history'].map(async(path)=>(await request.get(`${path}?client=${client}`)).json()));
    const before=await read();
    const sampled=await graph(request,'x+y+ans',{client,at:'3'});
    expect(sampled.response.ok()).toBe(true);expect(Number(sampled.data.points[1].y)).toBeCloseTo(2/3,11);
    expect(await read()).toEqual(before);
    expect((await graph(request,'x',{client,at:'2'})).response.status()).toBe(409);
    expect((await graph(request,'x',{client:missing})).data.ok).toBe(false);
    expect((await request.get('/api/session?client='+missing)).ok()).toBe(false);
    expect((await graph(request,'y',{client:missing})).data.ok).toBe(false);
    expect(await read()).toEqual(before);
});
test('graph strict schema and inherited HTTP origin protections',async({request})=>{
    for(const extra of [{points:'1'},{points:'1025'},{precision:'0'},{min:'1/3'},{max:'-1'},{parameter:'i'},{action:'commit'},{at:'0'}])
        expect((await graph(request,'x',extra)).response.ok()).toBe(false);
    for(const input of ['x=7','rand()','x+random()','x+unknown']) expect((await graph(request,input)).response.ok()).toBe(false);
    expect((await request.post(route()+'&points=4',{data:'x'})).ok()).toBe(false);
    expect((await request.post(route(),{data:'x',headers:{Origin:'https://untrusted.example'}})).status()).toBe(403);
    expect((await request.get(route())).ok()).toBe(false);
    const unicode=await graph(request,'π+missing');
    expect(unicode.response.ok()).toBe(false);expect(unicode.data.column).toBe(3);
});

test('adaptive sampler leaves tangent poles and step jumps disconnected',async({request})=>{
 const sample=async expression=>(await request.post('/api/graph/plot?min=-2&max=2',{data:expression})).json();
 const tangent=await sample('tan(x)');expect(tangent.ok).toBe(true);expect(tangent.limited).toBe(true);
 for(let i=1;i<tangent.points.length;i++)for(const pole of [-Math.PI/2,Math.PI/2])
  if(Number(tangent.points[i-1].x)<pole&&Number(tangent.points[i].x)>pole)expect(tangent.points[i].connect).toBe(false);
 const step=await sample('floor(x)');expect(step.ok).toBe(true);
 for(let i=1;i<step.points.length;i++)if(step.points[i].connect)expect(step.points[i].y).toBe(step.points[i-1].y);
 const logarithm=await sample('ln(x)');expect(logarithm.ok).toBe(true);
 for(let i=1;i<logarithm.points.length;i++)if(logarithm.points[i].connect){expect(logarithm.points[i].kind).toBe('real');expect(logarithm.points[i-1].kind).toBe('real');}
});
