const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const { spawnSync } = require('node:child_process');
const root = path.resolve(__dirname, '..');
const args = process.argv.slice(2);
const binaryDirectory = args.shift();
const outputDirectory = args.shift();
const quick = args.includes('--quick');
if (!binaryDirectory || !outputDirectory || args.some(v => v !== '--quick'))
    throw Error('Usage: node benchmarks/run_baseline.js binary-directory output-directory [--quick]');
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
function capture(command, arguments_) {
    const result = spawnSync(command, arguments_, { cwd: root, env, encoding: 'utf8',
        windowsHide: true, maxBuffer: 16 * 1024 * 1024 });
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
    mode: quick ? 'quick harness smoke' : 'full diagnostic baseline', repetitions: 3,
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
function save(name, command, parameters) {
    console.error(name);
    const begin = Date.now();
    const result = capture(command, parameters);
    fs.writeFileSync(path.join(output, name + '.csv'), result.stdout);
    fs.writeFileSync(path.join(output, name + '.stderr.txt'), result.stderr);
    manifest.commands.push({ name, command, parameters, elapsedMs: Date.now() - begin });
    fs.writeFileSync(path.join(output, 'manifest.json'), JSON.stringify(manifest, null, 2) + '\n');
}
save('allocation-check', binary('allocation_stats_check'), []);
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
