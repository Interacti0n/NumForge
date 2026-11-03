// Independent archive bytes and disposable files; no host mount or download.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const zlib = require('node:zlib');
const {Readable} = require('node:stream');
const {pipeline} = require('node:stream/promises');
const {spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');
const appRequire = createRequire('/app/package.json');
const receipt = '/opt/numforge/monitor-patches.json';
const manifest = fs.existsSync(receipt) ? JSON.parse(fs.readFileSync(receipt, 'utf8')) : {};
const guards = globalThis.monitorPatchGuards;

function directory(t) {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'nf-tar-'));
    t.after(() => fs.rmSync(root, {recursive: true}));
    return root;
}
const write = (file, value) => {fs.mkdirSync(path.dirname(file), {recursive: true}); fs.writeFileSync(file, JSON.stringify(value));};
function guardFixture(t) {
    const root = directory(t), snapshot = path.join(root, 'before.json'), packages = {};
    function installPackage(location, name, version, integrity, dependencies) {
        write(path.join(root, location, 'package.json'), {name, version, ...(dependencies ? {dependencies} : {})});
        packages[location] = {version, integrity, resolved: `https://registry.npmjs.org/${name}/-/${name.split('/').at(-1)}-${version}.tgz`};
    }
    function lock() {write(path.join(root, 'package-lock.json'), {packages}); write(path.join(root, 'node_modules/.package-lock.json'), {packages});}
    write(path.join(root, 'package.json'), {version: '2.5.9', dependencies: {tar: '~6.2.1'}});
    installPackage('node_modules/tar', 'tar', '6.2.1', 'old-tar', {minipass: '^5.0.0'});
    installPackage('node_modules/tar/node_modules/minipass', 'minipass', '5.0.0', 'old-local');
    installPackage('node_modules/native', 'native', '1.0.0', 'native', {tar: '^6.1.11'});
    installPackage('node_modules/minipass', 'minipass', '3.3.6', 'root-minipass', {yallist: '^4.0.0'});
    installPackage('node_modules/yallist', 'yallist', '4.0.0', 'root-yallist');
    fs.symlinkSync('/app/node_modules/semver', path.join(root, 'node_modules/semver'));
    lock();
    const replacement = {name: 'minipass', path: 'node_modules/tar/node_modules/minipass', version: '7.1.3', integrity: 'new-local',
        addedBy: 'tar', replaces: {version: '5.0.0', integrity: 'old-local'}};
    const consumer = {consumer: 'node_modules/native', consumerVersion: '1.0.0', consumerIntegrity: 'native',
        name: 'tar', fromRange: '^6.1.11', version: '7.5.22', compatibility: 'stable-major-upgrade'};
    const config = {upstreamVersion: '2.5.9', keepAbsent: [],
        directMigrations: [{name: 'tar', fromRange: '~6.2.1', version: '7.5.22', compatibility: 'stable-major-upgrade'}],
        consumerMigrations: [consumer], addedDependencies: [replacement],
        overrides: {native: {tar: '7.5.22'}, 'tar@7.5.22': {minipass: '7.1.3'}},
        packages: [{name: 'tar', from: '6.2.1', version: '7.5.22', before: 'node_modules/tar', after: 'node_modules/tar', integrity: 'new-tar'}]};
    function install() {
        installPackage('node_modules/tar', 'tar', '7.5.22', 'new-tar', {minipass: '^7.1.2'});
        installPackage(replacement.path, 'minipass', '7.1.3', 'new-local'); lock();
    }
    return {root, snapshot, config, replacement, consumer, install, installPackage, lock, packages};
}

