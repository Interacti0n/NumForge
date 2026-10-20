// Serial diagnostic recording; generate Python --workspace --stress fixtures first.
const fs = require('node:fs'), path = require('node:path'), crypto = require('node:crypto');
const os = require('node:os'), assert = require('node:assert/strict');
const {spawnSync} = require('node:child_process');
if (process.argv.length !== 6)
    throw Error('Usage: node run_factorial_stress.cjs calculator-limit-exe bigint-exe fixtures.tsv empty-output-directory');
const [calculator, bigint, fixture, output] = process.argv.slice(2).map(x => path.resolve(x));
fs.mkdirSync(output, {recursive:true});
assert.equal(fs.readdirSync(output).length, 0, 'Recording directory must be empty');
const source = fs.readFileSync(fixture, 'utf8');
const cases = source.trim().split(/\r?\n/).filter(x => x.startsWith('factorial-') && x.includes('-reuse\t')).map(row => {
    const fields = row.split('\t'), n = Number(fields[3]);
    // Complete independent integer conversion and half-even rounding before timing.
    const digits = BigInt('0x' + fields[5].slice(4)).toString();
    let prefix = BigInt(digits.slice(0, 11)), exponent = digits.length - 1;
    const guard = Number(digits[11]);
    if (guard > 5 || guard === 5 && (/[1-9]/.test(digits.slice(12)) || prefix % 2n)) ++prefix;
    let front = String(prefix);
    if (front.length > 11) { front = front.slice(0, -1); ++exponent; }
    const tail = front.slice(1).replace(/0+$/, '');
    const expected = front[0] + (tail ? '.' + tail : '') + 'E+' + exponent;
    fs.writeFileSync(path.join(output, `${n}.expected`), expected);
    const probe = spawnSync(bigint, ['--factorial-plan', String(n)],
        {encoding:'utf8', windowsHide:true, timeout:10000});
    assert.equal(probe.status, 0, probe.stderr);
    const plan = JSON.parse(probe.stdout); assert.equal(plan.representable, true);
    return {n, digits:digits.length, expected, treePlanBytes:plan.requested_bytes,
        treePlanLargest:plan.largest_allocation, planFitsFreshBudget:plan.admitted};
});
assert(cases.length > 0);
const hash = x => crypto.createHash('sha256').update(x).digest('hex');
const records = {started:new Date().toISOString(), node:process.version,
    conditions:'Serial cold processes; no concurrent builds/tests, no affinity or frequency lock.',
    cpu:os.cpus()[0].model, platform:process.platform, arch:process.arch,
    calculatorHash:hash(fs.readFileSync(calculator)), bigintHash:hash(fs.readFileSync(bigint)),
    fixtureHash:hash(source), cases, rows:[], raw:[]};
fs.writeFileSync(path.join(output, 'cases.tsv'), source);
function persist() { fs.writeFileSync(path.join(output, 'results.json'), JSON.stringify(records, null, 2)); }
function measureCalculator(c, run, phases) {
    const env = {...process.env}; delete env.NUMFORGE_BENCH_EXPECTED;
    env.NUMFORGE_BENCH_EXPECTED_FILE = path.join(output, `${c.n}.expected`);
    const result = spawnSync(calculator, [c.n + '!', '10', '2', '5000', ...(phases ? ['--phases'] : [])],
        {env, encoding:'utf8', windowsHide:true, timeout:20000});
    if (result.error) throw result.error;
    const lines = result.stdout.trim().split(/\r?\n/);
    assert.equal(lines.length, 2, result.stderr);
    const keys = lines[0].split(','), values = lines[1].split(',');
    assert.equal(keys.length, values.length);
    const row = {n:c.n, run, phases, exit:result.status, ...Object.fromEntries(keys.map((k,i) => [k, values[i]]))};
    if (row.status_code === '0') assert.equal(result.status, 0, 'Independent rounded reference mismatch');
    else assert(['time','allocation'].includes(row.budget_reason), JSON.stringify(row));
    for (const key of ['elapsed_ms', ...(phases ? ['evaluate_ms','format_ms'] : [])])
        assert(Number.isFinite(Number(row[key])) && Number(row[key]) >= 0);
    records.rows.push(row); persist();
    console.log(`${phases ? 'phases' : 'production'} ${c.n}: ${row.status} ${row.elapsed_ms} ms (${row.budget_reason})`);
}
// Ordinary production pipeline once; phase pipeline three serial cold processes.
for (const c of cases) measureCalculator(c, 0, false);
for (let run = 1; run <= 3; ++run)
    for (const c of run === 2 ? [...cases].reverse() : cases) measureCalculator(c, run, true);
// Isolate budgeted BigInt calculation, comparing every successful entire integer.
for (const c of cases) for (const operation of ['factorial','tree_split']) {
    const name = `${operation}-${c.n}-reuse`;
    const result = spawnSync(bigint, [fixture, '--budget', '--case', name],
        {encoding:'utf8', windowsHide:true, timeout:30000, maxBuffer:1024*1024});
    if (result.error) throw result.error;
    if (result.status !== 0) assert.match(result.stderr, /Budget rejection:.*reason=[12]/);
    records.raw.push({n:c.n, operation, exit:result.status, stdout:result.stdout, stderr:result.stderr});
    persist(); console.log(`BigInt ${name}: ${result.status === 0 ? 'exact match' : result.stderr.trim()}`);
}
console.log('Factorial stress recording complete; successful outputs independently checked.');
