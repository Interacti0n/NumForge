// Enforce source-level dependencies without requiring a compiler or generated files.
const fs = require('node:fs');
const path = require('node:path');
function checkArchitecture(root) {
    const layers = ['src/runtime', 'src/core', 'src/modules', 'src/engine',
        'src/application', 'apps', 'bindings'];
    const allowed = [new Set([0]), new Set([0, 1]), new Set([0, 1, 2]),
        new Set([0, 1, 2, 3]), new Set([0, 1, 2, 3, 4]),
        new Set([0, 1, 2, 3, 4, 5]), new Set([0, 1, 2, 3, 6])];
    const errors = [];
    function walk(directory) {
        return fs.readdirSync(path.join(root, directory), {withFileTypes: true}).flatMap(entry => {
            const name = directory + '/' + entry.name;
            if (entry.name.startsWith('.') || entry.name === '__pycache__') return [];
            return entry.isDirectory() ? walk(name) : [name];
        });
    }
    const layerOf = name => layers.findIndex(prefix => name.startsWith(prefix + '/'));
    const sources = [...layers.slice(0, 5).flatMap(walk), ...walk('apps')]
        .filter(name => /\.[ch]$/.test(name));
    // Native adapters live at this level; nested Python caches/builds are not sources.
    const pythonSources = fs.readdirSync(path.join(root, 'bindings/python'), {withFileTypes: true})
        .filter(entry => entry.isFile() && /\.[ch]$/.test(entry.name))
        .map(entry => 'bindings/python/' + entry.name);
    sources.push(...pythonSources);
    const privateHeaders = new Set(sources.filter(n => n.endsWith('.h')).map(n => path.posix.basename(n)));
    for (const name of [...sources, ...walk('include/numforge').filter(n => n.endsWith('.h'))]) {
        const text = fs.readFileSync(path.join(root, name), 'utf8');
        for (const match of text.matchAll(/^\s*#\s*include\s*["<]([^">]+)[">]/gm)) {
            const target = match[1];
            if (name.startsWith('include/') && /^(src|apps|bindings)\//.test(target)) {
                errors.push(`${name}: public header includes private ${target}`);
            }
            // Public headers are callable from every layer; their own private leaks
            // are checked above. System and generated headers are compiler checked.
            const dependency = layerOf(target);
            if (dependency < 0) {
                if (target.startsWith('../') || target.startsWith('./') || privateHeaders.has(target)) {
                    errors.push(`${name}: use a repository-qualified private include: ${target}`);
                }
                continue;
            }
            if (!fs.existsSync(path.join(root, target))) errors.push(`${name}: missing ${target}`);
            if (name.startsWith('bindings/python/') && target.startsWith('src/')) {
                errors.push(`${name}: Python adapters must use public numeric/engine headers, not ${target}`);
            }
            if (!name.startsWith('src/core/') && target.startsWith('src/core/') && target.endsWith('_internal.h')) {
                const exactRoot = target === 'src/core/bigrational/sqrt_internal.h' &&
                    (name === 'src/modules/algebra/polynomial.c' || name === 'src/engine/evaluator/exact_functions.c');
                const probe = name === 'src/runtime/instrumentation/decimal_profile.c';
                if (!exactRoot && !probe) errors.push(`${name}: core storage is private; use public numeric APIs`);
            }
            if (target === 'src/engine/evaluator/evaluator_internal.h' && !name.startsWith('src/engine/')) {
                errors.push(`${name}: evaluator storage is engine-private; use an engine API header`);
            }
            if (name.startsWith('apps/') && /^src\/engine\/(parser|values|formatting|evaluator)\//.test(target) &&
                target !== 'src/engine/evaluator/functions.h') {
                errors.push(`${name}: transport must use engine/application APIs, not ${target}`);
            }
            const module = /^src\/modules\/([^/]+)\//.exec(target);
            if (module && !name.startsWith(`src/modules/${module[1]}/`)) {
                // Private method comparisons share callback hooks, never result storage.
                const research = name === 'src/application/equations/numeric_root.c' &&
                    target === 'src/modules/numerical/root_strategy_internal.h';
                if (!research) errors.push(`${name}: module-private dependency on ${target}; use its public API`);
            }
            const owner = layerOf(name);
            // Opt-in decimal instrumentation inspects limb storage for measurements.
            const probe = name === 'src/runtime/instrumentation/decimal_profile.c' && dependency === 1;
            if (owner >= 0 && !allowed[owner].has(dependency) && !probe) {
                errors.push(`${name}: forbidden upward dependency on ${target}`);
            }
        }
    }
    // Every Python adapter source is registered explicitly; headers remain private.
    function manifestTokens(file) {
        return fs.readFileSync(path.join(root, file), 'utf8').replace(/#[^\r\n]*/g, '')
            .replace(/[()"]/g, ' ').split(/\s+/);
    }
    const registeredSources = new Set();
    function checkManifest(directory, files, label) {
        const manifest = directory + '/CMakeLists.txt';
        if (!fs.existsSync(path.join(root, manifest))) { errors.push(`${directory}: missing CMakeLists.txt`); return; }
        const tokens = new Set(manifestTokens(manifest));
        for (const name of files) {
            registeredSources.add(name);
            if (!tokens.has(path.posix.relative(directory, name))) errors.push(`${name}: absent from ${label} manifest`);
        }
        for (const token of tokens) {
            if (token.endsWith('.c') && !fs.existsSync(path.join(root, directory, token)))
                errors.push(`${manifest}: missing registered source ${token}`);
        }
    }
    checkManifest('bindings/python', pythonSources.filter(n => n.endsWith('.c')), 'Python adapter');
    // Cover the remaining implementation manifests too, not just mathematical modules.
    for (const directory of ['src/runtime', 'src/core', 'src/engine', 'src/application',
        'apps/cli', 'apps/web/http', 'apps/web/server']) {
        checkManifest(directory, sources.filter(n => n.startsWith(directory + '/') && n.endsWith('.c') &&
            !n.startsWith('src/runtime/instrumentation/')), 'implementation');
    }
    // Instrumentation is owned by the explicitly registered opt-in benchmark manifest.
    const rootManifest = new Set(manifestTokens('CMakeLists.txt'));
    const instrumentationManifest = new Set(rootManifest);
    if (rootManifest.has('benchmarks') && fs.existsSync(path.join(root, 'benchmarks/CMakeLists.txt'))) {
        for (const token of manifestTokens('benchmarks/CMakeLists.txt'))
            instrumentationManifest.add(token.replace(/^\$\{PROJECT_SOURCE_DIR\}\//, ''));
    }
    for (const name of sources.filter(n => n.startsWith('src/runtime/instrumentation/') && n.endsWith('.c'))) {
        registeredSources.add(name);
        if (!instrumentationManifest.has(name)) errors.push(`${name}: absent from benchmark instrumentation manifest`);
    }
    // Every mathematical module owns an explicit manifest. No implicit source glob.
    const moduleIndex = fs.readFileSync(path.join(root, 'src/modules/CMakeLists.txt'), 'utf8').replace(/#[^\r\n]*/g, '');
    const registeredModules = new Set([...moduleIndex.matchAll(/add_subdirectory\(\s*"?([^"\s)]+)"?\s*\)/g)]
        .map(match => match[1]));
    for (const entry of fs.readdirSync(path.join(root, 'src/modules'), {withFileTypes: true})) {
        if (!entry.isDirectory()) continue;
        const directory = 'src/modules/' + entry.name;
        if (!registeredModules.has(entry.name)) errors.push(`${directory}: absent from module index`);
        checkManifest(directory, walk(directory).filter(n => n.endsWith('.c')), 'module');
    }
    for (const name of sources.filter(n => n.endsWith('.c'))) {
        if (!registeredSources.has(name)) errors.push(`${name}: no owning source manifest`);
    }
    return {errors, count: sources.length};
}

module.exports = {checkArchitecture};
if (require.main === module) {
    const {errors, count} = checkArchitecture(path.resolve(__dirname, '..'));
    if (errors.length) { console.error(errors.join('\n')); process.exitCode = 1; }
    else console.log(`Architecture checked: ${count} implementation files; public headers and source manifests.`);
}