if (guards && fs.existsSync(receipt)) {
    test('consumer major migration leaves nested packages for npm and preserves unrelated helper identities', t => {
        const f = guardFixture(t);
        guards.prepare(f.root, f.config, f.snapshot);
        assert.equal(JSON.parse(fs.readFileSync(path.join(f.root, f.replacement.path, 'package.json'))).version, '5.0.0');
        f.install(); guards.verify(f.root, f.config, f.snapshot);
        assert.equal(JSON.parse(fs.readFileSync(path.join(f.root, 'node_modules/minipass/package.json'))).version, '3.3.6');
        assert.equal(JSON.parse(fs.readFileSync(path.join(f.root, 'node_modules/yallist/package.json'))).version, '4.0.0');
    });
    test('consumer major migration requires the matching explicit direct policy', t => {
        const f = guardFixture(t); f.config.directMigrations = [];
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /same qualified direct migration/);
    });
    test('unknown consumer policies cannot relax the stable-major rule', t => {
        const f = guardFixture(t); f.consumer.compatibility = 'unqualified';
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /Unknown consumer compatibility policy/);
    });
    test('major consumer policy cannot remove a dependency tree through lock refresh', t => {
        const f = guardFixture(t); f.consumer.dependencyRefresh = {from: {}, to: {}};
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /let npm reify/);
    });
    for (const [field, bad] of [['version', '4.0.0'], ['integrity', 'unexpected']]) {
        test('replacement rejects changed old ' + field + ' before writing', t => {
            const f = guardFixture(t); f.replacement.replaces[field] = bad;
            const original = fs.readFileSync(path.join(f.root, 'package.json'));
            assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /Replaced dependency (version|integrity) changed/);
            assert.deepEqual(fs.readFileSync(path.join(f.root, 'package.json')), original);
        });
    }
    test('an installed addition needs an explicit replacement identity', t => {
        const f = guardFixture(t); delete f.replacement.replaces;
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /already exists/);
    });
    test('replacement cannot change a shared root helper', t => {
        const f = guardFixture(t); f.replacement.path = 'node_modules/minipass';
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /owner-local dependency/);
    });
    test('replacement cannot delete nested packages or migrate another consumer implicitly', t => {
        const f = guardFixture(t);
        f.installPackage(f.replacement.path + '/node_modules/child', 'child', '1.0.0', 'child'); f.lock();
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /cannot remove nested packages/);
        assert(fs.existsSync(path.join(f.root, f.replacement.path, 'node_modules/child/package.json')));
        fs.rmSync(path.join(f.root, f.replacement.path, 'node_modules'), {recursive: true});
        delete f.packages[f.replacement.path + '/node_modules/child'];
        f.installPackage('node_modules/tar/node_modules/other', 'other', '1.0.0', 'other', {minipass: '^5.0.0'}); f.lock();
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /another installed consumer/);
    });
    test('version-qualified helper override still requires its exact reviewed pin', t => {
        const f = guardFixture(t); f.config.overrides['tar@7.5.22'].minipass = '7.1.2';
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /pinned exactly/);
    });
    test('a new helper may own only declared, pinned transitive dependencies', t => {
        const f = guardFixture(t);
        f.config.addedDependencies.push({name: '@scope/fs', path: 'node_modules/@scope/fs', version: '4.0.1', integrity: 'fs', addedBy: 'tar'},
            {name: 'minipass', path: 'node_modules/@scope/fs/node_modules/minipass', version: '7.1.3', integrity: 'nested', addedBy: '@scope/fs'});
        f.config.overrides['@scope/fs'] = {'.': '4.0.1', minipass: '7.1.3'};
        guards.prepare(f.root, f.config, f.snapshot); f.install();
        f.installPackage('node_modules/tar', 'tar', '7.5.22', 'new-tar', {minipass: '^7.1.2', '@scope/fs': '^4.0.0'});
        f.installPackage('node_modules/@scope/fs', '@scope/fs', '4.0.1', 'fs', {minipass: '^7.0.4'});
        f.installPackage('node_modules/@scope/fs/node_modules/minipass', 'minipass', '7.1.3', 'nested'); f.lock();
        guards.verify(f.root, f.config, f.snapshot);
    });
    test('added dependency ownership must terminate at an explicit migration', t => {
        const f = guardFixture(t);
        f.config.addedDependencies.push({name: 'first', path: 'node_modules/first', version: '1.0.0', addedBy: 'second'},
            {name: 'second', path: 'node_modules/second', version: '1.0.0', addedBy: 'first'});
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /ownership cycle/);
    });
    for (const retainConsumer of [false, true]) {
        test('pruning a relocated helper ' + (retainConsumer ? 'rejects an unsatisfied consumer' : 'permits a compatible owner-local replacement'), t => {
            const f = guardFixture(t);
            f.installPackage('node_modules/tar', 'tar', '6.2.1', 'old-tar', {minipass: '^5.0.0', helper: '^2.0.0'});
            f.installPackage('node_modules/helper', 'helper', '2.0.0', 'old-helper');
            if (retainConsumer) f.installPackage('node_modules/native', 'native', '1.0.0', 'native', {tar: '^6.1.11', helper: '^2.0.0'});
            f.lock();
            f.config.prunedDependencies = [{name: 'helper', path: 'node_modules/helper', version: '2.0.0', integrity: 'old-helper', removedBy: 'tar'}];
            f.config.addedDependencies.push({name: 'helper', path: 'node_modules/tar/node_modules/helper', version: '3.0.0', integrity: 'new-helper', addedBy: 'tar'});
            f.config.overrides['tar@7.5.22'].helper = '3.0.0';
            guards.prepare(f.root, f.config, f.snapshot); f.install();
            fs.rmSync(path.join(f.root, 'node_modules/helper'), {recursive: true}); delete f.packages['node_modules/helper'];
            f.installPackage('node_modules/tar', 'tar', '7.5.22', 'new-tar', {minipass: '^7.1.2', helper: '^3.0.0'});
            f.installPackage('node_modules/tar/node_modules/helper', 'helper', '3.0.0', 'new-helper'); f.lock();
            if (retainConsumer) assert.throws(() => guards.verify(f.root, f.config, f.snapshot), /Cannot resolve helper|Removed package still required/);
            else guards.verify(f.root, f.config, f.snapshot);
        });
    }
}

