'use strict';
// Serial measurements: do not build or run another CPU-intensive suite beside
// this recorder. Timeouts apply to whole child processes, not individual APIs.
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const {spawnSync} = require('node:child_process');
const {makeCases: decimalCases} = require('./decimal_math_cases');
const {makeCases: bigintCases} = require('./bigint_cases');
const [binaryDirectory, outputDirectory] = process.argv.slice(2);
if (!binaryDirectory || !outputDirectory || process.argv.length !== 4)
    throw Error('Usage: node benchmarks/run_performance_audit.js binary-directory empty-output-directory');
const root = path.resolve(__dirname, '..'), binaries = path.resolve(binaryDirectory);
const output = path.resolve(outputDirectory);
fs.mkdirSync(output, {recursive: true});
if (fs.readdirSync(output).length) throw Error('Use an empty directory; previous results are preserved');
const env = {};
for (const [key, value] of Object.entries(process.env))
    if (!Object.keys(env).some(previous => previous.toLowerCase() === key.toLowerCase())) env[key] = value;
delete env.NUMFORGE_BENCH_EXPECTED;
const binary = name => path.join(binaries, name + (process.platform === 'win32' ? '.exe' : ''));
function optional(command, args) {
    const r = spawnSync(command, args, {encoding: 'utf8', windowsHide: true, cwd: root, env});
    return r.status === 0 ? r.stdout.trim() : 'unavailable';
}
const manifest = {recordedAt: new Date().toISOString(), commit: optional('git', ['rev-parse', 'HEAD']),
    status: optional('git', ['status', '--short']), cpus: os.cpus(), platform: process.platform,
    os: os.release(), node: process.version, totalMemory: os.totalmem(),
    powerScheme: process.platform === 'win32' ? optional('powercfg', ['/getactivescheme']) : 'unavailable',
    note: 'Serial; no affinity/frequency lock or control of unrelated background processes. Request counters enabled; live tracking only in separate probes.',
    commands: []};
fs.writeFileSync(path.join(output, 'working-tree.patch'), optional('git', ['diff', 'HEAD']));
fs.mkdirSync(path.join(output, 'sources'));
for (const file of ['run_performance_audit.js', 'summarize_performance_audit.js', 'bigint_cases.js',
    'decimal_math_cases.js', 'calculator_limit_benchmark.c', 'decimal_math_benchmark.c'])
    fs.copyFileSync(path.join(__dirname, file), path.join(output, 'sources', file));
const cache = path.resolve(binaries, '..', 'CMakeCache.txt');
if (fs.existsSync(cache)) {
    fs.copyFileSync(cache, path.join(output, 'CMakeCache.txt'));
    const files = path.join(path.dirname(cache), 'CMakeFiles');
    for (const entry of fs.readdirSync(files)) {
        const compiler = path.join(files, entry, 'CMakeCCompiler.cmake');
        if (fs.existsSync(compiler)) fs.copyFileSync(compiler, path.join(output, 'CMakeCCompiler.cmake'));
    }
}
function saveManifest() { fs.writeFileSync(path.join(output, 'manifest.json'), JSON.stringify(manifest, null, 2) + '\n'); }
function record(name, executable, args, timeout = 15000) {
    const begin = Date.now();
    const r = spawnSync(executable, args, {cwd: root, env, encoding: 'utf8', windowsHide: true,
        timeout, maxBuffer: 16 * 1024 * 1024});
    const status = r.error?.code === 'ETIMEDOUT' ? 'process_timeout' : r.error ? 'process_error' :
        r.status === 0 ? 'completed' : r.stderr.includes('Reference mismatch:') ? 'reference_mismatch' : 'failed';
    fs.writeFileSync(path.join(output, name + '.csv'), r.stdout || '');
    fs.writeFileSync(path.join(output, name + '.stderr.txt'), r.stderr || '');
    manifest.commands.push({name, executable, args, timeoutMs: timeout, elapsedMs: Date.now() - begin,
        status, exitCode: r.status, error: r.error?.message});
    saveManifest();
    if (status !== 'completed') console.error(`${name}: ${status}`);
    if (manifest.commands.length % 50 === 0) console.error(`Recorded ${manifest.commands.length} attempts; latest ${name}`);
    return r;
}
saveManifest();
record('allocation-check', binary('allocation_stats_check'), []);
const decimals = decimalCases(false);
const decimalFile = path.join(output, 'decimal-cases.tsv');
fs.writeFileSync(decimalFile, decimals.join('\n') + '\n');
// Full independent matrix, one API call in each of three fresh processes.
// No untimed warm-up or memory/phase calls are included in timing-only mode.
for (let run = 1; run <= 3; run++) for (const row of decimals) {
    const name = row.split('\t')[0];
    record(`decimal-${name}-run${run}`, binary('decimal_math_benchmark'),
        [decimalFile, '--validate', '--timing-only', '--case', name], 12000);
}
// Representative size sweeps retain the existing full seven-sample protocol.
// The very large GCD uses one verified call per fresh process and a separate
// memory probe, rather than repeatedly warming a long Euclidean chain.
const integers = bigintCases(false).filter(row => {
    const [name, op, mode] = row.split('\t');
    return mode === 'reuse' && (name.startsWith('mul-balanced-') || name.startsWith('mul-ratio-') ||
        name.startsWith('mul-reverse-large-') || name.startsWith('square-mul-large-') ||
        /^square-special-(large-)?\d+$/.test(name) || ['divmod', 'gcd', 'pow'].includes(op) ||
        /^(factorial|tree)-(20|10000|50000|100000)$/.test(name));
});
const integerFile = path.join(output, 'bigint-cases.tsv');
fs.writeFileSync(integerFile, integers.join('\n') + '\n');
for (let run = 1; run <= 3; run++) for (const row of integers)
    record(`bigint-${row.split('\t')[0]}-run${run}`, binary('bigint_arithmetic_benchmark'),
        [integerFile, ...(row.startsWith('gcd-large-power-pair-') ? ['--validate'] : []),
            '--case', row.split('\t')[0]], 120000);
