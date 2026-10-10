'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const [decimal, calculator] = process.argv.slice(2);
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'numforge-audit-'));
const fixture = path.join(directory, 'case.tsv');
function invoke(binary, args, env = {}) {
    const result = spawnSync(binary, args, {encoding: 'utf8', windowsHide: true,
        timeout: 20000, env: {...process.env, ...env}});
    if (result.error) throw result.error;
    return result;
}
function row(result) {
    assert.equal(result.status, 0, result.stderr);
    const lines = result.stdout.trim().split(/\r?\n/);
    assert.equal(lines.length, 2);
    const keys = lines[0].split(','), values = lines[1].split(',');
    assert.equal(keys.length, values.length);
    return Object.fromEntries(keys.map((key, i) => [key, values[i]]));
}
try {
    fs.writeFileSync(fixture, 'sum\tadd\treuse\t12\t13\t0\t5\t0\t0\t25\n');
    for (const options of [['--quick'], ['--validate']]) {
        const measured = row(invoke(decimal, [fixture, '--timing-only', ...options]));
        assert.equal(measured.validation, 'exact');
        assert.equal(+measured.samples, options[0] === '--quick' ? 3 : 1);
        assert.ok(!('peak_live_bytes' in measured), 'Timing-only mode must not fabricate memory metrics');
        for (const key of ['min_ns_per_op', 'median_ns_per_op', 'max_ns_per_op'])
            assert.ok(Number.isFinite(+measured[key]) && +measured[key] >= 0);
    }
    fs.writeFileSync(fixture, 'wrong\tadd\treuse\t12\t13\t0\t5\t0\t0\t26\n');
    const wrong = invoke(decimal, [fixture, '--validate', '--timing-only']);
    assert.equal(wrong.status, 1);
    assert.match(wrong.stderr, /Reference mismatch/);
    const sum = row(invoke(calculator, ['1+2', '10', '0', '5000'], {NUMFORGE_BENCH_EXPECTED: '3'}));
    assert.equal(+sum.status_code, 0);
    assert.equal(+sum.output_bytes, 1);
    assert.ok(+sum.alloc_calls > 0 && +sum.requested_bytes > 0);
    const quantity = row(invoke(calculator, ['qty(1/3;"m")+qty(1/6;"m")', '10', '0', '5000'],
        {NUMFORGE_BENCH_EXPECTED: '0.5 m'}));
    assert.equal(+quantity.status_code, 0);
    fs.writeFileSync(fixture, 'π');
    const constant = row(invoke(calculator, ['--input-file', fixture, '10', '0', '5000'],
        {NUMFORGE_BENCH_EXPECTED: '3.1415926536'}));
    assert.equal(+constant.status_code, 0);
    fs.writeFileSync(fixture, '1'.repeat(4097));
    const oversized = row(invoke(calculator, ['--input-file', fixture, '10', '0', '5000']));
    assert.notEqual(+oversized.status_code, 0);
    assert.equal(+oversized.output_bytes, 0);
    const invalid = row(invoke(calculator, ['1/0', '10', '0', '5000']));
    assert.notEqual(+invalid.status_code, 0);
    assert.equal(+invalid.output_bytes, 0);
    assert.notEqual(invoke(calculator, ['1+2', '10', '0', '-1']).status, 0);
    assert.notEqual(invoke(calculator, ['1+2', '10', '0', '5000'], {NUMFORGE_BENCH_EXPECTED: '4'}).status, 0);
    console.log('Timing-only references and complete-pipeline success/failure diagnostics verified.');
} finally {
    fs.rmSync(directory, {recursive: true, force: true});
}
