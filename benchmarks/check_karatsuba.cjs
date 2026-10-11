const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const {makeCases} = require('./karatsuba_cases.cjs');
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'numforge-karatsuba-'));
try {
    const fixtures = path.join(directory, 'cases.tsv');
    const rows = makeCases(!process.argv.includes('--full'));
    fs.writeFileSync(fixtures, rows.join('\n') + '\n');
    const result = spawnSync(process.argv[2], [fixtures, '--validate'], {
        encoding:'utf8', windowsHide:true, maxBuffer:16 * 1024 * 1024, timeout:180000
    });
    if (result.error || result.status !== 0) throw result.error || Error(result.stderr);
    const output = result.stdout.trim().split(/\r?\n/);
    if (output.length !== rows.length + 1) throw Error('Missing measurements');
    for (let i = 1; i < output.length; ++i) {
        const fields = output[i].split(',');
        if (fields.length !== 15 || fields[0] !== rows[i - 1].split('\t')[0] || fields[5] !== '1' ||
            fields.slice(3).some(v => !Number.isFinite(+v) || +v < 0) ||
            +fields[13] < +fields[11] || +fields[13] < +fields[12]) throw Error('Invalid measurement');
    }
    const corrupt = rows.find(row => row.startsWith('balanced-16-alias_a-karatsuba16\t')).split('\t');
    corrupt[0] = 'corrupt'; corrupt[5] = String(BigInt(corrupt[5]) + 1n);
    fs.writeFileSync(fixtures, corrupt.join('\t') + '\n');
    const bad = spawnSync(process.argv[2], [fixtures, '--validate'], {encoding:'utf8', windowsHide:true});
    if (bad.error || bad.status !== 1 || !bad.stderr.includes('Failed exact reference')) throw Error('Corrupt reference accepted');
    for (const [text, expectedStatus] of [
        ['hex\tkaratsuba16\talias_a\thex:-000C\thex:D\thex:-9c\t0\n', 0],
        ['zero\tkaratsuba32\treuse\thex:-0000\t123\thex:0\t0\n', 0],
        ['bad\tmul\treuse\thex:xyz\t1\t1\t0\n', 1],
        ['bad\tmul\treuse\thex:\t1\t1\t0\n', 1]
    ]) {
        fs.writeFileSync(fixtures, text);
        const check = spawnSync(process.argv[2], [fixtures, '--validate'], {encoding:'utf8', windowsHide:true});
        if (check.error || check.status !== expectedStatus) throw Error('Hex fixture contract failed');
    }
    console.log(`Karatsuba: ${rows.length} whole-product references passed; corrupt reference rejected.`);
} finally {
    fs.rmSync(directory, {recursive:true, force:true});
}
