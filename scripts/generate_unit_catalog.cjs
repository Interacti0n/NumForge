/* Maintainer-only catalogue generator. Uses BigInt arithmetic, no floats.
 * Run: node scripts/generate_unit_catalog.cjs [--check]
 * Generated C data are consumed directly; Python/Node are not runtime dependencies. */
const fs = require('fs');
const path = require('path');
const root = path.resolve(__dirname, '..');
const sources = {
    si: 'https://www.nist.gov/pml/special-publication-330/sp-330-section-3',
    accepted: 'https://www.nist.gov/pml/special-publication-330/sp-330-section-4',
    imperial: 'https://www.legislation.gov.uk/ukpga/1985/72/schedule/1',
    us: 'https://www.nist.gov/system/files/documents/2023/01/30/appc-23-HB44.pdf',
    nautical: 'https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8',
    temperature: 'https://www.nist.gov/pml/owm/si-units-temperature',
    binary: 'https://physics.nist.gov/cuu/Units/binary.html',
    angles: 'https://www.nist.gov/system/files/documents/pml/wmd/metric/EU_Metric_Directive_20102.pdf'
};
const rows = [];
function gcd(a, b) { while (b !== 0n) [a,b] = [b,a%b]; return a; }
function ratio(n, d=1n) { n=BigInt(n); d=BigInt(d); const g=gcd(n<0n?-n:n,d); return [n/g,d/g]; }
function mul(a,b) { return ratio(a[0]*b[0],a[1]*b[1]); }
function decimal(s) { const [whole,frac=''] = s.split('.'); return ratio(BigInt(whole+frac),10n**BigInt(frac.length)); }
function pow10(exponent) { return exponent >= 0 ? [10n**BigInt(exponent),1n] : [1n,10n**BigInt(-exponent)]; }
function add(id,symbol,q,scale,en,sk,source,derivation,offset=[0n,1n],pi=0) {
    if (rows.some(r=>r.id===id)) throw Error('Duplicate ID '+id);
    rows.push({id,symbol,q,scale:ratio(...scale),offset:ratio(...offset),en,sk,source,derivation,pi});
}
const prefixes = [
    ['', '', '', 0], ['q','quecto','quecto',-30], ['r','ronto','ronto',-27],
    ['y','yocto','yocto',-24], ['z','zepto','zepto',-21], ['a','atto','atto',-18],
    ['f','femto','femto',-15], ['p','pico','piko',-12], ['n','nano','nano',-9],
    ['u','micro','mikro',-6], ['m','milli','mili',-3], ['c','centi','centi',-2],
    ['d','deci','deci',-1], ['da','deca','deka',1], ['h','hecto','hekto',2],
    ['k','kilo','kilo',3], ['M','mega','mega',6], ['G','giga','giga',9],
    ['T','tera','tera',12], ['P','peta','peta',15], ['E','exa','exa',18],
    ['Z','zetta','zetta',21], ['Y','yotta','yotta',24], ['R','ronna','ronna',27],
    ['Q','quetta','quetta',30]
];
for (const [p,en,sk,e] of prefixes) {
    const symbol=p==='u'?'µ':p;
    for (const [suffix,q,power,shift,english,slovak] of [
        ['m','LENGTH',1,0,'metre','meter'], ['m2','AREA',2,0,'metre','meter'],
        ['m3','VOLUME',3,0,'metre','meter'], ['g','MASS',1,-3,'gram','gram'],
        ['s','TIME',1,0,'second','sekunda'], ['L','VOLUME',1,-3,'litre','liter']
    ]) {
        const modifier=power===2?'square ':power===3?'cubic ':'';
        const modifierSk=power===2?'štvorcový ':power===3?'kubický ':'';
        const sym=suffix==='m2'?'m²':suffix==='m3'?'m³':suffix;
        add(p+suffix,symbol+sym,q,pow10(e*power+shift),modifier+en+english,
            modifierSk+sk+slovak,'si', '10^('+e+' * '+power+' + '+shift+') of canonical unit; '+(suffix==='L'?'litre = 10^-3 m3':suffix==='g'?'gram = 10^-3 kg':'SI prefix'));
    }
}
add('min','min','TIME',[60n,1n],'minute','minúta','accepted','60 seconds');
add('h','h','TIME',[3600n,1n],'hour','hodina','accepted','60 minutes');
add('day','d','TIME',[86400n,1n],'day (fixed duration)','deň (pevné trvanie)','accepted','24 hours');
add('t','t','MASS',[1000n,1n],'tonne','tona','accepted','1000 kilograms');
add('ha','ha','AREA',[10000n,1n],'hectare','hektár','accepted','10000 square metres');
const inch=decimal('0.0254'), foot=decimal('0.3048'), yard=decimal('0.9144'), mile=decimal('1609.344');
for (const [id,en,sk,factor] of [['in','inch','palec',inch],['ft','international foot','medzinárodná stopa',foot],['yd','yard','yard',yard],['mi','international mile','medzinárodná míľa',mile]]) {
    add(id,id,'LENGTH',factor,en,sk,'imperial','yard = 0.9144 m; inch = yard/36; foot = yard/3; mile = 1760 yards');
    for (const power of [2,3]) {
        let scale=[1n,1n]; for(let i=0;i<power;i++) scale=mul(scale,factor);
        add(id+power,id+(power===2?'²':'³'),power===2?'AREA':'VOLUME',scale,(power===2?'square ':'cubic ')+en,(id==='ft'||id==='mi'?(power===2?'štvorcová ':'kubická '):(power===2?'štvorcový ':'kubický '))+sk,'imperial','exact '+id+' factor raised to '+power);
    }
}
add('acre','ac','AREA',mul([4840n,1n],mul(yard,yard)),'acre','aker','imperial','4840 square yards');
const pound=decimal('0.45359237');
add('lb','lb','MASS',pound,'pound (avoirdupois)','libra (avoirdupois)','imperial','0.45359237 kg');
add('oz','oz','MASS',mul(pound,[1n,16n]),'ounce (avoirdupois)','unca (avoirdupois)','imperial','1/16 avoirdupois pound');
const gallons=[['US',mul([231n,1n],mul(inch,mul(inch,inch))),'us','231 cubic inches'],['UK',decimal('0.00454609'),'imperial','4.54609 litres']];
for(const [region,gallon,source,derivation] of gallons) {
    const variants=region==='US'?[['gal',1n],['qt',4n],['pt',8n],['floz',128n]]:[['gal',1n],['qt',4n],['pt',8n],['floz',160n]];
    for(const [id,divisor] of variants) {
        const names={gal:['gallon','galón'],qt:['quart','kvart'],pt:['pint','pinta'],floz:['fluid ounce','tekutá unca']};
        add(id+'_'+region,id+' ('+region+')','VOLUME',mul(gallon,[1n,divisor]),names[id][0]+' ('+region+')',names[id][1]+' ('+region+')',source,derivation+' / '+divisor);
    }
}
add('nmi','nmi','LENGTH',[1852n,1n],'nautical mile','námorná míľa','nautical','1852 metres');
add('kn','kn','SPEED',[463n,900n],'knot','uzol','nautical','1852 metres / 3600 seconds');
add('mph','mph','SPEED',mul(mile,[1n,3600n]),'mile per hour','míľa za hodinu','imperial','international mile / 3600 seconds');
add('m/s','m/s','SPEED',[1n,1n],'metre per second','meter za sekundu','si','metre / second');
add('km/h','km/h','SPEED',[5n,18n],'kilometre per hour','kilometer za hodinu','accepted','1000 metres / 3600 seconds');
for (const [id,symbol,scale,offset,en,sk,q] of [
 ['K','K',[1n,1n],[0n,1n],'kelvin','kelvin','TEMPERATURE'],
 ['degC','°C',[1n,1n],[27315n,100n],'degree Celsius','stupeň Celzia','TEMPERATURE'],
 ['degF','°F',[5n,9n],[45967n,180n],'degree Fahrenheit','stupeň Fahrenheita','TEMPERATURE'],
 ['deltaK','ΔK',[1n,1n],[0n,1n],'kelvin interval','rozdiel teplôt v kelvinoch','TEMPERATURE_INTERVAL'],
 ['deltaC','Δ°C',[1n,1n],[0n,1n],'Celsius interval','rozdiel teplôt v stupňoch Celzia','TEMPERATURE_INTERVAL'],
 ['deltaF','Δ°F',[5n,9n],[0n,1n],'Fahrenheit interval','rozdiel teplôt v stupňoch Fahrenheita','TEMPERATURE_INTERVAL']
]) add(id,symbol,q,scale,en,sk,'temperature','K = C + 273.15; K = (F + 459.67) * 5/9; interval has no offset',offset);
for(const [id,en,sk,bits] of [['bit','bit','bit',1n],['B','byte','bajt',8n]]) {
    add(id,id,'INFORMATION',[bits,1n],en,sk,'binary','byte = 8 bits');
    for(const [p,english,slovak,e] of prefixes.filter(p=>p[3]>=3))
        add(p+id,p+id,'INFORMATION',[bits*10n**BigInt(e),1n],english+en,slovak+sk,'si','10^'+e+' * '+bits+' bits; byte = 8 bits');
    for(const [i,p] of ['Ki','Mi','Gi','Ti','Pi','Ei','Zi','Yi'].entries()) {
        const names=['kibi','mebi','gibi','tebi','pebi','exbi','zebi','yobi'];
        add(p+id,p+id,'INFORMATION',[bits*2n**BigInt(10*(i+1)),1n],names[i]+en,names[i]+sk,'si','2^'+10*(i+1)+' * '+bits+' bits; byte = 8 bits');
    }
}
for(const [id,symbol,n,d,pi,en,sk,why] of [
 ['rad','rad',1n,1n,0,'radian','radián','canonical radian'],
 ['deg','°',1n,180n,1,'degree','stupeň','pi/180 radians'],
 ['arcmin','′',1n,10800n,1,'arcminute','uhlová minúta','degree/60'],
 ['arcsec','″',1n,648000n,1,'arcsecond','uhlová sekunda','degree/3600'],
 ['turn','turn',2n,1n,1,'turn','otáčka','360 degrees = 2*pi radians'],
 ['gon','gon',1n,200n,1,'gon (gradian)','gon (grad)','400 gon = full turn']
]) add(id,symbol,'ANGLE',[n,d],en,sk,id==='gon'||id==='turn'?'angles':'accepted',why,[0n,1n],pi);
const quote=s=>JSON.stringify(s);
const inc='/* Generated by scripts/generate_unit_catalog.cjs; do not edit. */\n'+rows.map(r=>
    '    { { '+[quote(r.id),quote(r.symbol),'NUMFORGE_UNIT_'+r.q,quote(r.en),quote(r.sk),quote(sources[r.source]),r.pi].join(', ')+' }, '+[...r.scale,...r.offset].map(x=>quote(String(x))).join(', ')+' },').join('\n')+'\n';
