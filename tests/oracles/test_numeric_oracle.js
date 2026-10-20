// Deterministic exact-rational reference; no NumForge arithmetic is used here.
const {spawnSync} = require('child_process');
const fs = require('node:fs');
const path = require('node:path');
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

// Exact magnitude length at decimal/binary boundaries, including both signs.
for (const k of [...Array.from({length:256}, (_, i) => i + 1), 500, 2000, 10000, 19000]) {
  for (const n of [pow(k)-1n, pow(k), pow(k)+1n]) for (const sign of [1n,-1n])
    add('idigits',sign*n,0,0,0,n.toString().length);
}
add('idigits',0,0,0,0,1);
for (let bits=1; bits<=1024; bits++) {
  for (const n of [1n<<BigInt(bits), (1n<<BigInt(bits))-1n])
    add('idigits',n,0,0,0,n.toString().length);
}
// Guard digit rounding and carries across powers of ten, all rounding modes.
for (const k of [1,2,18,19,20,38,39,64,100,500,2000])
  for (const n of [pow(k)+5n, pow(k+1)-5n]) for (const sign of [1n,-1n])
    for (let mode=0; mode<6; mode++) {
      const input=sign*n, precision=k, discarded=abs(input).toString().length-precision;
      add('droundsig',input,0,precision,mode,decimal(round(input,pow(discarded),mode),-discarded));
    }
// Mantissa alignment in either direction, equal and near-equal mantissas.
for (const k of [1,19,20,38,39,100,500,2000]) {
  for (const [a,b] of [[pow(k)-1n,1n],[1n,pow(k)+1n],[pow(k)+1n,10n],
                        [pow(k)-1n,pow(k)+1n],[pow(k)+1n,pow(k)-1n]])
    for (const sign of [1n,-1n]) for (let mode=0; mode<6; mode++) {
      let n=a, d=b, exponent=0;
      while(n>=d*10n){d*=10n;exponent++;}
      while(n<d){n*=10n;exponent--;}
      const scale=18-exponent;
      const numerator=scale>=0?sign*a*pow(scale):sign*a;
      const denominator=scale<0?b*pow(-scale):b;
      add('dsig',sign*a,b,19,mode,decimal(round(numerator,denominator,mode),scale));
    }
}

// Independent standard BigInt base conversion, including signs and chunk edges.
for (let base=2; base<=36; base++) {
  for (const n of [0n, -255n, 1n<<64n, (1n<<1024n)+123456789n]) {
    add('ibase',n,0,base,0,n.toString(base));
    add('ibase',n,0,base,1,n.toString(base).toUpperCase());
    add('iparse',n.toString(base),0,base,0,n);
    add('iparse',n.toString(base).toUpperCase(),0,base,0,n);
  }
}
function scientific(n, places, mode, shift=0) {
  let length=abs(n).toString().length, exponent=length-1+shift;
  let coefficient=round(n,10n**BigInt(length-places-1),mode);
  let text=abs(coefficient).toString();
  if(text.length>places+1) exponent++;
  text=text.replace(/0+$/,'');
  return (coefficient<0n?'-':'')+text[0]+(text.length>1?'.'+text.slice(1):'')+'E'+(exponent<0?'-':'+')+Math.abs(exponent);
}
// Large coefficients exercise the bounded-prefix path and ambiguous sticky tails.
for (let i=0;i<8;i++) {
  let text=String(1+rnd(9));
  for(let j=1;j<(i<4 ? 2000+i*200 : 12000+(i-4)*1000);j++) text+=rnd(10);
  text=text.slice(0,-1)+'1';
  const n=BigInt((i%2?'-':'')+text);
  for(const places of [0,10,100,128,129]) for(let mode=0;mode<6;mode++)
    for(const shift of [0,-10000]) add('dfmt',`${n}e${shift}`,0,places,mode,scientific(n,places,mode,shift));
}
for (const text of ['123456789005'+'0'.repeat(1987)+'1','999999999995'+'0'.repeat(1987)+'1']) {
  for(const sign of [1n,-1n]) for(let mode=0;mode<6;mode++) {
    const n=sign*BigInt(text);add('dfmt',n,0,10,mode,scientific(n,10,mode));
  }
}

