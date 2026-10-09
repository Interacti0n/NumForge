'use strict';
const fs = require('node:fs');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const assert = require('node:assert/strict');
const rows = fs.readFileSync(path.join(__dirname, 'complex_references.tsv'), 'utf8')
  .trim().split(/\r?\n/).filter(x => !x.startsWith('#')).map(x => x.split('\t'));
assert.equal(rows.length,744,'Incomplete reference corpus');
for (const row of rows) {
  assert.equal(row.length,7);
  assert.ok([12,34,100,250].includes(Number(row[3])));
  assert.ok(/^[0-5]$/.test(row[4]));
}
// Exact BigInt comparison, including tiny nonzero components; no double oracle.
function decimal(s) {
  const m = /^([+-]?)(\d+)(?:\.(\d*))?(?:e([+-]?\d+))?$/i.exec(s);
  assert.ok(m, `Invalid decimal ${s}`);
  return {n: BigInt((m[1] === '-' ? '-' : '') + m[2] + (m[3] || '')),
    e: Number(m[4] || 0) - (m[3] || '').length};
}
const abs = n => n < 0n ? -n : n;
function close(actual, expected, digits) {
  const a = decimal(actual), b = decimal(expected);
  const order = b.n === 0n ? 0 : abs(b.n).toString().length - 1 + b.e;
  const toleranceExponent = order - digits + 1;
  const e = Math.min(a.e, b.e, toleranceExponent);
  const scale = (n, power) => n * 10n ** BigInt(power - e);
  return abs(scale(a.n,a.e) - scale(b.n,b.e)) <= scale(2n,toleranceExponent);
}
assert.equal(close('0','1e-100',34),false);
assert.equal(close('1.0001','1',12),false);
for (const input of ['unknown 1 1 34 5\n', 'sqrt broken 1 34 5\n', 'sqrt 1 1 0 5\n', 'sqrt 1']) {
  const rejected = spawnSync(process.argv[2], [], {input, encoding:'utf8', windowsHide:true, timeout:10000});
  assert.ifError(rejected.error); assert.notEqual(rejected.status,0,'Invalid protocol accepted');
}
const benchmark = process.argv.includes('--benchmark');
const samples = benchmark ? 5 : 1;
const timing = new Map(); let failures = 0;
for (let sample = 0; sample < samples; sample++) {
  const result = spawnSync(process.argv[2], [], {input: rows.map(r => r.slice(0,5).join(' ')).join('\n')+'\n',
    encoding:'utf8', windowsHide:true, timeout:120000, maxBuffer:8*1024*1024});
  assert.ifError(result.error); assert.equal(result.status,0,result.stderr);
  const output = result.stdout.trim().split(/\r?\n/).map(x=>x.split('\t'));
  assert.equal(output.length,rows.length);
  rows.forEach((r,j)=> {
    const [real,imag,seconds] = output[j];
    assert.ok(Number.isFinite(Number(seconds)) && Number(seconds)>=0);
    if (!close(real,r[5],Number(r[3])) || !close(imag,r[6],Number(r[3]))) {
      failures++;
      if (sample === 0) console.error(`${r.slice(0,5).join(' ')}: got ${real}, ${imag}; expected ${r[5]}, ${r[6]}`);
    }
    const key = `${r[0]}:${r[3]}:${j}`;
    if (!timing.has(key)) timing.set(key,[]);
    timing.get(key).push(Number(seconds)*1000);
  });
}
if (failures) throw new Error(`${failures}/${rows.length} complex cases outside error bound`);
console.log(`${rows.length} independent complex cases passed`);
if (benchmark) {
  console.log('operation,digits,case_index,re,im,rounding,samples,min_ms,median_ms,max_ms');
  for (const [key,values] of timing) {
    values.sort((a,b)=>a-b);
    const [op,digits,index] = key.split(':');
    console.log(`${op},${digits},${index},${rows[index][1]},${rows[index][2]},${rows[index][4]},${values.length},${values[0].toFixed(6)},${values[Math.floor(values.length/2)].toFixed(6)},${values.at(-1).toFixed(6)}`);
  }
}
