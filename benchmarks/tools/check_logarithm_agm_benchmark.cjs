'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const exe = process.argv[2];
const algorithms = process.argv[4] ? [process.argv[4]] : ['agm', 'interval'];
assert(exe, 'Pass the experimental benchmark executable');
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'numforge-ln-agm-'));
function run(args) {
  const result = spawnSync(exe, args.map(String), { encoding:'utf8', timeout:60000 });
  assert.equal(result.error, undefined);
  assert.equal(result.status, 0, result.stderr);
  const [header, line] = result.stdout.trim().split(/\r?\n/);
  assert(header && line, result.stdout);
  const keys = header.split(','), values = line.split(',');
  assert.equal(keys.length, values.length);
  return Object.fromEntries(keys.map((key,i) => [key,values[i]]));
}
try {
  const fixture = process.argv[3] || path.join(__dirname,'../references/logarithm_agm_smoke.tsv');
  const rows = fs.readFileSync(fixture,'utf8').trim().split(/\r?\n/).slice(1);
  let checks = 0;
  for (let index=0; index<rows.length; index++) {
    const [input,digits,rounding,reference,restriction] = rows[index].split('\t');
    const file = path.join(directory,'reference.txt');
    fs.writeFileSync(file,reference);
    for (const algorithm of algorithms) for (const extra of [80,160]) {
      if (restriction === 'interval-only' && algorithm !== 'interval') continue;
      if (restriction === 'adaptive' && (algorithm === 'agm' || (algorithm === 'public' && Number(digits)<2000))) continue;
      const optional = index % 3 === 0 ? ['--alias'] : index % 3 === 1 ? ['--memory'] : [];
      const row = run([algorithm,input,digits,extra,rounding,60000,file,...optional]);
      assert.equal(row.status,'0', JSON.stringify(row));
      assert.equal(row.reference_match,'1', JSON.stringify(row));
      assert.equal(row.budget_reason,'none');
      assert(Number(row.elapsed_ms)>=0 && Number(row.alloc_calls)>0);
      assert(Number(row.format_ms)>=0);
      if (restriction && algorithm === 'interval' && Number(rounding)>=4) assert(Number(row.attempts)>=2, JSON.stringify(row));
      if (optional[0]==='--memory') assert(Number(row.peak_live)>0);
      checks++;
    }
  }
  for (const algorithm of algorithms) for (const optional of [[],['--alias'],['--memory']]) {
    const row = run([algorithm,'2',5000,80,5,1,'-',...optional]);
    assert.equal(row.status,'2');
    assert.equal(row.budget_reason,'time');
    assert.equal(row.output_bytes,'0');
  }
  for (const algorithm of algorithms) for (const input of ['0','-1']) {
    const row = run([algorithm,input,34,80,5,5000,'-','--alias']);
    assert.equal(row.status,'3');
    assert.equal(row.budget_reason,'none');
  }
  const invalid = spawnSync(exe,['unknown','2','34','80','5','5000','-'],{encoding:'utf8'});
  assert.equal(invalid.status,1);
  console.log(`${checks} exact logarithm references, deadlines, domains and aliases passed`);
} finally {
  fs.rmSync(directory,{recursive:true,force:true});
}