const formats = ['bigint_full', 'auto_10', 'scientific_100', 'mode_scientific_full', 'fixed_10'];
const caches = ['legacy_cold', 'legacy_hit', 'legacy_approx_precision', 'session_cold', 'session_hit', 'session_commit'];
for (let run = 1; run <= 3; run++) {
    for (const format of formats) for (const digits of [16, 1024, 16384])
        record(`format-${format}-${digits}-run${run}`, binary('decimal_format_benchmark'),
            ['--case', format, '--digits', String(digits)]);
    for (const name of caches) record(`cache-${name}-run${run}`, binary('cache_memory_benchmark'), ['--case', name]);
    record(`calculator-phases-run${run}`, binary('calculator_benchmark'), ['--factorial-only'], 30000);
    record(`http-run${run}`, process.execPath, [path.join(__dirname, 'http_benchmark.js'), binary('numforge_web')], 60000);
}
for (const precision of [34, 250])
    record(`complex-p${precision}`, process.execPath,
        [path.join(root, 'tests', 'test_complex_oracle.js'), binary('complex_oracle_driver'),
            '--benchmark', `--precision=${precision}`], 300000);
// These are exploratory application outcomes, not independently validated
// mathematical references. The CLI/C API pipeline keeps all default budgets.
const applications = [
    ['factorial_50000', '50000!', 10], ['factorial_100000', '100000!', 10],
    ['power_100000', '2^100000', 10], ['power_1000000', '2^1000000', 10],
    ['rational_large', '2^100000/3^63000', 10],
    ['scale_gap', '1+1E-100000', 10],
    ['complex_integer', '(2^100000+2^100000*i)', 10],
    ['unit_rational', 'convert(1/3;"m";"cm")', 10],
    ['ncr_1000', 'ncr(1000;500)', 10], ['ncr_10000', 'ncr(10000;5000)', 10],
    ['ncr_100000', 'ncr(100000;50000)', 10],
    ['npr_10000', 'npr(10000;10000)', 10], ['npr_50000', 'npr(50000;50000)', 10],
    ['gcd_large', 'gcd(2^100000-1;3^63000)', 10],
    ['lcm_large', 'lcm(2^10000-1;2^9999-1)', 10],
    ['isqrt_10000', 'isqrt(2^10000)', 10], ['isqrt_100000', 'isqrt(2^100000)', 10],
];
for (const places of [10, 100, 1000, 2000, 10000]) for (const [name, expression] of [
    ['ln', 'ln(2)'], ['sin_large_angle', 'sin(1E100)'], ['atan', 'atan(1)'],
    ['tan_pole', 'tan(1.5707963267948966192313216916397514420985)'],
    ['sqrt', 'sqrt(2)'], ['complex_ln', 'ln(1+i)'], ['complex_tan', 'tan(1+i)'],
]) applications.push([`${name}_p${places}`, expression, places]);
for (const [name, expression, places] of [
    ['factorial_full', '20000!', -1], ['factorial_full_small', '10000!', -1],
    ['integer_full', '2^200000', -1], ['integer_plain', '2^200000', 10],
]) applications.push([name, expression, places, name === 'integer_plain' ? 1 : 0]);
applications.push(
    ['log10_p2000', 'log(2)', 2000], ['log_p2000', 'log(2;3)', 2000],
    ['atanh_boundary_p2000', 'atanh(0.99999999999999999999)', 2000],
    ['exp_p2000', 'exp(1)', 2000], ['exp_p10000', 'exp(1)', 10000],
    ['root_p10000', 'root(2;7)', 10000], ['cbrt_p10000', 'cbrt(2)', 10000],
    ['pi_p2000', 'π', 2000], ['pi_p10000', 'π', 10000], ['phi_p10000', 'φ', 10000],
    ['double_ln_p2000', 'ln(2)+ln(3)', 2000],
    ['sin_cos_p2000', 'sin(1E100)+cos(1E100)', 2000], ['exp_ln_p2000', 'exp(ln(2))', 2000],
    ['complex_auto_output_limit', '2^110000+2^110000*i', 10, 0],
    ['complex_scientific_same', '2^110000+2^110000*i', 10, 2]);
