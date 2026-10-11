'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const directory = path.resolve(process.argv[2] || '');
if (process.argv.length !== 3) throw Error('Usage: node benchmarks/summarize_performance_audit.js audit-directory');
const manifest = JSON.parse(fs.readFileSync(path.join(directory, 'manifest.json'), 'utf8'));
assert.ok(manifest.completedAt, 'Collection is incomplete');
function csv(command) {
    const lines = fs.readFileSync(path.join(directory, command.name + '.csv'), 'utf8').trim().split(/\r?\n/);
    // The complex oracle emits a human-readable validation summary first.
    const at = lines.findIndex(line => /^(case|operation|status|sample),/.test(line));
    if (at < 0) return [];
    const keys = lines[at].split(',');
    return lines.slice(at + 1).filter(Boolean).map(line => {
        const values = line.split(',');
        assert.equal(values.length, keys.length, command.name + ': malformed CSV');
        return Object.fromEntries(keys.map((key, i) => [key, values[i]]));
    });
}
const outcomes = {}, seen = new Set(), groups = {};
for (const command of manifest.commands) {
    assert.ok(!seen.has(command.name), 'Duplicate command'); seen.add(command.name);
    for (const suffix of ['.csv', '.stderr.txt']) assert.ok(fs.existsSync(path.join(directory, command.name + suffix)));
    outcomes[command.status] = (outcomes[command.status] || 0) + 1;
    const key = command.name.replace(/-run\d+$/, '');
    (groups[key] ||= []).push({...command, rows: command.status === 'completed' ? csv(command) : []});
}
const counts = manifest.fixtureCounts;
assert.equal(manifest.commands.length, 3 * (counts.decimal + counts.bigint + counts.application + 23) + 3 +
    3 * (manifest.profileCases?.length || 0) + (manifest.queueRecorded ? 1 : 0),
    'Unexpected attempt count');
for (const [file, family] of [['decimal-cases.tsv', 'decimal'], ['bigint-cases.tsv', 'bigint']]) {
    const fixtures = fs.readFileSync(path.join(directory, file), 'utf8').trim().split(/\r?\n/);
    assert.equal(fixtures.length, counts[family]);
    for (const fixture of fixtures) {
        const key = family + '-' + fixture.split('\t')[0];
        assert.equal(groups[key]?.length, 3, key + ': missing attempts');
        for (const attempt of groups[key]) if (attempt.status === 'completed')
            assert.equal(attempt.rows.length, 1, key + ': missing verified timing');
    }
}
const median = values => [...values].sort((a, b) => a - b)[Math.floor(values.length / 2)];
const results = Object.entries(groups).filter(([name]) => /^(decimal|bigint|format|cache)-/.test(name)).map(([name, attempts]) => {
    const rows = attempts.flatMap(x => x.rows), times = rows.map(x => +(x.median_ns_per_op ?? x.median_ns) / 1e6);
    assert.ok(times.every(x => Number.isFinite(x) && x >= 0), name);
    return {name, completed: rows.length, outcomes: attempts.map(x => x.status),
        medianMs: times.length ? median(times) : null,
        lowMs: times.length ? Math.min(...times) : null, highMs: times.length ? Math.max(...times) : null,
        validation: [...new Set(rows.map(x => x.validation).filter(Boolean))],
        peakLiveBytes: rows.length && ('peak_live_bytes' in rows[0] || 'peak_tracked_payload_bytes' in rows[0]) ?
            Math.max(...rows.map(x => +(x.peak_live_bytes ?? x.peak_tracked_payload_bytes))) : null};
});
const applications = manifest.applications.map(([name, expression, places, notation = 0]) => {
    const attempts = groups['limit-' + name]; assert.equal(attempts?.length, 3, name);
    const rows = attempts.flatMap(x => x.rows);
    assert.equal(rows.length, 3, name + ': incomplete application outcomes');
    return {name, expression, places, notation, status: rows.map(x => x.status),
        statusCodes: rows.map(x => +x.status_code), medianMs: median(rows.map(x => +x.elapsed_ms)),
        lowMs: Math.min(...rows.map(x => +x.elapsed_ms)), highMs: Math.max(...rows.map(x => +x.elapsed_ms)),
        requestedBytes: Math.max(...rows.map(x => +x.requested_bytes)),
        budgetReasons: rows.map(x => x.budget_reason || 'unrecorded'),
        outputBytes: Math.max(...rows.map(x => +x.output_bytes))};
});
const complex = Object.entries(groups).filter(([name]) => name.startsWith('complex-')).flatMap(([name, attempts]) =>
    attempts.flatMap(x => x.rows).map(row => ({...row, source: name})));
const profiles = (manifest.profileCases || []).map(name => {
    const attempts = groups['profile-' + name]; assert.equal(attempts?.length, 3, name);
    assert.ok(attempts.every(x => x.rows.length === 1), name + ': incomplete profile');
    return {name, rows: attempts.flatMap(x => x.rows)};
});
const queue = manifest.queueRecorded ? groups['http-queue'] : [];
if (manifest.queueRecorded) {
    assert.equal(queue?.length, 1); assert.equal(queue[0].rows.length, 3);
}
console.log(JSON.stringify({commit: manifest.commit, recordedAt: manifest.recordedAt,
    completedAt: manifest.completedAt, fixtureCounts: counts, attemptCount: manifest.commands.length,
    outcomes, results: results.sort((a, b) => (b.medianMs || 0) - (a.medianMs || 0)),
    applications: applications.sort((a, b) => b.medianMs - a.medianMs), complex,
    profiles, queue: queue.flatMap(x => x.rows)}, null, 2));
