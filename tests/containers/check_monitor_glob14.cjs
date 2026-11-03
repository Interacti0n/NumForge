const test = require('node:test'), assert = require('node:assert/strict');
const fs = require('node:fs'), os = require('node:os'), path = require('node:path'), crypto = require('node:crypto');
const {createRequire} = require('node:module');
const {pathToFileURL} = require('node:url');
const app = createRequire('/app/package.json'), owner = createRequire(app.resolve('redbean-node/package.json'));
const guards = globalThis.monitorPatchGuards;
const hash = value => crypto.createHash('sha256').update(value).digest('hex');
const write = (file, value) => {fs.mkdirSync(path.dirname(file), {recursive: true}); fs.writeFileSync(file, typeof value === 'string' ? value : JSON.stringify(value));};
function directory(t) {const root = fs.mkdtempSync(path.join(os.tmpdir(), 'nf-glob14-')); t.after(() => fs.rmSync(root, {recursive: true, force: true})); return root;}

if (guards) {
    function fixture(t) {
        const root = directory(t), entries = {}, snapshot = path.join(root, 'before.json');
        function pkg(location, name, version, integrity, dependencies = {}, optionalDependencies) {
            write(path.join(root, location, 'package.json'), {name, version, dependencies, ...(optionalDependencies ? {optionalDependencies} : {})});
            entries[location] = {version, integrity, resolved: `https://registry.npmjs.org/${name}/-/${name.split('/').at(-1)}-${version}.tgz`};
        }
        function lock() {write(path.join(root, 'package-lock.json'), {packages: entries}); write(path.join(root, 'node_modules/.package-lock.json'), {packages: entries});}
        write(path.join(root, 'package.json'), {version: '2.5.9', dependencies: {owner: '^1.0.0'}});
        pkg('node_modules/owner', 'owner', '1.0.0', 'owner', {glob: '^10.0.0'});
        pkg('node_modules/owner/node_modules/glob', 'glob', '10.3.16', 'old-glob', {minimatch: '^9.0.0', old: '^1.0.0'});
        pkg('node_modules/minimatch', 'minimatch', '9.0.3', 'old-mm', {oldBrace: '^2.0.0'});
        pkg('node_modules/oldBrace', 'oldBrace', '2.0.0', 'old-brace');
        pkg('node_modules/old', 'old', '1.0.0', 'old', {leaf: '^1.0.0'}, {optionalLeaf: '^1.0.0'});
        pkg('node_modules/old/node_modules/leaf', 'leaf', '1.0.0', 'leaf');
        pkg('node_modules/optionalLeaf', 'optionalLeaf', '1.0.0', 'optional');
        fs.symlinkSync('/app/node_modules/semver', path.join(root, 'node_modules/semver')); lock();
        const glob = {name: 'glob', from: '10.3.16', version: '14.0.2', integrity: 'new-glob', before: 'node_modules/owner/node_modules/glob', after: 'node_modules/owner/node_modules/glob'};
        const minimatch = {name: 'minimatch', from: '9.0.3', version: '11.0.1', integrity: 'new-mm', before: 'node_modules/minimatch', after: 'node_modules/owner/node_modules/minimatch', ownedBy: 'glob'};
        const replacement = 'module.exports = require("./index.js");\n', source = glob.after + '/dist/commonjs/index.min.js';
        const config = {upstreamVersion: '2.5.9', keepAbsent: [], packages: [glob, minimatch],
            overrides: {owner: {glob: {'.': '14.0.2', minimatch: {'.': '11.0.1', newBrace: '5.0.0'}}}},
            consumerMigrations: [{consumer: 'node_modules/owner', consumerVersion: '1.0.0', consumerIntegrity: 'owner', name: 'glob', fromRange: '^10.0.0', version: '14.0.2', compatibility: 'consumer-major-upgrade',
                dependencyMigration: {from: {minimatch: '^9.0.0', old: '^1.0.0'}, to: {minimatch: '^11.0.0'}}}],
            addedDependencies: [{name: 'newBrace', path: 'node_modules/owner/node_modules/newBrace', version: '5.0.0', integrity: 'new-brace', addedBy: 'minimatch', scope: 'owner'}],
            prunedDependencies: [
                {name: 'oldBrace', path: 'node_modules/oldBrace', version: '2.0.0', integrity: 'old-brace', removedBy: 'minimatch'},
                {name: 'old', path: 'node_modules/old', version: '1.0.0', integrity: 'old', removedBy: 'glob'},
                {name: 'leaf', path: 'node_modules/old/node_modules/leaf', version: '1.0.0', integrity: 'leaf', removedBy: 'old'},
                {name: 'optionalLeaf', path: 'node_modules/optionalLeaf', version: '1.0.0', integrity: 'optional', removedBy: 'old', dependencyKind: 'optionalDependencies'}],
            sourcePatches: [{path: source, stage: 'installed', owner: {name: 'glob', path: glob.after, version: glob.version, integrity: glob.integrity}, beforeHash: hash('ORIGINAL_BUNDLE'), afterHash: hash(replacement), replacement}]};
        function install() {
            for (const location of [minimatch.before, ...config.prunedDependencies.map(p => p.path)]) {
                fs.rmSync(path.join(root, location), {recursive: true, force: true}); delete entries[location];
            }
            pkg(glob.after, 'glob', glob.version, glob.integrity, {minimatch: '^11.0.0'});
            pkg(minimatch.after, 'minimatch', minimatch.version, minimatch.integrity, {newBrace: '^5.0.0'});
            pkg(config.addedDependencies[0].path, 'newBrace', '5.0.0', 'new-brace');
            write(path.join(root, source), 'ORIGINAL_BUNDLE'); lock();
        }
        return {root, config, snapshot, pkg, lock, install, source, replacement};
    }
    test('owned major dependency, scoped helper, transitive/optional pruning and installed source adaptation remain bounded', t => {
        const f = fixture(t); guards.prepare(f.root, f.config, f.snapshot); f.install(); guards.verify(f.root, f.config, f.snapshot);
        assert.equal(fs.readFileSync(path.join(f.root, f.source), 'utf8'), f.replacement);
    });
    test('owned dependency major cannot bypass an unrelated consumer', t => {
        const f = fixture(t); f.pkg('node_modules/unrelated', 'unrelated', '1.0.0', 'unrelated', {minimatch: '^9.0.0'}); f.lock();
        const before = fs.readFileSync(path.join(f.root, 'package.json'));
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /Unsupported range/);
        assert.deepEqual(fs.readFileSync(path.join(f.root, 'package.json')), before);
    });
    test('scoped helper cannot escape its qualified consumer', t => {
        const f = fixture(t); f.config.addedDependencies[0].scope = 'other';
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /qualified consumer/);
    });
    test('scoped helper needs explicit ownership through the migrated parent', t => {
        const f = fixture(t); delete f.config.packages[1].ownedBy;
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /qualified consumer/);
    });
    test('pruned dependency must resolve at the declared original path', t => {
        const f = fixture(t); Object.assign(f.config.prunedDependencies[2], {path: 'node_modules/optionalLeaf', integrity: 'optional'});
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /pruned dependency resolution/);
    });
    test('transitive pruning rejects a cycle without a migrated root', t => {
        const f = fixture(t); f.config.prunedDependencies[1].removedBy = 'leaf';
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /ownership cycle/);
    });
    test('optional pruning must declare the original dependency kind', t => {
        const f = fixture(t); delete f.config.prunedDependencies[3].dependencyKind;
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /not an old owner dependency/);
    });
    for (const mutation of ['identity', 'path', 'stage']) {
        test('installed source adaptation rejects an unqualified ' + mutation, t => {
            const f = fixture(t), patch = f.config.sourcePatches[0];
            if (mutation === 'identity') patch.owner.integrity = 'changed';
            if (mutation === 'path') patch.path = '../outside.js';
            if (mutation === 'stage') patch.stage = 'arbitrary';
            assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /pinned package|escapes|Unknown source/);
        });
    }
    for (const mutation of ['preimage', 'replacement']) {
        test('installed source adaptation rejects a changed ' + mutation + ' before writing', t => {
            const f = fixture(t); guards.prepare(f.root, f.config, f.snapshot); f.install();
            if (mutation === 'preimage') write(path.join(f.root, f.source), 'ALTERED_BUNDLE');
            else f.config.sourcePatches[0].afterHash = '0'.repeat(64);
            const before = fs.readFileSync(path.join(f.root, f.source));
            assert.throws(() => guards.verify(f.root, f.config, f.snapshot));
            assert.deepEqual(fs.readFileSync(path.join(f.root, f.source)), before);
        });
    }
    test('removing a declared parent cannot silently remove an undeclared descendant', t => {
        const f = fixture(t), leaf = f.config.prunedDependencies[2];
        f.config.prunedDependencies.splice(2, 1); guards.prepare(f.root, f.config, f.snapshot); f.install();
        fs.rmSync(path.join(f.root, leaf.path), {recursive: true, force: true});
        const lock = JSON.parse(fs.readFileSync(path.join(f.root, 'package-lock.json'))); delete lock.packages[leaf.path];
        write(path.join(f.root, 'package-lock.json'), lock); write(path.join(f.root, 'node_modules/.package-lock.json'), lock);
        assert.throws(() => guards.verify(f.root, f.config, f.snapshot), /Unrelated package changed/);
    });
}