manifest.applications = applications;
// Fill the forward/inverse complex and intermediate-precision gaps. These
// remain resource diagnostics; independent mathematical references are checked
// by the decimal and complex oracle families, not inferred from a success code.
for (const places of [100, 1000, 2000, 5000, 10000])
    for (const op of ['sin', 'cos', 'sinh', 'cosh', 'tanh'])
        applications.push([`complex_${op}_p${places}`, `${op}(1+i)`, places]);
for (const op of ['exp', 'sqrt', 'asin', 'acos', 'atan', 'asinh', 'acosh', 'atanh'])
    applications.push([`complex_${op}_p2000`, `${op}(1+i)`, 2000]);
for (const [name, expression] of [
    ['complex_tan_scaled', 'tan(1+100*i)'], ['complex_tanh_scaled', 'tanh(100+i)'],
    ['complex_sinh_tiny', 'sinh(1E-100+1E-100*i)'],
    ['complex_sinh_boundary', 'sinh(0.5+0.5*i)'],
    ['complex_log_branch', 'ln(-1+1E-100*i)'],
    ['complex_power', '(1+i)^(0.5+0.25*i)'],
    ['rational_complex', '(2^10000+i)/(3^6300-i)'],
    ['quantity_sum', 'qty(1/3;"m")+qty(1/6;"m")'],
]) applications.push([`${name}_p2000`, expression, 2000]);
for (const [name, expression] of [['ln', 'ln(2)'], ['exp', 'exp(1)'],
    ['sin_large_angle', 'sin(1E100)'], ['root', 'root(2;7)'], ['pi', 'π']])
    applications.push([`${name}_p5000`, expression, 5000]);
const profiles = ['mul_2000_reuse', 'large_division_2000_dense_divsig_reuse',
    'exp_ordinary_p2000', 'ln_ordinary_p2000', 'log10_ordinary_p2000',
    'sin_large_angle_p2000', 'tan_resolved_pole_p2000', 'sinh_ordinary_p2000',
    'sqrt_ordinary_p2000', 'root_degree7_p2000', 'atanh_near_boundary_p2000',
    'pi_factory_p2000'];
for (const name of profiles)
    if (!decimals.some(row => row.startsWith(name + '\t'))) throw Error('Missing profile fixture ' + name);
manifest.profileCases = profiles;
saveManifest();
for (const [name, expression] of applications)
    fs.writeFileSync(path.join(output, `input-${name}.txt`), expression, 'utf8');
for (let run = 1; run <= 3; run++) for (const [name, expression, places, notation = 0] of applications)
    record(`limit-${name}-run${run}`, binary('calculator_limit_benchmark'),
        ['--input-file', path.join(output, `input-${name}.txt`), String(places), String(notation), '5000'], 12000);
// Separate memory/phase calls: profiling clocks do not contaminate headline
// API timers. Every probe still checks its full independent decimal reference.
for (let run = 1; run <= 3; run++) for (const name of profiles)
    record(`profile-${name}-run${run}`, binary('decimal_math_benchmark'),
        [decimalFile, '--validate', '--case', name], 60000);
record('http-queue', process.execPath,
    [path.join(__dirname, 'http_queue_benchmark.js'), binary('numforge_web')], 60000);
manifest.queueRecorded = true;
manifest.completedAt = new Date().toISOString();
manifest.fixtureCounts = {decimal: decimals.length, bigint: integers.length, application: applications.length};
saveManifest();
console.error(`Audit complete: ${manifest.commands.length} attempts in ${output}`);
const failures = manifest.commands.filter(command =>
    ['failed', 'process_error', 'reference_mismatch'].includes(command.status));
if (failures.length) {
    console.error(`${failures.length} unexpected harness/reference failures; inspect manifest and stderr artifacts.`);
    process.exitCode = 1;
}
