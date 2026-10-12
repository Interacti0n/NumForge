const {test,expect}=require('@playwright/test');
const {randomBytes}=require('node:crypto');
test('adaptive harder functions, broad ranges and oscillation safeguards',async({request})=>{
 test.setTimeout(30000);
 const sample=async(expression,min,max,precision='34')=>{
  const response=await request.post('/api/graph/plot?'+new URLSearchParams({min,max,precision}),{data:expression});
  expect(response.ok()).toBe(true);const data=await response.json();expect(data.ok).toBe(true);
  expect(data.count).toBeLessThanOrEqual(1024);return data;
 };
 const sine=await sample('sin(3x)','-10','10');expect(sine.limited).toBe(false);
 expect(sine.points.slice(1).every(p=>p.connect)).toBe(true);
 for(const [expression,reference] of [
  ['sin(3x)',x=>Math.sin(3*x)],['sin(3x)+cos(2x)',x=>Math.sin(3*x)+Math.cos(2*x)],
  ['sin(3x)*exp(-x*x/20)',x=>Math.sin(3*x)*Math.exp(-x*x/20)]]){
  const data=await sample(expression,'-10','10');let edges=0;expect(data.limited).toBe(false);
  const span=Number(data.y_max)-Number(data.y_min);
  data.points.forEach((p,i)=>{
   const x=Number(p.x),y=Number(p.y);expect(Math.abs(y-reference(x))).toBeLessThan(1e-10);
   if(p.connect){++edges;const a=data.points[i-1],ax=Number(a.x),ay=Number(a.y);
    for(const t of [.25,.5,.75])expect(Math.abs(ay+(y-ay)*t-reference(ax+(x-ax)*t))/span).toBeLessThan(.0011);
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
