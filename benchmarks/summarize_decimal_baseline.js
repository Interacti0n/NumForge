// Read-only aggregation; incomplete collections and inconsistent artifacts fail.
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const directory = path.resolve(process.argv[2] || '');
if (!process.argv[2]) throw Error('Usage: node benchmarks/summarize_decimal_baseline.js baseline-directory');
const manifest = JSON.parse(fs.readFileSync(path.join(directory, 'manifest.json'), 'utf8'));
assert.equal(manifest.suite, 'decimal/math');
assert.equal(manifest.repetitions, 3);
const fixtures = fs.readFileSync(path.join(directory, 'decimal-cases.tsv'), 'utf8').trim().split(/\r?\n/);
assert.equal(fixtures.length, manifest.fixtureCount);
const commands = manifest.commands.filter(entry => entry.name !== 'allocation-check');
assert.equal(commands.length, fixtures.length * 3, 'Collection incomplete');
const seen = new Set(), groups = new Map();
const outcomes = {verified: 0, boundedDirected: 0, referenceMismatch: 0, timedOut: 0};
for (const entry of commands) {
    const key = `${entry.run}:${entry.name}`;
    assert.ok(!seen.has(key), 'Duplicate run/case'); seen.add(key);
    assert.ok([1, 2, 3].includes(entry.run));
    const group = groups.get(entry.name) || {verified: 0, timeout: 0, mismatch: 0, bounded: 0, medians: [], peaks: []};
    groups.set(entry.name, group);
    const prefix = path.join(directory, `decimal-${entry.name}-run${entry.run}`);
    if (entry.status === 'timeout') {
        assert.ok(fs.existsSync(prefix + '.timeout.txt')); assert.ok(!fs.existsSync(prefix + '.csv'));
        group.timeout++; outcomes.timedOut++;
    } else if (entry.status === 'reference_mismatch') {
        assert.ok(fs.existsSync(prefix + '.reference-mismatch.txt')); assert.ok(!fs.existsSync(prefix + '.csv'));
        group.mismatch++; outcomes.referenceMismatch++;
    } else {
        assert.equal(entry.status, 'verified');
        const lines = fs.readFileSync(prefix + '.csv', 'utf8').trim().split(/\r?\n/);
        assert.equal(lines.length, 2);
        const header = lines[0].split(','), fields = lines[1].split(',');
        assert.equal(header.length, fields.length);
        const values = Object.fromEntries(header.map((name, i) => [name, fields[i]]));
        assert.equal(values.case, entry.name);
        for (const name of header.slice(3).filter(name => name !== 'validation'))
            assert.ok(Number.isFinite(+values[name]) && (name === 'parameter' || +values[name] >= 0), entry.name + '/' + name);
        assert.ok(+values.min_ns_per_op <= +values.median_ns_per_op && +values.median_ns_per_op <= +values.max_ns_per_op);
        assert.ok(+values.peak_live_bytes >= +values.live_bytes && +values.peak_live_bytes >= +values.baseline_live_bytes);
        assert.ok(['exact', 'bounded_directed'].includes(values.validation));
        if (values.validation === 'bounded_directed') {
            assert.ok(entry.name.startsWith('legacy_') && +values.rounding < 4);
            group.bounded++; outcomes.boundedDirected++;
        }
        group.verified++; outcomes.verified++;
        group.medians.push(+values.median_ns_per_op); group.peaks.push(+values.peak_live_bytes);
    }
}
assert.deepEqual(outcomes, manifest.outcomes);
assert.equal(groups.size, fixtures.length);
console.error(JSON.stringify({cases: fixtures.length, attempts: commands.length, outcomes}));
console.log('case,verified_runs,timeout_runs,reference_mismatch_runs,bounded_directed_runs,median_min_ns,median_max_ns,peak_min_bytes,peak_max_bytes');
for (const row of fixtures) {
    const name = row.split('\t')[0], group = groups.get(name);
    assert.ok(group && group.verified + group.timeout + group.mismatch === 3);
    const range = values => values.length ? [Math.min(...values), Math.max(...values)] : ['', ''];
    console.log([name, group.verified, group.timeout, group.mismatch, group.bounded,
        ...range(group.medians), ...range(group.peaks)].join(','));
}
