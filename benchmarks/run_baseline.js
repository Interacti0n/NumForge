const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const { spawnSync } = require('node:child_process');
const root = path.resolve(__dirname, '..');
const args = process.argv.slice(2);
const binaryDirectory = args.shift();
const outputDirectory = args.shift();
const quick = args.includes('--quick');
const bigintSuite = args.includes('--bigint');
const factorialOnly = args.includes('--factorial-only');
const decimalSuite = args.includes('--decimal');
const timeoutIndex = args.indexOf('--case-timeout-ms');
const caseTimeout = timeoutIndex < 0 ? (bigintSuite && !quick ? 120000 : 15000) : Number(args[timeoutIndex + 1]);
if (timeoutIndex >= 0) args.splice(timeoutIndex, 2);
const prefixIndex = args.indexOf('--case-prefix');
const casePrefix = prefixIndex < 0 ? null : args[prefixIndex + 1];
if (prefixIndex >= 0) args.splice(prefixIndex, 2);
if (!binaryDirectory || !outputDirectory || (bigintSuite && decimalSuite) || (factorialOnly && !bigintSuite) ||
    !Number.isInteger(caseTimeout) || caseTimeout < 1 ||
    (prefixIndex >= 0 && (!decimalSuite || !casePrefix || !/^[A-Za-z0-9_-]+$/.test(casePrefix))) ||
    args.some(v => v !== '--quick' && v !== '--bigint' && v !== '--decimal' && v !== '--factorial-only'))
    throw Error('Usage: node benchmarks/run_baseline.js binary-directory output-directory [--quick] [--bigint|--decimal] [--factorial-only] [--case-timeout-ms N] [--case-prefix name]');
