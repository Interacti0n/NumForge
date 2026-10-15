const {test,expect}=require('@playwright/test');
const even=x=>{const n=Math.floor(x),t=x-n;return t<.5?n:t>.5?n+1:n%2===0?n:n+1;};
const cases=[
 ['floor(x)',Math.floor],['ceil(x)',Math.ceil],['trunc(x)',Math.trunc],['round(x)',even],
 ['floor(3*x+0.37)',x=>Math.floor(3*x+.37)],['ceil(-2*x+0.37)',x=>Math.ceil(-2*x+.37)],
 ['floor(x)+x/10',x=>Math.floor(x)+x/10],['0.000000001*floor(x)',x=>1e-9*Math.floor(x)],
 ['floor(x)+ceil(2*x+0.37)',x=>Math.floor(x)+Math.ceil(2*x+.37)],
 ['floor(sin(3*x))',x=>Math.floor(Math.sin(3*x))]
];
test('step metadata prevents accepted chords across shifted, scaled and composed jumps',async({request})=>{
 for(const [expression,f] of cases)for(const precision of [34,35]){
  const span=expression.includes('sin')?10:100;
  const q=new URLSearchParams({mode:'fast',min:-span,max:span,ymin:-10,ymax:10,width:800,height:440,precision});
  const b=await (await request.post('/api/graph/plot?'+q,{data:expression})).json();expect(b.ok,expression).toBe(true);expect(b.evaluation).toBe('precision');
  let checks=0,worst=0;
  for(let i=1;i<b.points.length;i++){const c=b.points[i],a=b.points[i-1];if(!c.connect)continue;
   const ax=Number(a.x),cx=Number(c.x),ay=Number(a.y),cy=Number(c.y);
   for(const t of [.125,.25,.5,.75,.875]){const x=ax+(cx-ax)*t,y=f(x);worst=Math.max(worst,Math.abs(y-(ay*(1-t)+cy*t)));checks++;}
  }
  expect(worst,expression).toBeLessThan(expression.startsWith('0.000000001')?2e-17:2e-8);
  expect(checks,expression).toBeGreaterThan(50);expect(b.count).toBeLessThanOrEqual(2048);
 }
});

test('legacy and screen step samples preserve plateau connections and exact sampled values',async({request})=>{
 for(const mode of ['', 'fast'])for(const [expression,f] of cases.slice(0,4)){
  const q=new URLSearchParams({min:-2,max:2,...(mode?{mode,ymin:-10,ymax:10,width:320,height:300}:{})});
  const b=await (await request.post('/api/graph/plot?'+q,{data:expression})).json();expect(b.ok).toBe(true);let edges=0;
  for(let i=0;i<b.points.length;i++){const p=b.points[i];expect(Number(p.y),expression+' at '+p.x).toBe(f(Number(p.x))||0);
   if(p.connect){expect(p.y).toBe(b.points[i-1].y);edges++;}}
  expect(edges).toBeGreaterThan(20);
 }
});

test('screen step refinement defers off-screen jumps and retains visible plateaus',async({request})=>{
 for(const precision of [34,35])for(const width of [320,800]){
  const q=new URLSearchParams({mode:'fast',precision,min:-100,max:100,ymin:-10,ymax:10,width,height:440});
  const b=await (await request.post('/api/graph/plot?'+q,{data:'floor(3*x+0.37)'})).json();expect(b.ok).toBe(true);expect(b.screen_clipped).toBe(true);
  let covered=0,expected=0,index=0,mismatches=0;
  for(let i=0;i<2049;i++){const x=-3.2+6.4*i/2048,y=Math.floor(3*x+.37);if(y<-10||y>10)continue;expected++;
   while(index<b.points.length&&Number(b.points[index].x)<x)index++;
   const a=b.points[index-1],c=b.points[index];if(!a||!c||!c.connect)continue;
   if(Number(a.y)!==y||Number(c.y)!==y)mismatches++;covered++;
  }
  expect(mismatches).toBe(0);expect(covered/expected).toBeGreaterThan(.98);expect(b.count).toBeLessThan(2048);
 }
});