if (owner('glob/package.json').version === '14.0.2' || process.env.NUMFORGE_FORCE_GLOB14_CHECKS) {
    const glob = owner('glob'), globRequire = createRequire(owner.resolve('glob/package.json'));
    const pattern = Array.from({length: 10}, (_, i) => '**/../x' + i + '/').join('') + 'target';
    test('modern branch pins glob 14 and minimatch 11 while keeping the legacy callable matcher', () => {
        assert.equal(owner('glob/package.json').version, '14.0.2');
        assert.equal(globRequire('minimatch/package.json').version, '11.0.1');
        assert.equal(globRequire('path-scurry/package.json').version, '3.0.0');
        const legacy = createRequire(app.resolve('glob/package.json'));
        assert.equal(legacy('minimatch/package.json').version, '3.1.5');
        assert.equal(typeof legacy('minimatch'), 'function');
    });
    test('default CJS and ESM entrypoints use the fixed raw dependency graph', async () => {
        assert.equal(glob, owner('glob/raw'));
        const root = path.dirname(owner.resolve('glob/package.json'));
        const esm = await import(pathToFileURL(path.join(root, 'dist/esm/index.min.js')).href);
        const raw = await import(pathToFileURL(path.join(root, 'dist/esm/index.js')).href);
        assert.deepEqual(Object.keys(esm).sort(), Object.keys(glob).sort());
        for (const name of Object.keys(raw)) assert.equal(esm[name], raw[name], name);
        assert.deepEqual(esm.globSync('absent-pattern-*', {cwd: '/tmp'}), []);
    });
    for (const limit of [8, 32]) {
        test('minimatch bounds optimizer amplification at an explicit budget ' + limit, () => {
            const mm = globRequire('minimatch');
            const result = new mm.Minimatch(pattern, {optimizationLevel: 2, braceExpandMax: limit});
            assert(result.globParts.length <= limit, 'optimizer exceeded its pattern budget');
            assert.equal(result.match('unrelated-file'), false);
            assert.equal(mm.minimatch('model/a.js', '**/*.js'), true);
        });
    }
    test('default glob constructor respects the matcher amplification budget', t => {
        const walk = new glob.Glob(pattern, {cwd: directory(t), optimizationLevel: 2, braceExpandMax: 8});
        assert(walk.patterns.length <= 8, 'default export still uses the obsolete bundled matcher');
    });
    function tree(t) {
        const root = directory(t);
        for (let a = 0; a < 4; a++) for (let b = 0; b < 4; b++) for (let c = 0; c < 4; c++) fs.mkdirSync(path.join(root, '' + a, '' + b, '' + c), {recursive: true});
        return root;
    }
    for (const stream of [false, true]) {
        test('aborted ' + (stream ? 'stream' : 'promise') + ' walk stops issuing filesystem reads', async t => {
            const root = tree(t), controller = new AbortController(), reason = new Error('fixture abort');
            let calls = 0, atReject;
            const io = {...fs, readdir(file, options, callback) {
                calls++; fs.readdir(file, options, (error, entries) => {if (calls === 1) controller.abort(reason); callback(error, entries);});
            }};
            try {
                const options = {cwd: root, fs: io, signal: controller.signal};
                if (stream) {for await (const item of glob.globStream('**/*', options)) {assert.equal(typeof item, 'string');}}
                else await glob.glob('**/*', options);
                assert.fail('walk should reject cancellation');
            } catch (error) {assert.equal(error, reason); atReject = calls;}
            await new Promise(resolve => setTimeout(resolve, 150));
            assert(calls >= 1); assert.equal(calls, atReject, 'filesystem work continued after rejection');
        });
    }
    test('a pre-aborted walk does not read the filesystem', async t => {
        const controller = new AbortController(), reason = new Error('already aborted'); controller.abort(reason);
        let calls = 0;
        const io = {...fs, readdir(...args) {calls++; fs.readdir(...args);}};
        await assert.rejects(Promise.resolve().then(() => glob.glob('**/*', {cwd: directory(t), signal: controller.signal, fs: io})), error => error === reason);
        assert.equal(calls, 0);
    });
    test('actual RedBean model discovery preserves JS/CommonJS/default exports and excludes hidden/nested files', t => {
        const root = directory(t), bean = app.resolve('redbean-node/dist/bean-model.js');
        const model = 'class Fixture extends require(' + JSON.stringify(bean) + ').BeanModel {}';
        write(path.join(root, 'first.js'), 'module.exports = ' + model);
        write(path.join(root, 'výpočet.js'), 'exports.default = ' + model);
        for (const name of ['.hidden.js', 'ignored.txt', 'nested/ignored.js']) write(path.join(root, name), 'throw new Error("must not be loaded")');
        const store = new (app('redbean-node').RedBeanNode)(); store.autoloadModels(root, 'js');
        assert.deepEqual(Object.keys(store.modelList).sort(), ['first', 'výpočet']);
    });
    test('legacy glob callback and synchronous APIs retain their separate dependency branch', async t => {
        const root = directory(t); write(path.join(root, 'fixture.txt'), 'fixture');
        const legacy = app('glob');
        const result = await new Promise((resolve, reject) => legacy('*.txt', {cwd: root}, (error, rows) => error ? reject(error) : resolve(rows)));
        assert.deepEqual(result, ['fixture.txt']); assert.deepEqual(legacy.sync('*.txt', {cwd: root}), ['fixture.txt']);
    });
}
