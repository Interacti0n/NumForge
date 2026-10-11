// Run only after builds/correctness checks finish; each benchmark process is serial.
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const crypto = require('node:crypto');
const {spawnSync} = require('node:child_process');
const {makeCases} = require('./karatsuba_cases.cjs');
const executable = path.resolve(process.argv[2] || '');
const output = path.resolve(process.argv[3] || '');
const factorials = process.argv.length === 6 && process.argv[4] === '--factorials';
if ((!factorials && process.argv.length !== 4) || !fs.existsSync(executable))
    throw Error('Usage: node run_karatsuba.cjs executable empty-output-directory [--factorials fixtures.tsv]');
fs.mkdirSync(output, {recursive:true});
if (fs.readdirSync(output).length) throw Error('Output directory must be empty');
const rows = factorials ? fs.readFileSync(process.argv[5], 'utf8').trim().split(/\r?\n/) : makeCases(false, true);
const hash = text => crypto.createHash('sha256').update(text).digest('hex');
const fixture = rows.join('\n') + '\n';
fs.writeFileSync(path.join(output, 'cases.tsv'), fixture);
const revision = spawnSync('git', ['rev-parse', 'HEAD'], {encoding:'utf8', windowsHide:true});
fs.writeFileSync(path.join(output, 'metadata.json'), JSON.stringify({
    started:new Date().toISOString(), executable, executableSha256:hash(fs.readFileSync(executable)),
    fixtureSha256:hash(fixture), revision:revision.stdout.trim(),
    platform:process.platform, arch:process.arch, cpu:os.cpus()[0]?.model,
    node:process.version, rows:rows.length, runs:3, samplesPerRun:factorials ? 1 : 3,
    order:'forward, reverse, forward', timing:'whole operation; parsing/copying/reference comparison excluded',
    conditions:'No affinity/frequency lock. Run without concurrent builds or tests.'
}, null, 2));
const recordings = [];
for (let run = 0; run < 3; ++run) {
    const input = path.join(output, `run-${run + 1}.tsv`);
    fs.writeFileSync(input, (run === 1 ? [...rows].reverse() : rows).join('\n') + '\n');
    const result = spawnSync(executable, [input, factorials ? '--validate' : '--quick'], {
        encoding:'utf8', windowsHide:true, maxBuffer:8 * 1024 * 1024, timeout:600000
    });
    fs.writeFileSync(path.join(output, `run-${run + 1}.csv`), result.stdout || '');
    fs.writeFileSync(path.join(output, `run-${run + 1}.stderr`), result.stderr || '');
    if (result.error || result.status !== 0) throw result.error || Error(result.stderr);
    const lines = result.stdout.trim().split(/\r?\n/).slice(1);
    if (lines.length !== rows.length) throw Error('Missing benchmark rows');
    recordings.push(new Map(lines.map(line => {
        const f = line.split(',');
        if (f.length !== 15 || f.slice(3).some(v => !Number.isFinite(+v) || +v < 0))
            throw Error('Invalid CSV');
        return [f[0], {ns:+f[7], calls:+f[9], bytes:+f[10], baseline:+f[11], peak:+f[13]}];
    })));
    console.log(`Completed serial recording ${run + 1}/3: ${lines.length} exact products`);
}
const summary = [];
for (const row of rows) {
    const name = row.split('\t')[0], values = recordings.map(r => r.get(name));
    if (values.some(v => !v)) throw Error('Missing named recording');
    const sorted = values.map(v => v.ns).sort((a,b) => a-b);
    summary.push({name, minNs:sorted[0], medianNs:sorted[1], maxNs:sorted[2],
        calls:values[0].calls, requestedBytes:values[0].bytes,
        peakExtraBytes:values[0].peak-values[0].baseline});
}
fs.writeFileSync(path.join(output, 'summary.json'), JSON.stringify(summary, null, 2));
console.log('Median milliseconds across three per-process medians:');
if (factorials) for (const row of summary)
    console.log(row.name, (row.medianNs / 1e6).toFixed(3), 'ms');
else for (const n of [32,64,128,256,512,1024,2048,4096,8192]) {
    console.log(n, ['mul','karatsuba16','karatsuba32','karatsuba64','karatsuba128','karatsuba256'].map(op => {
        const item = summary.find(r => r.name === `balanced-${n}-reuse-${op}`);
        return `${op}=${(item.medianNs/1e6).toFixed(4)}`;
    }).join(' '));
}
