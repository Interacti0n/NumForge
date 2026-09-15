// Deterministic exact-rational reference; no NumForge arithmetic is used here.
const {spawnSync} = require('child_process');
if (!process.argv[2]) throw Error('Usage: node test_numeric_oracle.js <driver>');
let seed = 0x12345678;
function rnd(n) { seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0; return seed % n; }
function integer() {
  let s=String(1+rnd(9));
  for(let i=1,n=1+rnd(140);i<n;i++) s+=rnd(10);
  return BigInt((rnd(2)?'-':'')+s);
}
const pow = s => 10n ** BigInt(s);
const abs = n => n<0n?-n:n;
function gcd(a,b) {a=abs(a);b=abs(b);while(b){[a,b]=[b,a%b];}return a;}
function decimal(c,s) {
  if(c===0n)return '0';
  const sign=c<0n?'-':'';let t=abs(c).toString();
  while(s>0&&t.endsWith('0')){t=t.slice(0,-1);s--;}
  if(s<=0)return sign+t+'0'.repeat(-s);
  if(t.length<=s)return sign+'0.'+'0'.repeat(s-t.length)+t;
  return sign+t.slice(0,-s)+'.'+t.slice(-s);
}
function round(n,d,mode) {
  const neg=(n<0n)!==(d<0n);n=abs(n);d=abs(d);
  let q=n/d,r=n%d;
  const up=r!==0n&&(mode===1||(mode===2&&neg)||(mode===3&&!neg)||
    (mode===4&&2n*r>=d)||(mode===5&&(2n*r>d||(2n*r===d&&q%2n===1n))));
  if(up)q++; return neg?-q:q;
}
const cases=[];
function add(op,a,b,s,m,expected){cases.push({input:`${op} ${a} ${b} ${s} ${m}`,expected:String(expected)});}
for(let i=0;i<180;i++) {
  const a=integer(),b=integer();
  add('iadd',a,b,0,0,a+b);add('isub',a,b,0,0,a-b);add('imul',a,b,0,0,a*b);
  add('idiv',a,b,0,0,a/b);add('imod',a,b,0,0,a%b);
  if(i<20)add('igcd',a,b,0,0,gcd(a,b));
  if(i<12){const n=BigInt(rnd(7));add('ipow',a,n,0,0,a**n);}
  const sa=rnd(40)-10,sb=rnd(40)-10,s=Math.max(sa,sb);
  const ta=`${a}e${-sa}`,tb=`${b}e${-sb}`;
  add('dadd',ta,tb,0,0,decimal(a*pow(s-sa)+b*pow(s-sb),s));
  add('dsub',ta,tb,0,0,decimal(a*pow(s-sa)-b*pow(s-sb),s));
  add('dmul',ta,tb,0,0,decimal(a*b,sa+sb));
  const target=rnd(28)-7;
  for(let mode=0;mode<6;mode++) {
    let delta=target+sb-sa,n=a,d=b;
    if(delta>=0)n*=pow(delta);else d*=pow(-delta);
    add('ddiv',ta,tb,target,mode,decimal(round(n,d,mode),target));
    const c=target>=sa?a*pow(target-sa):round(a,pow(sa-target),mode);
    add('dscale',ta,tb,target,mode,decimal(c,target));
    const precision=1+rnd(40);
    let sn=a,sd=b;
    if(sb>=sa)sn*=pow(sb-sa);else sd*=pow(sa-sb);
    let magnitudeN=abs(sn),magnitudeD=abs(sd),exponent=0;
    while(magnitudeN>=magnitudeD*10n){magnitudeD*=10n;exponent++;}
    while(magnitudeN<magnitudeD){magnitudeN*=10n;exponent--;}
    const outputScale=precision-1-exponent;
    if(outputScale>=0)sn*=pow(outputScale);else sd*=pow(-outputScale);
    add('dsig',ta,tb,precision,mode,decimal(round(sn,sd,mode),outputScale));
    let reducedDenominator=abs(b)/gcd(a,b),twos=0,fives=0;
    while(reducedDenominator%2n===0n){reducedDenominator/=2n;twos++;}
    while(reducedDenominator%5n===0n){reducedDenominator/=5n;fives++;}
    const exactScale=Math.max(twos,fives);
    const expected=reducedDenominator===1n
      ? decimal(a*pow(exactScale)/b,exactScale+sa-sb)
      : decimal(round(sn,sd,mode),outputScale);
    add('dcalc',ta,tb,precision,mode,expected);
  }
}
// Independent binary-search oracle: no Newton iteration shared with C.
function isqrt(n) {
  let lo=0n,hi=n+1n;
  while(hi-lo>1n){const mid=(lo+hi)/2n;if(mid*mid<=n)lo=mid;else hi=mid;}
  return lo;
}
for(let i=0;i<100;i++) {
  const a=integer(),b=integer(),g=gcd(a,b),n=abs(a);
  add('calc',`gcd(${a};${b})`,0,0,0,g);
  add('calc',`lcm(${a};${b})`,0,0,0,abs(a/g*b));
  add('calc',`mod(${a};${b})`,0,0,0,a%b);
  for(const v of [n,n*n,n*n-1n,n*n+1n])
    add('calc',`isqrt(${v})`,0,0,0,isqrt(v));
}
// Rational inequalities and binary search independently check root rounding,
// including exact midpoint ties, without the C guard/sticky representation.
function rootFloor(n,d,k) {
  let lo=0n,hi=1n;
  while(hi**k*d<=n)hi*=2n;
  while(hi-lo>1n){const m=(lo+hi)/2n;if(m**k*d<=n)lo=m;else hi=m;}
  return lo;
}
function rootReference(c,s,k,p,mode) {
  const neg=c<0n;c=abs(c);const order=BigInt(k),exact=rootFloor(c,1n,order);
  if(exact**order===c && s%k===0)return decimal(neg?-exact:exact,s/k);
  let n=c,d=1n;
  if(s>=0)d=pow(s);else n*=pow(-s);
  let exponent=0;
  if(n>=d){while(pow((exponent+1)*k)*d<=n)exponent++;}
  else {while(n*pow(-exponent*k)<d)exponent--;}
  const scale=p-1-exponent;
  if(scale>=0)n*=pow(scale*k);else d*=pow(-scale*k);
  let q=rootFloor(n,d,order);
  const exactGrid=q**order*d===n;
  const midpoint=(2n*q+1n)**order*d,twice=n*2n**order;
  const up=!exactGrid&&(mode===1||(mode===2&&neg)||(mode===3&&!neg)||
    (mode===4&&twice>=midpoint)||(mode===5&&(twice>midpoint||(twice===midpoint&&q%2n===1n))));
  if(up)q++;
  return decimal(neg?-q:q,scale);
}
for(let i=0;i<80;i++) {
  const k=2+rnd(8),s=rnd(25)-12,p=1+rnd(20);
  let c=abs(integer());if(k%2 && i%2)c=-c;
  for(let mode=0;mode<6;mode++)add('rroot',`${c}e${-s}`,k,p,mode,rootReference(c,s,k,p,mode));
  if(i<40){const r=BigInt(1+rnd(100000)),t=rnd(11)-5;add('rroot',`${r**BigInt(k)}e${-t*k}`,k,1,5,decimal(r,t));}
}
const result=spawnSync(process.argv[2],{input:cases.map(c=>c.input).join('\n')+'\n',encoding:'utf8',timeout:110000,windowsHide:true,maxBuffer:8*1024*1024});
if(result.error)throw result.error;
if(result.status!==0)throw Error(`Exit ${result.status}: ${result.stderr}`);
const actual=result.stdout.trim().split(/\r?\n/);
let failures=0;
cases.forEach((c,i)=>{if(actual[i]!==c.expected){if(failures<8)console.log({case:c.input,expected:c.expected,actual:actual[i]});failures++;}});
if(actual.length!==cases.length)throw Error(`Expected ${cases.length} outputs; got ${actual.length}`);
console.log(JSON.stringify({seed:'0x12345678',cases:cases.length,failures}));
process.exitCode=failures?1:0;