const binaries = path.resolve(binaryDirectory);
const output = path.resolve(outputDirectory);
fs.mkdirSync(output, { recursive: true });
if (fs.readdirSync(output).length) throw Error('Use an empty output directory; existing baselines are preserved');
// Windows environment names are case-insensitive; avoid duplicate PATH/Path
// when launching MSBuild or other tools through Node.
const env = {}, seen = new Set();
for (const [key, value] of Object.entries(process.env)) {
    if (!seen.has(key.toUpperCase())) { seen.add(key.toUpperCase()); env[key] = value; }
}
function capture(command, arguments_, timeoutMs) {
    const result = spawnSync(command, arguments_, { cwd: root, env, encoding: 'utf8',
        windowsHide: true, timeout: timeoutMs, maxBuffer: 16 * 1024 * 1024 });
    if (result.error || result.status !== 0) throw result.error || Error(result.stderr || `Exit ${result.status}`);
    return result;
}
function optional(command, arguments_) {
    try { return capture(command, arguments_).stdout.trim(); } catch { return 'unavailable'; }
}
const extension = process.platform === 'win32' ? '.exe' : '';
const binary = name => path.join(binaries, name + extension);
const manifest = {
    recordedAt: new Date().toISOString(), commit: optional('git', ['rev-parse', 'HEAD']),
    status: optional('git', ['status', '--short']), platform: process.platform,
    release: os.release(), processArch: process.arch,
    cpus: os.cpus().map(cpu => ({ model: cpu.model, reportedMHz: cpu.speed })),
    totalSystemMemoryBytes: os.totalmem(), node: process.version,
    powerScheme: process.platform === 'win32' ? optional('powercfg', ['/getactivescheme']) : 'record externally',
    mode: quick ? 'quick harness smoke' : 'full diagnostic baseline', suite: decimalSuite ? 'decimal/math' : bigintSuite ? 'bigint arithmetic' : 'format/cache/http', repetitions: 3,
    instrumentation: 'requested allocations always on; live ledger in separate diagnostic samples; exclusive phase hooks',
    processPeak: process.platform === 'win32' ? 'peak_working_set_bytes' : 'peak_rss_bytes',
    commands: [], note: 'No CPU affinity or frequency locking; repeated runs do not imply an idle machine.'
};
fs.writeFileSync(path.join(output, 'working-tree.patch'), optional('git', ['diff', 'HEAD']));
// Include new harness/profiler sources: git diff excludes untracked files.
const untracked = optional('git', ['ls-files', '--others', '--exclude-standard']);
if (untracked !== 'unavailable') for (const file of untracked.split(/\r?\n/).filter(Boolean)) {
    if (!file.startsWith('benchmarks/') && !file.startsWith('src/internal/')) continue;
    const target = path.join(output, 'untracked-source', file);
    fs.mkdirSync(path.dirname(target), { recursive: true }); fs.copyFileSync(path.join(root, file), target);
}
const cache = [path.join(binaries, '..', 'CMakeCache.txt'), path.join(binaries, 'CMakeCache.txt')].find(fs.existsSync);
if (cache) fs.copyFileSync(cache, path.join(output, 'CMakeCache.txt'));
// The generated compiler description contains the compiler version, target
// architecture and toolchain identity, beyond the executable path in the cache.
if (cache) {
    const files = path.join(path.dirname(cache), 'CMakeFiles');
    if (fs.existsSync(files)) for (const entry of fs.readdirSync(files)) {
        const compiler = path.join(files, entry, 'CMakeCCompiler.cmake');
        if (fs.existsSync(compiler)) fs.copyFileSync(compiler, path.join(output, 'CMakeCCompiler.cmake'));
    }
}
function save(name, command, parameters) {
    console.error(name);
    const begin = Date.now();
    const result = capture(command, parameters, caseTimeout);
    fs.writeFileSync(path.join(output, name + '.csv'), result.stdout);
    fs.writeFileSync(path.join(output, name + '.stderr.txt'), result.stderr);
    manifest.commands.push({ name, command, parameters, elapsedMs: Date.now() - begin });
    fs.writeFileSync(path.join(output, 'manifest.json'), JSON.stringify(manifest, null, 2) + '\n');
}
save('allocation-check', binary('allocation_stats_check'), []);
if (decimalSuite) {
    const {makeCases} = require('./decimal_math_cases');
    const rows = makeCases(quick).filter(row => !casePrefix || row.startsWith(casePrefix));
    if (!rows.length) throw Error('No decimal cases match --case-prefix');
    const fixtures = path.join(output, 'decimal-cases.tsv');
    fs.writeFileSync(fixtures, rows.join('\n') + '\n');
    manifest.caseTimeoutMs = caseTimeout;
    manifest.casePrefix = casePrefix;
    manifest.fixtureCount = rows.length;
    manifest.outcomes = {verified: 0, boundedDirected: 0, referenceMismatch: 0, timedOut: 0};
    for (let run = 1; run <= 3; run++) {
        for (const row of rows) {
            const name = row.split('\t')[0];
            const parameters = [fixtures, ...(quick ? ['--quick'] : []), '--case', name];
            const begin = Date.now();
            const result = spawnSync(binary('decimal_math_benchmark'), parameters, {
                cwd: root, env, encoding: 'utf8', windowsHide: true,
                timeout: caseTimeout, maxBuffer: 1024 * 1024});
            const timedOut = result.error?.code === 'ETIMEDOUT';
            const entry = {name, run, command: binary('decimal_math_benchmark'), parameters,
                elapsedMs: Date.now() - begin, status: timedOut ? 'timeout' : 'verified'};
            if (timedOut) {
                manifest.outcomes.timedOut++;
                fs.writeFileSync(path.join(output, `decimal-${name}-run${run}.timeout.txt`),
                    `Timed out after ${caseTimeout} ms. No verified timing or memory metrics.\n`);
            } else if (!result.error && result.status === 1 && result.stderr.startsWith('Reference mismatch: ')) {
                entry.status = 'reference_mismatch';
                entry.stderr = result.stderr;
                manifest.outcomes.referenceMismatch++;
                fs.writeFileSync(path.join(output, `decimal-${name}-run${run}.reference-mismatch.txt`), result.stderr);
            } else if (result.error || result.status !== 0) {
                entry.status = 'failed'; entry.stderr = result.stderr;
                manifest.commands.push(entry);
                fs.writeFileSync(path.join(output, 'manifest.json'), JSON.stringify(manifest, null, 2) + '\n');
                throw result.error || Error(result.stderr || `Exit ${result.status}`);
            } else {
                const lines = result.stdout.trim().split(/\r?\n/);
                if (lines.length !== 2 || lines[1].split(',')[0] !== name)
                    throw Error('Missing isolated measurement: ' + name);
                fs.writeFileSync(path.join(output, `decimal-${name}-run${run}.csv`), result.stdout);
                fs.writeFileSync(path.join(output, `decimal-${name}-run${run}.stderr.txt`), result.stderr);
                manifest.outcomes.verified++;
                if (lines[1].split(',')[6] === 'bounded_directed') manifest.outcomes.boundedDirected++;
            }
            manifest.commands.push(entry);
            fs.writeFileSync(path.join(output, 'manifest.json'), JSON.stringify(manifest, null, 2) + '\n');
            if (manifest.commands.length % 100 === 0) console.error(JSON.stringify(manifest.outcomes));
        }
        console.error(`Decimal/math run ${run} complete: ${JSON.stringify(manifest.outcomes)}`);
    }
    console.error('Decimal/math baseline saved to ' + output);
    process.exit(0);
}
if (bigintSuite) {
    const { makeCases } = require('./bigint_cases');
    const rows = makeCases(quick, factorialOnly);
    manifest.factorialOnly = factorialOnly;
    manifest.fixtureCount = rows.length;
    manifest.caseTimeoutMs = caseTimeout;
    const fixtures = path.join(output, 'bigint-cases.tsv');
    fs.writeFileSync(fixtures, rows.join('\n') + '\n');
    for (let run = 1; run <= 3; run++) for (const row of rows) {
        const name = row.split('\t')[0];
        save(`bigint-${name}-run${run}`, binary('bigint_arithmetic_benchmark'),
            [fixtures, ...(quick ? ['--quick'] : []), '--case', name]);
    }
    console.error('BigInt baseline saved to ' + output);
    process.exit(0);
}
const cacheCases = ['legacy_cold', 'legacy_hit', 'legacy_precision', 'legacy_notation', 'legacy_angle',
    'legacy_expression', 'legacy_stale', 'legacy_approx_precision', 'session_cold', 'session_hit',
    'session_commit', 'session_random', 'rescale_fresh', 'rescale_reused'];
const formatCases = ['bigint_full', 'scientific_10', 'scientific_100', 'scientific_full', 'fixed_10',
    'auto_10', 'auto_full', 'mode_scientific_10', 'mode_scientific_full'];
const sizes = quick ? [16, 256, 1024] : [16, 64, 256, 1024, 4096, 16384];
for (let run = 1; run <= 3; run++) {
    const flags = quick ? ['--quick'] : [];
    for (const operation of formatCases) for (const digits of sizes)
        save(`format-${operation}-${digits}-run${run}`, binary('decimal_format_benchmark'),
            [...flags, '--case', operation, '--digits', String(digits)]);
    for (const edge of ['zero', 'negative', 'scale_positive', 'scale_negative', 'carry', 'tie_even', 'tie_odd', 'sticky'])
        save(`format-${edge}-run${run}`, binary('decimal_format_benchmark'), [...flags, '--case', edge]);
    for (const name of cacheCases)
        save(`cache-${name}-run${run}`, binary('cache_memory_benchmark'), [...flags, '--case', name]);
    save(`http-run${run}`, process.execPath,
        [path.join(root, 'benchmarks', 'http_benchmark.js'), binary('numforge_web'), ...flags]);
}
console.error('Baseline saved to ' + output);
