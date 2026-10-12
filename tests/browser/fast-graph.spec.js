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