if (manifest.directMigrations?.some(p => p.name === 'tar') || process.env.NUMFORGE_FORCE_TAR_CHECKS) {
    const tar = appRequire('tar');
    function member(name, body = Buffer.alloc(0), type = '0', link = '') {
        body = Buffer.from(body);
        const header = Buffer.alloc(512);
        const field = (offset, size, value) => header.write(value, offset, size, 'utf8');
        field(0, 100, name); field(100, 8, '0000644\0'); field(108, 8, '0000000\0'); field(116, 8, '0000000\0');
        field(124, 12, body.length.toString(8).padStart(11, '0') + '\0'); field(136, 12, '00000000000\0');
        header.fill(32, 148, 156); field(156, 1, type); field(157, 100, link); field(257, 6, 'ustar\0'); field(263, 2, '00');
        const checksum = header.reduce((sum, byte) => sum + byte, 0);
        field(148, 8, checksum.toString(8).padStart(6, '0') + '\0 ');
        return Buffer.concat([header, body, Buffer.alloc((512 - body.length % 512) % 512)]);
    }
    const archive = (...members) => Buffer.concat([...members, Buffer.alloc(1024)]);
    function child(root, code, timeout = 2500) {
        const result = spawnSync(process.execPath, ['--max-old-space-size=96', '-e',
            "const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),tar=require('tar');const root=process.argv[1];" + code, root],
            {encoding: 'utf8', timeout, maxBuffer: 32768});
        assert.ifError(result.error); assert.equal(result.status, 0, result.stderr.slice(-2000));
    }

    test('all five reviewed consumers share the exact tar pin and keep root helpers unchanged', () => {
        assert.equal(appRequire('tar/package.json').version, '7.5.22');
        const file = appRequire.resolve('tar');
        for (const name of ['@louislam/sqlite3', '@mapbox/node-pre-gyp', 'node-gyp', 'cacache']) {
            assert.equal(createRequire(appRequire.resolve(name + '/package.json')).resolve('tar'), file);
        }
        for (const [name, version] of [['chownr', '2.0.0'], ['minipass', '3.3.6'], ['minizlib', '2.1.2'], ['yallist', '4.0.0'], ['mkdirp', '1.0.4']]) {
            assert.equal(appRequire(name + '/package.json').version, version, name);
        }
    });
    test('independent USTAR bytes list and extract through file and stream APIs', async t => {
        const root = directory(t), bytes = archive(member('pkg/first.txt', 'first\n'), member('pkg/nested/second.txt', 'Výpočet 😀'));
        const file = path.join(root, 'fixture.tgz'); fs.writeFileSync(file, zlib.gzipSync(bytes));
        const entries = []; await tar.t({file, onentry: entry => entries.push(entry.path)});
        assert.deepEqual(entries, ['pkg/first.txt', 'pkg/nested/second.txt']);
        for (const mode of ['file', 'stream']) {
            const cwd = path.join(root, mode); fs.mkdirSync(cwd);
            if (mode === 'file') await tar.x({file, cwd, strip: 1});
            else await pipeline(Readable.from([zlib.gzipSync(bytes)]), tar.x({cwd, strip: 1}));
            assert.equal(fs.readFileSync(path.join(cwd, 'first.txt'), 'utf8'), 'first\n');
            assert.equal(fs.readFileSync(path.join(cwd, 'nested/second.txt'), 'utf8'), 'Výpočet 😀');
        }
    });
    test('safe symlinks and hardlinks still extract inside the destination', async t => {
        const root = directory(t), file = path.join(root, 'links.tar'), cwd = path.join(root, 'out'); fs.mkdirSync(cwd);
        fs.writeFileSync(file, archive(member('original', 'fixture'), member('hard', '', '1', 'original'), member('symbolic', '', '2', 'original')));
        await tar.x({file, cwd});
        assert.equal(fs.readFileSync(path.join(cwd, 'hard'), 'utf8'), 'fixture');
        assert.equal(fs.readFileSync(path.join(cwd, 'symbolic'), 'utf8'), 'fixture');
        assert.equal(fs.statSync(path.join(cwd, 'hard')).ino, fs.statSync(path.join(cwd, 'original')).ino);
    });
    for (const sync of [false, true]) {
        test('hardlink traversal cannot expose a sibling fixture file: sync=' + sync, async t => {
            const root = directory(t), file = path.join(root, 'escape.tar'), cwd = path.join(root, 'out'); fs.mkdirSync(cwd);
            fs.writeFileSync(path.join(root, 'sentinel.txt'), 'DUMMY_SENTINEL');
            fs.writeFileSync(file, archive(member('nested/deeper/link', '', '1', '../sentinel.txt')));
            const warnings = [];
            await tar.x({file, cwd, sync, onwarn: code => warnings.push(code)});
            assert(!fs.existsSync(path.join(cwd, 'nested/deeper/link')));
            assert.equal(fs.readFileSync(path.join(root, 'sentinel.txt'), 'utf8'), 'DUMMY_SENTINEL');
            assert(warnings.length > 0);
        });
    }
    test('parent traversal and symlink pivot cannot overwrite a sibling sentinel', async t => {
        const root = directory(t), cwd = path.join(root, 'out'), outside = path.join(root, 'outside'); fs.mkdirSync(cwd); fs.mkdirSync(outside);
        fs.writeFileSync(path.join(outside, 'sentinel'), 'preserved');
        const file = path.join(root, 'pivot.tar');
        fs.writeFileSync(file, archive(member('../outside/sentinel', 'bad'), member('pivot', '', '2', '../outside'), member('pivot/sentinel', 'bad')));
        await tar.x({file, cwd, onwarn: () => {}});
        assert.equal(fs.readFileSync(path.join(outside, 'sentinel'), 'utf8'), 'preserved');
    });
    test('deep GNU long-name member selection terminates without an uncaught stack overflow', t => {
        const root = directory(t), file = path.join(root, 'deep.tgz');
        const bytes = archive(member('././@LongLink', 'a/'.repeat(12000) + 'end\0', 'L'), member('short', 'fixture'));
        fs.writeFileSync(file, zlib.gzipSync(bytes));
        child(root, "(async()=>{let count=0;await tar.t({file:path.join(root,'deep.tgz'),onentry:()=>count++},['selected']);assert.equal(count,0);})().catch(e=>{console.error(e);process.exitCode=1;});");
    });
    test('negative header size cannot hang archive replacement', t => {
        const root = directory(t), file = path.join(root, 'negative.tar');
        const bytes = archive(member('bad'));
        // Signed base-256, as used for negative numeric fields in GNU/PAX tar.
        bytes.fill(255, 124, 136); bytes[134] = 254; bytes[135] = 0;
        bytes.fill(32, 148, 156);
        const checksum = bytes.subarray(0, 512).reduce((sum, byte) => sum + byte, 0);
        bytes.write(checksum.toString(8).padStart(6, '0') + '\0 ', 148, 8, 'ascii');
        assert.equal(new tar.Header(bytes).cksumValid, true);
        fs.writeFileSync(file, bytes); fs.writeFileSync(path.join(root, 'good'), 'fixture');
        child(root, "(async()=>{const file=path.join(root,'negative.tar');try{await tar.r({file,cwd:root},['good']);}catch(e){assert.match(e.message,/negative|invalid|corrupt|malformed/i);return;}const entries=[];await tar.t({file,onentry:e=>entries.push(e.path)});assert(entries.includes('good'),'successful replacement must append the requested member');})().catch(e=>{console.error(e);process.exitCode=1;});");
    });
    test('SQLite actual synchronous extraction script accepts the fixture archive', t => {
        const root = directory(t), cwd = path.join(root, 'out'); fs.mkdirSync(cwd);
        const file = path.join(root, 'fixture.tgz'); fs.writeFileSync(file, zlib.gzipSync(archive(member('include/fixture.h', '#define FIXTURE 1\n'))));
        const script = path.join(path.dirname(appRequire.resolve('@louislam/sqlite3/package.json')), 'deps/extract.js');
        const result = spawnSync(process.execPath, [script, file, cwd], {encoding: 'utf8', timeout: 3000, maxBuffer: 32768});
        assert.ifError(result.error); assert.equal(result.status, 0, result.stderr);
        assert.equal(fs.readFileSync(path.join(cwd, 'include/fixture.h'), 'utf8'), '#define FIXTURE 1\n');
    });
    test('actual node-pre-gyp local binary installation preserves strip and entry handling', async t => {
        const root = directory(t), file = path.join(root, 'fixture.tgz');
        fs.writeFileSync(file, zlib.gzipSync(archive(member('package/fixture.node', 'DUMMY_NATIVE_FIXTURE'))));
        const packageJSON = {name: 'nf-fixture', version: '1.0.0', main: './index.js', binary: {module_name: 'fixture', module_path: './binding', host: 'file://' + root + '/', package_name: 'fixture.tgz'}};
        write(path.join(root, 'package.json'), packageJSON);
        const install = appRequire('@mapbox/node-pre-gyp/lib/install.js');
        await new Promise((resolve, reject) => install({package_json: packageJSON, opts: {module_root: root, 'fallback-to-build': false}}, [], error => error ? reject(error) : resolve()));
        assert.equal(fs.readFileSync(path.join(root, 'binding/fixture.node'), 'utf8'), 'DUMMY_NATIVE_FIXTURE');
    });
    test('actual node-gyp header installation preserves filtering and completion metadata', async t => {
        const root = directory(t), file = path.join(root, 'headers.tgz');
        fs.writeFileSync(file, zlib.gzipSync(archive(member('node-v22.23.4/include/node/fixture.h', '#define FIXTURE 1\n'),
            member('node-v22.23.4/include/node/ignored.txt', 'ignored'))));
        const gyp = {opts: {target: '22.23.4', tarball: file}, devDir: path.join(root, 'dev'), package: {installVersion: 9}};
        await new Promise((resolve, reject) => appRequire('node-gyp/lib/install.js')(gyp, [], error => error ? reject(error) : resolve()));
        assert.equal(fs.readFileSync(path.join(gyp.devDir, '22.23.4/include/node/fixture.h'), 'utf8'), '#define FIXTURE 1\n');
        assert(!fs.existsSync(path.join(gyp.devDir, '22.23.4/include/node/ignored.txt')));
        assert.equal(fs.readFileSync(path.join(gyp.devDir, '22.23.4/installVersion'), 'utf8').trim(), '9');
    });
    test('bounded high-ratio gzip is rejected by the main export default ratio guard', async t => {
        const root = directory(t), file = path.join(root, 'ratio.tgz');
        const bytes = archive(member('zeros', Buffer.alloc(16 * 1024 * 1024)));
        const compressed = zlib.gzipSync(bytes); assert(compressed.length < 20 * 1024);
        fs.writeFileSync(file, compressed);
        await assert.rejects(tar.t({file}), /decompression ratio/i);
    });
    test('main and raw exports enforce an explicitly configured decompression ratio', async t => {
        const root = directory(t), file = path.join(root, 'small-ratio.tgz');
        fs.writeFileSync(file, zlib.gzipSync(archive(member('zeros', Buffer.alloc(65536)))));
        await assert.rejects(tar.t({file, maxDecompressionRatio: 4}), /decompression ratio/i);
        if (appRequire('tar/package.json').exports?.['./raw']) {
            await assert.rejects(appRequire('tar/raw').t({file, maxDecompressionRatio: 4}), /decompression ratio/i);
        }
    });
} else {
    console.log('Original tar version: migration checks deferred to the qualified candidate.');
}