// Repeated GCD scratch reuse, exact independent JS modulo references. Keep
// operand text below the test driver's 20,000-byte per-token protocol limit.
let fa = 0n, fb = 1n;
for (let i = 0; i < 450; i++) [fa, fb] = [fb, fa + fb];
for (const [a, b] of [[fa, fb], [fa*97n, fb*97n],
    [(1n<<16000n)-1n, 3n**10080n], [(1n<<4096n)+17n, (1n<<2048n)-1n]]) {
  const expected = gcd(a, b);
  for (const signA of [1n, -1n]) for (const signB of [1n, -1n])
    add('igcd', signA*a, signB*b, 0, 0, expected);
  add('igcd', b, a, 0, 0, expected);
}

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
// Exercise one persistent workspace through changing precision/cache/operation.
for (const c of [...cases]) {
  if (c.input.startsWith('dsig ')) cases.push({...c, input: c.input.replace(/^dsig /, 'dwdiv ')});
  if (c.input.startsWith('droundsig ')) cases.push({...c, input: c.input.replace(/^droundsig /, 'dwround ')});
}
function roundedSignificant(n, scale, digits, mode) {
  if (!n) return '0';
  const discarded = Math.max(0, abs(n).toString().length - digits);
  return decimal(round(n, pow(discarded), mode), scale - discarded);
}
for (const [a,b,sa,sb] of [[123456789555n,37n,3,0],[-123456789555n,37n,3,0],
    [999999999999n,-999999999998n,12,12],[25n,4n,-10,10],
    [0n,-37n,0,3],[5n,2n,18,-3],[-5n,2n,18,-3],
    [12500n,2n,4,1],[(1n<<1024n)+7n,3n,0,19]]) {
  for(const digits of [1,2,9,34]) for(let mode=0;mode<6;mode++) {
    const scale=Math.max(sa,sb),sum=a*pow(scale-sa)+b*pow(scale-sb);
    add('dwadd',`${a}e${-sa}`,`${b}e${-sb}`,digits,mode,roundedSignificant(sum,scale,digits,mode));
    add('dwmul',`${a}e${-sa}`,`${b}e${-sb}`,digits,mode,roundedSignificant(a*b,sa+sb,digits,mode));
  }
}

// Frozen high-precision references are generated independently by mpmath.
const referenceFile = path.join(__dirname, '../fixtures/transcendental_references.tsv');
let referenceCases = 0;
for (const line of fs.readFileSync(referenceFile, 'utf8').split(/\r?\n/)) {
  if (!line || line.startsWith('#')) continue;
  const fields = line.split('\t');
  if (fields.length !== 6) throw Error(`Malformed reference row: ${line}`);
  const [op, a, b, digits, mode, expected] = fields;
  cases.push({input:`${op} ${a} ${b} ${digits} ${mode}`, expected, reference:true});
  referenceCases++;
}
if (referenceCases !== 319) throw Error(`Expected 319 high-precision references, got ${referenceCases}`);
function decimalParts(value) {
  const match = /^(-?)(\d+)(?:\.(\d+))?$/.exec(value);
  if (!match) return null;
  return {coefficient: BigInt((match[1] || '') + match[2] + (match[3] || '')),
    scale: (match[3] || '').length};
}
function withinOneUlp(expected, actual, digits) {
  const reference = decimalParts(expected), observed = decimalParts(actual);
  if (!reference || !observed || reference.coefficient === 0n) return false;
  const magnitude = abs(reference.coefficient).toString().length - reference.scale - 1;
  const ulpExponent = magnitude - digits + 1;
  const scale = Math.max(reference.scale, observed.scale, -ulpExponent, 0);
  const difference = abs(reference.coefficient * pow(scale - reference.scale) -
    observed.coefficient * pow(scale - observed.scale));
  return difference <= pow(scale + ulpExponent);
}
const result=spawnSync(process.argv[2],{input:cases.map(c=>c.input).join('\n')+'\n',encoding:'utf8',timeout:110000,windowsHide:true,maxBuffer:8*1024*1024});
if(result.error)throw result.error;
if(result.status!==0)throw Error(`Exit ${result.status}: ${result.stderr}`);
const actual=result.stdout.trim().split(/\r?\n/);
let failures=0;
let boundedDirected=0;
cases.forEach((c,i)=>{
  if(actual[i]===c.expected)return;
  const fields=c.input.split(' '), digits=Number(fields[3]), mode=Number(fields[4]);
  if(c.reference && mode<4 && withinOneUlp(c.expected,actual[i],digits)) {boundedDirected++;return;}
  if(failures<(process.env.NUMFORGE_ORACLE_VERBOSE ? 100 : 8))
    console.log({case:c.input,expected:c.expected,actual:actual[i]});
  failures++;
});
if(actual.length!==cases.length)throw Error(`Expected ${cases.length} outputs; got ${actual.length}`);
console.log(JSON.stringify({seed:'0x12345678',cases:cases.length,boundedDirected,failures}));
process.exitCode=failures?1:0;
