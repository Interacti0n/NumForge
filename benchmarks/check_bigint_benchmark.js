const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const { makeCases } = require('./bigint_cases');
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'numforge-bigint-'));
try {
    const fixtures = path.join(directory, 'cases.tsv');
    const rows = makeCases(true);
    const factorialRows = makeCases(true, true);
    if (factorialRows.length !== 8 || factorialRows.some(row => !/^(factorial|tree)-(0|1|20|100)\t/.test(row)))
        throw Error('Invalid factorial-only smoke fixtures');
    fs.writeFileSync(fixtures, rows.join('\n') + '\n');
    const result = spawnSync(process.argv[2], [fixtures, '--quick', '--check'], {
        encoding: 'utf8', windowsHide: true, maxBuffer: 8 * 1024 * 1024
    });
    if (result.error || result.status !== 0) throw result.error || Error(result.stderr);
    const output = result.stdout.trim().split(/\r?\n/);
    if (output.length !== rows.length + 1) throw Error('Missing benchmark rows');
    for (let i = 1; i < output.length; i++) {
        const columns = output[i].split(',');
        if (columns.length !== 15 || columns[0] !== rows[i - 1].split('\t')[0] ||
            columns.slice(3).some(v => !Number.isFinite(Number(v)) || Number(v) < 0))
            throw Error('Invalid benchmark output: ' + output[i]);
        if (+columns[6] > +columns[7] || +columns[7] > +columns[8] ||
            +columns[13] < +columns[11] || +columns[13] < +columns[12]) throw Error('Invalid metrics');
    }
    // A corrupted reference must fail, including in Release/NDEBUG builds.
    fs.writeFileSync(fixtures, 'bad\tmul\treuse\t12\t13\t157\t0\n');
    const bad = spawnSync(process.argv[2], [fixtures, '--quick'], { encoding: 'utf8', windowsHide: true });
    if (bad.error || bad.status !== 1 || !bad.stderr.includes('Failed exact reference'))
        throw Error('Corrupted oracle accepted');
    // A record above the 1 MiB buffer must be rejected before parsing a
    // truncated BigInt reference. Large complete references are checked by
    // the full factorial sweep, not added to fast CI timings.
    fs.writeFileSync(fixtures, 'oversized\tmul\treuse\t'+'9'.repeat(1048576)+'\t1\t1\t0\n');
    const oversized = spawnSync(process.argv[2], [fixtures, '--quick'], { encoding: 'utf8', windowsHide: true });
    if (oversized.error || oversized.status !== 1) throw Error('Oversized record accepted');
    console.log(`Validated ${rows.length} exact-reference cases and rejected a corrupted reference.`);
} finally {
    fs.rmSync(directory, { recursive: true, force: true });
}