let doc='# Unit catalogue and factor provenance\n\nGenerated by scripts/generate_unit_catalog.cjs; '+rows.length+' entries.\nCanonical units: metre, square/cubic metre, kilogram, second, metre/second,\nkelvin (point/interval), bit and radian. Factors and offsets below are exact;\nangle factors retain symbolic pi. Local names are display labels, not input aliases.\n\n';
for(const [key,url] of Object.entries(sources)) doc+='- ['+key+']('+url+')\n';
doc+='\n| ID | English | Slovak | Quantity | Scale | Offset | Source / derivation |\n| --- | --- | --- | --- | --- | --- | --- |\n';
for(const r of rows) doc+='| '+[r.id,r.en,r.sk,r.q,r.scale.join('/')+(r.pi?' * pi':''),r.offset.join('/'),'['+r.source+']('+sources[r.source]+'): '+r.derivation].join(' | ')+' |\n';
let changed=false;
for(const [file,content] of [['src/units/unit_catalog.inc',inc],['docs/reference/UNIT_CATALOG.md',doc]]) {
 const destination=path.join(root,file);
 if(process.argv.includes('--check')) { if(!fs.existsSync(destination)||fs.readFileSync(destination,'utf8')!==content) { console.error('Out of date: '+file); changed=true; } }
 else fs.writeFileSync(destination,content);
}
if(changed) process.exitCode=1;
else console.log(rows.length+' catalogue entries verified/generated.');
