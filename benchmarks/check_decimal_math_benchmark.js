const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const assert = require('node:assert/strict');
const {spawnSync} = require('node:child_process');
const {makeCases} = require('./decimal_math_cases');
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'numforge-decimal-'));
const fixtures = path.join(directory, 'cases.tsv');
const binary = process.argv[2];
function invoke(parameters) {
    return spawnSync(binary, [fixtures, ...parameters], {encoding: 'utf8', windowsHide: true,
        timeout: 110000, maxBuffer: 8 * 1024 * 1024});
}
function checkRows(output, rows) {
    const lines = output.trim().split(/\r?\n/), header = lines.shift().split(',');
    assert.equal(lines.length, rows.length, 'Missing measurement rows');
    let bounded = 0;
    lines.forEach((line, i) => {
        const fields = line.split(','); assert.equal(fields.length, header.length);
        const values = Object.fromEntries(header.map((key, j) => [key, fields[j]]));
        assert.equal(values.case, rows[i].split('\t')[0]);
        for (const key of header.slice(3).filter(key => key !== 'validation'))
            assert.ok(Number.isFinite(+values[key]) && (key === 'parameter' || +values[key] >= 0), key + ':' + values[key]);
        assert.ok(+values.min_ns_per_op <= +values.median_ns_per_op && +values.median_ns_per_op <= +values.max_ns_per_op);
        assert.ok(+values.peak_live_bytes >= +values.baseline_live_bytes && +values.peak_live_bytes >= +values.live_bytes);
        assert.ok(['exact', 'bounded_directed'].includes(values.validation));
        if (values.validation === 'bounded_directed') {
            assert.ok(values.case.startsWith('legacy_') && +values.rounding < 4);
            bounded++;
        }
        // Sub-resolution phases may report zero; never enforce timing
        // thresholds in CI. All diagnostic fields must remain finite/nonnegative.
    });
    return bounded;
}
try {
    const rows = makeCases(true);
    fs.writeFileSync(fixtures, rows.join('\n') + '\n');
    const result = invoke(['--quick']);
    if (result.error || result.status !== 0) throw result.error || Error(result.stderr);
    const bounded = checkRows(result.stdout, rows);
    // The extreme pole regression must now satisfy the strict oracle.
    const pole = makeCases(false).find(row => row.startsWith('tan_near_pole_p10\t'));
    fs.writeFileSync(fixtures, pole + '\n');
    const diagnostic = invoke(['--validate']);
    if (diagnostic.error) throw diagnostic.error;
    assert.equal(diagnostic.status, 0, diagnostic.stderr);
    checkRows(diagnostic.stdout, [pole]);
    // Assertions remain effective in Release builds. Both nearest and exact
    // arithmetic must reject a wrong oracle rather than producing a timing row.
    for (const row of ['bad\tadd\treuse\t12\t13\t0\t5\t0\t0\t26',
                       'bad_nearest\texp\treuse\t1\t0\t10\t5\t0\t1\t2.718281830']) {
        fs.writeFileSync(fixtures, row + '\n');
        const bad = invoke(['--validate']);
        assert.equal(bad.status, 1); assert.match(bad.stderr, /Failed independent reference/);
    }
    console.log(`Validated ${rows.length} decimal/math cases; bounded directed rows=${bounded}; extreme-pole diagnostic=${diagnostic.status === 0 ? 'verified' : 'reference_mismatch'}; corrupted references rejected.`);
} finally {
    assert.equal(path.dirname(directory), path.resolve(os.tmpdir()));
    fs.rmSync(directory, {recursive: true, force: true});
}
module.exports = {checkRows};
