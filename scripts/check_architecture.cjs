// Enforce source-level dependencies without requiring a compiler or generated files.
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
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
// The Python tree can contain local build output: inspect its adapter only.
sources.push('bindings/python/native.c');
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
            if (target.startsWith('../') || target.startsWith('./')) {
                errors.push(`${name}: use a repository-qualified private include: ${target}`);
            }
            continue;
        }
        if (!fs.existsSync(path.join(root, target))) errors.push(`${name}: missing ${target}`);
        const owner = layerOf(name);
        // Opt-in decimal instrumentation inspects limb storage for measurements.
        const probe = name === 'src/runtime/instrumentation/decimal_profile.c' && dependency === 1;
        if (owner >= 0 && !allowed[owner].has(dependency) && !probe) {
            errors.push(`${name}: forbidden upward dependency on ${target}`);
        }
    }
}
// Every mathematical module owns an explicit manifest. No implicit source glob.
const moduleIndex = fs.readFileSync(path.join(root, 'src/modules/CMakeLists.txt'), 'utf8');
const registeredModules = new Set([...moduleIndex.matchAll(/add_subdirectory\(\s*"?([^"\s)]+)"?\s*\)/g)]
    .map(match => match[1]));
for (const entry of fs.readdirSync(path.join(root, 'src/modules'), {withFileTypes: true})) {
    if (!entry.isDirectory()) continue;
    const directory = 'src/modules/' + entry.name;
    if (!registeredModules.has(entry.name)) errors.push(`${directory}: absent from module index`);
    const manifest = path.join(root, directory, 'CMakeLists.txt');
    if (!fs.existsSync(manifest)) { errors.push(`${directory}: missing CMakeLists.txt`); continue; }
    const text = fs.readFileSync(manifest, 'utf8');
    for (const name of walk(directory).filter(n => n.endsWith('.c'))) {
        const relative = path.posix.relative(directory, name);
        if (!text.split(/\s+/).includes(relative)) errors.push(`${name}: absent from module manifest`);
    }
}
if (errors.length) { console.error(errors.join('\n')); process.exitCode = 1; }
else console.log(`Architecture checked: ${sources.length} implementation files; public headers and module manifests.`);
