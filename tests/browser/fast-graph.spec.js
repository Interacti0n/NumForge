const {test,expect}=require('@playwright/test');
async function plot(request,expression,options={}){
 const response=await request.post('/api/graph/plot?'+new URLSearchParams({mode:'fast',width:'800',height:'440',min:'-10',max:'10',...options}),{data:expression});
 expect(response.ok()).toBe(true);return response.json();
}
test('screen sampler preserves smooth curves and computed periodic extrema',async({request})=>{
 for(const expression of ['sin(x)','sin(33x)','sin(3x)+cos(2x)']){
  const data=await plot(request,expression);
  expect(data.schema_version).toBe(3);expect(data.count).toBeLessThanOrEqual(8192);
  expect(data.limited).toBe(false);expect(data.points.slice(1).every(p=>p[2]===1)).toBe(true);
 }
});
test('screen sampler keeps poles and domain boundaries disconnected',async({request})=>{
 for(const [expression,poles,domain,reference] of [
  ['tan(x)',Array.from({length:6},(_,i)=>(i-3)*Math.PI+Math.PI/2),()=>true,Math.tan],
  ['1/(x-0.1)',[.1],x=>x!==.1,x=>1/(x-.1)],['1/x',[0],x=>x!==0,x=>1/x],
  ['sqrt(x)',[],x=>x>=0,Math.sqrt],['ln(x)',[0],x=>x>0,Math.log]]){
  const data=await plot(request,expression);expect(data.schema_version).toBe(3);
  let error=0;const violations=[];
  data.points.forEach((p,i)=>{
   if(p[3]===0&&p[1]!==null){if(!domain(p[0]))violations.push('domain');error=Math.max(error,Math.abs(p[1]-reference(p[0]))/Math.max(1,Math.abs(reference(p[0]))));}
   if(p[2]){const previous=data.points[i-1];if(p[3]!==0||previous[3]!==0)violations.push('classification');
    for(const pole of poles)if(previous[0]<pole&&p[0]>pole)violations.push('pole');}
  });expect(violations,expression).toEqual([]);expect(error,expression).toBeLessThan(1e-9);
 }
});
test('wide sine extrema and pixel chord accuracy are retained',async({request})=>{
 const data=await plot(request,'sin(x)',{min:'-100',max:'611'});expect(data.limited).toBe(false);
 expect(data.points.slice(1).every(p=>p[2]===1)).toBe(true);
 for(let k=Math.ceil((-100-Math.PI/2)/Math.PI);Math.PI/2+k*Math.PI<611;++k){
  const x=Math.PI/2+k*Math.PI,p=data.points.find(p=>Math.abs(p[0]-x)<1e-7);
  expect(p).toBeTruthy();expect(Math.abs(p[1])).toBeCloseTo(1,9);
 }
 let error=0;
 for(let i=1;i<data.points.length;++i){const a=data.points[i-1],b=data.points[i];
  for(const t of [.25,.5,.75])error=Math.max(error,Math.abs(a[1]+(b[1]-a[1])*t-Math.sin(a[0]+(b[0]-a[0])*t))*440/2);
 }expect(error).toBeLessThan(1.2);
});
test('unsupported and high precision requests retain the precision sampler',async({request})=>{
 for(const [expression,options] of [['floor(x)',{}],['sin(x)',{precision:'100'}],['(10000000000000000+x)-10000000000000000',{}]]){
  const data=await plot(request,expression,options);expect(data.schema_version).toBe(2);expect(data.evaluation).toBe('precision');
 }
 for(const extra of [{width:'319'},{height:'1201'},{quality:'unknown'},{mode:'unknown'}]){
  const response=await request.post('/api/graph/plot?'+new URLSearchParams({mode:'fast',width:'800',height:'440',min:'-10',max:'10',...extra}),{data:'x'});expect(response.ok()).toBe(false);
 }
});

test('camera span controls pixel refinement without changing values or pole gaps',async({request})=>{
 const wide=await plot(request,'sin(x)',{min:'-30',max:'30',width:'2048',ymin:'-10',ymax:'10'});
 const narrow=await plot(request,'sin(x)',{min:'-30',max:'30',width:'2048',ymin:'-1',ymax:'1'});
 expect(wide.limited).toBe(false);expect(wide.count).toBeLessThan(narrow.count);
 expect(wide.points.slice(1).every(p=>p[2]===1)).toBe(true);
 let pixelError=0;
 wide.points.forEach((p,i)=>{
  expect(Math.abs(p[1]-Math.sin(p[0]))).toBeLessThan(1e-9);
  if(i&&p[2]){const a=wide.points[i-1];for(const t of [.25,.5,.75])
   pixelError=Math.max(pixelError,Math.abs(a[1]+(p[1]-a[1])*t-Math.sin(a[0]+(p[0]-a[0])*t))*440/20);}
 });expect(pixelError).toBeLessThan(1.2);
 const poles=await plot(request,'1/x',{ymin:'-10',ymax:'10'});
 poles.points.forEach((p,i)=>{if(p[2])expect(poles.points[i-1][0]<0&&p[0]>0).toBe(false);});
 for(const extra of [{ymin:'-10'},{ymax:'10'},{ymin:'10',ymax:'-10'},{ymin:'0',ymax:'0'},
  {ymin:'nan',ymax:'10'},{ymin:'-1e308',ymax:'1e308'},{ymin:'-10x',ymax:'10'},
  {ymin:'0x1',ymax:'10'},{ymin:' 0',ymax:'10'},{ymin:'0',ymax:'1e'},{ymin:'-10',ymax:'10',mode:''}]){
  const response=await request.post('/api/graph/plot?'+new URLSearchParams({mode:'fast',min:'-10',max:'10',...extra}),{data:'x'});
  expect(response.ok()).toBe(false);
 }
});

test('screen floor connects smooth steep and dense curves within bounded work',async({request})=>{
 for(const expression of ['x^10','exp(x)','(x-1)^10','sin(100x)']){
  const data=await plot(request,expression,{min:'-30',max:'30',width:'2048',ymin:'-10',ymax:'10'});
  expect(data.schema_version,expression).toBe(3);expect(data.count,expression).toBeLessThan(8192);
  expect(data.limited,expression).toBe(false);expect(data.points.slice(1).every(p=>p[2]===1),expression).toBe(true);
  if(expression==='sin(100x)'){
   let index=0;
   for(let k=Math.ceil((-3000-Math.PI/2)/Math.PI);(Math.PI/2+k*Math.PI)/100<30;++k){
    const peak=(Math.PI/2+k*Math.PI)/100;
    while(index<data.points.length&&data.points[index][0]<peak-1e-7)++index;
    expect(data.points[index][0]).toBeCloseTo(peak,7);expect(Math.abs(data.points[index][1])).toBeCloseTo(1,9);
   }
  }
 }
 // Under-resolved whole periods must still remain gaps, even in a smooth AST.
 const unresolved=await plot(request,'sin(1000x)',{min:'-30',max:'30',width:'2048',ymin:'-10',ymax:'10'});
 expect(unresolved.limited).toBe(true);expect(unresolved.points.slice(1).some(p=>p[2]===0)).toBe(true);
 const pole=await plot(request,'1/(x-0.1)^2',{ymin:'-10',ymax:'10'});
 pole.points.forEach((p,i)=>{if(p[2])expect(pole.points[i-1][0]<.1&&p[0]>.1).toBe(false);});
});
