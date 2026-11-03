// Executed on stdin inside the disposable monitor, with the build guards loaded
// by check_monitor.py. All HTTP traffic below stays on that container's loopback.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const http = require('node:http');
const zlib = require('node:zlib');
const {spawn, spawnSync} = require('node:child_process');
const {once} = require('node:events');
const {createRequire} = require('node:module');
const appRequire = createRequire('/app/package.json');

if (fs.existsSync('/opt/numforge/monitor-patches.json') || process.env.NUMFORGE_FORCE_PATCH_CHECKS) {
    const guards = globalThis.monitorPatchGuards;
    const write = (file, value) => { fs.mkdirSync(path.dirname(file), {recursive: true}); fs.writeFileSync(file, JSON.stringify(value)); };

    function fixture(t) {
        const root = fs.mkdtempSync(path.join(os.tmpdir(), 'nf-patch-guard-'));
        t.after(() => fs.rmSync(root, {recursive: true}));
        write(path.join(root, 'package.json'), {version: '2.5.9', dependencies: {'body-parser': '~1.20.5'}});
        write(path.join(root, 'node_modules/body-parser/package.json'), {name: 'body-parser', version: '1.20.8'});
        write(path.join(root, 'node_modules/other/package.json'), {name: 'other', version: '1.0.0'});
        fs.symlinkSync('/app/node_modules/semver', path.join(root, 'node_modules/semver'));
        const lock = {packages: {'node_modules/body-parser': {version: '1.20.8', integrity: 'original'}, 'node_modules/other': {version: '1.0.0', integrity: 'other'}}};
        write(path.join(root, 'package-lock.json'), lock);
        write(path.join(root, 'node_modules/.package-lock.json'), lock);
        const manifest = {upstreamVersion: '2.5.9', keepAbsent: [], overrides: {'body-parser': '1.20.9'}, packages: [{name: 'body-parser', from: '1.20.8', version: '1.20.9', before: 'node_modules/body-parser', after: 'node_modules/body-parser', integrity: 'expected'}]};
        const snapshot = path.join(root, 'before.json');
        function install() {
            write(path.join(root, 'node_modules/body-parser/package.json'), {name: 'body-parser', version: '1.20.9'});
            lock.packages['node_modules/body-parser'] = {version: '1.20.9', integrity: 'expected', resolved: 'https://registry.npmjs.org/body-parser/-/body-parser-1.20.9.tgz'};
            write(path.join(root, 'package-lock.json'), lock);
        }
        return {root, manifest, snapshot, lock, install};
    }

    test('guard rejects unsupported upstream before changing files', t => {
        const f = fixture(t);
        f.manifest.upstreamVersion = '2.5.10';
        const before = fs.readFileSync(path.join(f.root, 'package.json'));
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /Upstream version changed/);
        assert.deepEqual(fs.readFileSync(path.join(f.root, 'package.json')), before);
    });
    test('source adaptation rejects changed upstream code before modifying files', t => {
        const f = fixture(t);
        const file = path.join(f.root, 'server/monitor-types/grpc.js');
        fs.mkdirSync(path.dirname(file), {recursive: true});
        fs.writeFileSync(file, 'unchanged fixture source');
        f.manifest.sourcePatches = [{path: 'server/monitor-types/grpc.js', beforeHash: 'unexpected'}];
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /Upstream source changed/);
        assert.equal(fs.readFileSync(file, 'utf8'), 'unchanged fixture source');
    });
    test('guard rejects a consumer range that excludes the patch', t => {
        const f = fixture(t);
        write(path.join(f.root, 'package.json'), {version: '2.5.9', dependencies: {'body-parser': '1.20.8'}});
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /Unsupported range/);
    });
    test('explicit direct migration is exact and stays in the same stable major', t => {
        const f = fixture(t);
        write(path.join(f.root, 'package.json'), {version: '2.5.9', dependencies: {'body-parser': '1.20.8'}});
        f.manifest.overrides = {};
        f.manifest.directMigrations = [{name: 'body-parser', fromRange: '1.20.8', version: '1.20.9'}];
        guards.prepare(f.root, f.manifest, f.snapshot);
        f.install();
        guards.verify(f.root, f.manifest, f.snapshot);
        assert.equal(JSON.parse(fs.readFileSync(path.join(f.root, 'package.json'))).dependencies['body-parser'], '1.20.9');
    });
    test('explicit migration rejects a major jump before changing files', t => {
        const f = fixture(t);
        f.manifest.packages[0].version = '2.0.0';
        f.manifest.directMigrations = [{name: 'body-parser', fromRange: '~1.20.5', version: '2.0.0'}];
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /same stable major|within a stable major/);
    });
    test('direct migration cannot bypass another consumer range', t => {
        const f = fixture(t);
        write(path.join(f.root, 'node_modules/other/package.json'), {name: 'other', version: '1.0.0', dependencies: {'body-parser': '1.20.8'}});
        f.manifest.directMigrations = [{name: 'body-parser', fromRange: '~1.20.5', version: '1.20.9'}];
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /Unsupported range/);
    });

    function legacyFixture(t, compatibility = 'legacy-0.x', version = '0.34.0') {
        const f = fixture(t);
        write(path.join(f.root, 'package.json'), {version: '2.5.9', dependencies: {'body-parser': '~0.32.0'}});
        write(path.join(f.root, 'node_modules/body-parser/package.json'), {name: 'body-parser', version: '0.32.0'});
        f.lock.packages['node_modules/body-parser'].version = '0.32.0';
        write(path.join(f.root, 'package-lock.json'), f.lock);
        f.manifest.overrides = {};
        Object.assign(f.manifest.packages[0], {from: '0.32.0', version});
        f.manifest.directMigrations = [{name: 'body-parser', fromRange: '~0.32.0', version, compatibility}];
        return f;
    }
    function majorFixture(t, version = '10.3.1', from = '7.0.13') {
        const f = fixture(t);
        write(path.join(f.root, 'package.json'), {version: '2.5.9', dependencies: {'body-parser': '~' + from}});
        write(path.join(f.root, 'node_modules/body-parser/package.json'), {name: 'body-parser', version: from});
        f.lock.packages['node_modules/body-parser'].version = from;
        write(path.join(f.root, 'package-lock.json'), f.lock);
        f.manifest.overrides = {};
        Object.assign(f.manifest.packages[0], {from, version});
        f.manifest.directMigrations = [{name: 'body-parser', fromRange: '~' + from, version, compatibility: 'stable-major-upgrade'}];
        return f;
    }
    test('explicit stable major upgrade installs and verifies its exact pinned version', t => {
        const f = majorFixture(t);
        guards.prepare(f.root, f.manifest, f.snapshot);
        write(path.join(f.root, 'node_modules/body-parser/package.json'), {name: 'body-parser', version: '10.3.1', engines: {node: '>=20'}});
        f.lock.packages['node_modules/body-parser'] = {version: '10.3.1', integrity: 'expected', resolved: 'https://registry.npmjs.org/body-parser/-/body-parser-10.3.1.tgz'};
        write(path.join(f.root, 'package-lock.json'), f.lock);
        guards.verify(f.root, f.manifest, f.snapshot);
        assert.equal(JSON.parse(fs.readFileSync(path.join(f.root, 'package.json'))).dependencies['body-parser'], '10.3.1');
    });
    for (const [label, version, from] of [
        ['a downgrade', '6.9.0', '7.0.13'], ['the same major', '7.1.0', '7.0.13'],
        ['an unstable source', '10.3.1', '0.9.0'], ['a prerelease target', '10.4.0-beta.1', '7.0.13'],
        ['a prerelease source', '10.3.1', '7.0.13-beta.1'], ['an invalid target', 'latest', '7.0.13'],
    ]) {
        test('stable major upgrade rejects ' + label + ' before writing', t => {
            const f = majorFixture(t, version, from);
            const before = fs.readFileSync(path.join(f.root, 'package.json'));
            assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /forward between stable majors/);
            assert.deepEqual(fs.readFileSync(path.join(f.root, 'package.json')), before);
        });
    }
    test('explicit major policy cannot bypass a second consumer range', t => {
        const f = majorFixture(t);
        write(path.join(f.root, 'node_modules/other/package.json'), {name: 'other', version: '1.0.0', dependencies: {'body-parser': '~7.0.13'}});
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /Unsupported range/);
    });
    test('guard rejects a pinned package requiring an unsupported Node runtime', t => {
        const f = fixture(t);
        guards.prepare(f.root, f.manifest, f.snapshot);
        f.install();
        write(path.join(f.root, 'node_modules/body-parser/package.json'), {name: 'body-parser', version: '1.20.9', engines: {node: '>=999'}});
        assert.throws(() => guards.verify(f.root, f.manifest, f.snapshot), /Unsupported Node runtime/);
    });
    test('qualified legacy migration installs and verifies an exact forward 0.x pin', t => {
        const f = legacyFixture(t);
        guards.prepare(f.root, f.manifest, f.snapshot);
        write(path.join(f.root, 'node_modules/body-parser/package.json'), {name: 'body-parser', version: '0.34.0'});
        f.lock.packages['node_modules/body-parser'] = {version: '0.34.0', integrity: 'expected', resolved: 'https://registry.npmjs.org/body-parser/-/body-parser-0.34.0.tgz'};
        write(path.join(f.root, 'package-lock.json'), f.lock);
        guards.verify(f.root, f.manifest, f.snapshot);
        assert.equal(JSON.parse(fs.readFileSync(path.join(f.root, 'package.json'))).dependencies['body-parser'], '0.34.0');
    });
    for (const [label, policy, version, message] of [
        ['implicit 0.x migration', undefined, '0.34.0', /stable major/],
        ['legacy jump to 1.x', 'legacy-0.x', '1.0.0', /forward within 0.x/],
        ['legacy downgrade', 'legacy-0.x', '0.31.0', /forward within 0.x/],
        ['unchanged legacy version', 'legacy-0.x', '0.32.0', /forward within 0.x/],
        ['unknown compatibility policy', 'unreviewed', '0.34.0', /Unknown direct compatibility/],
    ]) {
        test('guard rejects ' + label + ' before writing', t => {
            const f = legacyFixture(t, policy, version);
            // Undefined must remain undefined rather than use the fixture default.
            f.manifest.directMigrations[0].compatibility = policy;
            const before = fs.readFileSync(path.join(f.root, 'package.json'));
            assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), message);
            assert.deepEqual(fs.readFileSync(path.join(f.root, 'package.json')), before);
        });
    }
    test('legacy qualification cannot bypass a separate consumer version range', t => {
        const f = legacyFixture(t);
        write(path.join(f.root, 'node_modules/other/package.json'), {name: 'other', version: '1.0.0', dependencies: {'body-parser': '~0.32.0'}});
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /Unsupported range/);
    });

    function consumerFixture(t) {
        const f = fixture(t);
        write(path.join(f.root, 'node_modules/other/package.json'), {name: 'other', version: '1.0.0', dependencies: {'body-parser': '1.20.8'}});
        const migration = {consumer: 'node_modules/other', consumerVersion: '1.0.0', consumerIntegrity: 'other',
            name: 'body-parser', fromRange: '1.20.8', version: '1.20.9'};
        f.manifest.consumerMigrations = [migration];
        f.manifest.overrides.other = {'body-parser': '1.20.9'};
        return {...f, migration};
    }
    test('explicit consumer migration preserves owner metadata and uses its exact scoped pin', t => {
        const f = consumerFixture(t);
        guards.prepare(f.root, f.manifest, f.snapshot);
        f.install();
        guards.verify(f.root, f.manifest, f.snapshot);
    });
    test('consumer migration rejects changed upstream version, integrity or range before writing', t => {
        const f = consumerFixture(t);
        const file = path.join(f.root, 'package.json');
        const before = fs.readFileSync(file);
        for (const [key, invalid] of [['consumerVersion', '2.0.0'], ['consumerIntegrity', 'changed'], ['fromRange', '~1.20.0']]) {
            const original = f.migration[key]; f.migration[key] = invalid;
            assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /consumer (version|integrity|range) changed/);
            assert.deepEqual(fs.readFileSync(file), before);
            f.migration[key] = original;
        }
    });
    test('consumer migration cannot authorize a major jump', t => {
        const f = consumerFixture(t);
        f.manifest.packages[0].version = f.migration.version = '2.0.0';
        f.manifest.overrides.other['body-parser'] = '2.0.0';
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /same stable major/);
    });
    test('consumer migration rejects an incorrect scoped pin', t => {
        const f = consumerFixture(t);
        f.manifest.overrides.other['body-parser'] = '1.20.8';
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /requires an exact override/);
    });
    test('consumer lock refresh rejects packages with runtime dependencies', t => {
        const f = consumerFixture(t);
        write(path.join(f.root, 'node_modules/body-parser/package.json'), {name: 'body-parser', version: '1.20.8', dependencies: {other: '^1.0.0'}});
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /limited to leaf packages/);
    });
    function refreshFixture(t) {
        const f = consumerFixture(t);
        f.migration.dependencyRefresh = {from: {other: '^1.0.0'}, to: {other: '~1.0.0'}};
        write(path.join(f.root, 'node_modules/body-parser/package.json'), {name: 'body-parser', version: '1.20.8', dependencies: {other: '^1.0.0'}});
        function install() {
            f.install();
            write(path.join(f.root, 'node_modules/body-parser/package.json'), {name: 'body-parser', version: '1.20.9', dependencies: {other: '~1.0.0'}});
        }
        return {...f, install};
    }
    test('explicit dependency refresh seeds exact target ranges and verifies installed closure', t => {
        const f = refreshFixture(t);
        guards.prepare(f.root, f.manifest, f.snapshot);
        assert.deepEqual(JSON.parse(fs.readFileSync(path.join(f.root, 'package-lock.json'))).packages['node_modules/body-parser'].dependencies, {other: '~1.0.0'});
        f.install();
        guards.verify(f.root, f.manifest, f.snapshot);
    });
    test('dependency refresh rejects changed upstream ranges before writing', t => {
        const f = refreshFixture(t);
        f.migration.dependencyRefresh.from.other = '^2.0.0';
        const before = fs.readFileSync(path.join(f.root, 'package.json'));
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /Upstream refreshed dependencies changed/);
        assert.deepEqual(fs.readFileSync(path.join(f.root, 'package.json')), before);
    });
    test('dependency refresh cannot delete a nested installed package', t => {
        const f = refreshFixture(t);
        write(path.join(f.root, 'node_modules/body-parser/node_modules/keep/package.json'), {name: 'keep', version: '1.0.0'});
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /cannot remove nested packages/);
        assert(fs.existsSync(path.join(f.root, 'node_modules/body-parser/node_modules/keep/package.json')));
    });
    test('dependency refresh rejects optional upstream dependencies', t => {
        const f = refreshFixture(t);
        write(path.join(f.root, 'node_modules/body-parser/package.json'), {name: 'body-parser', version: '1.20.8', dependencies: {other: '^1.0.0'}, optionalDependencies: {other: '^1.0.0'}});
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /cannot change optional dependencies/);
    });
    test('dependency refresh rejects target metadata outside the exact manifest', t => {
        const f = refreshFixture(t);
        guards.prepare(f.root, f.manifest, f.snapshot);
        f.install();
        write(path.join(f.root, 'node_modules/body-parser/package.json'), {name: 'body-parser', version: '1.20.9', dependencies: {other: '^1.0.0'}});
        assert.throws(() => guards.verify(f.root, f.manifest, f.snapshot), /Refreshed target dependencies changed/);
    });
    test('dependency refresh still rejects unsatisfied installed dependencies', t => {
        const f = refreshFixture(t);
        f.migration.dependencyRefresh.to.other = '^2.0.0';
        guards.prepare(f.root, f.manifest, f.snapshot);
        f.install();
        write(path.join(f.root, 'node_modules/body-parser/package.json'), {name: 'body-parser', version: '1.20.9', dependencies: {other: '^2.0.0'}});
        assert.throws(() => guards.verify(f.root, f.manifest, f.snapshot), /Unsatisfied patch dependency/);
    });
    test('consumer migration cannot bypass a different consumer range', t => {
        const f = consumerFixture(t);
        write(path.join(f.root, 'node_modules/third/package.json'), {name: 'third', version: '1.0.0', dependencies: {'body-parser': '1.20.8'}});
        f.lock.packages['node_modules/third'] = {version: '1.0.0', integrity: 'third'};
        write(path.join(f.root, 'package-lock.json'), f.lock);
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /Unsupported range/);
    });
    test('consumer migration rejects a changed owner after installation', t => {
        const f = consumerFixture(t);
        guards.prepare(f.root, f.manifest, f.snapshot);
        f.install();
        write(path.join(f.root, 'node_modules/other/package.json'), {name: 'other', version: '1.0.0', dependencies: {'body-parser': '1.20.8'}, changed: true});
        assert.throws(() => guards.verify(f.root, f.manifest, f.snapshot), /Migrated consumer metadata changed/);
    });
    test('guard rejects a different archive integrity', t => {
        const f = fixture(t);
        guards.prepare(f.root, f.manifest, f.snapshot);
        f.install();
        f.lock.packages['node_modules/body-parser'].integrity = 'tampered';
        write(path.join(f.root, 'package-lock.json'), f.lock);
        assert.throws(() => guards.verify(f.root, f.manifest, f.snapshot), /Unexpected archive integrity/);
    });
    test('guard rejects an unrelated package mutation', t => {
        const f = fixture(t);
        guards.prepare(f.root, f.manifest, f.snapshot);
        f.install();
        write(path.join(f.root, 'node_modules/other/package.json'), {name: 'other', version: '1.0.0', unexpected: true});
        assert.throws(() => guards.verify(f.root, f.manifest, f.snapshot), /Unrelated package changed/);
    });

    function additionFixture(t) {
        const f = fixture(t);
        f.manifest.directMigrations = [{name: 'body-parser', fromRange: '~1.20.5', version: '1.20.9'}];
        const added = {name: '@scope/helper', path: 'node_modules/@scope/helper', version: '4.4.2', integrity: 'helper-integrity', addedBy: 'body-parser'};
        f.manifest.addedDependencies = [added];
        f.manifest.overrides[added.name] = added.version;
        function install() {
            f.install();
            write(path.join(f.root, 'node_modules/body-parser/package.json'), {name: 'body-parser', version: '1.20.9', dependencies: {'@scope/helper': '^4.4.2'}});
            write(path.join(f.root, added.path, 'package.json'), {name: added.name, version: added.version});
            f.lock.packages[added.path] = {version: added.version, integrity: added.integrity, resolved: 'https://registry.npmjs.org/@scope/helper/-/helper-4.4.2.tgz'};
            write(path.join(f.root, 'package-lock.json'), f.lock);
        }
        return {...f, added, install};
    }

    test('guard permits only the pinned scoped addition required by a migrated owner', t => {
        const f = additionFixture(t);
        guards.prepare(f.root, f.manifest, f.snapshot);
        f.install();
        guards.verify(f.root, f.manifest, f.snapshot);
    });
    test('guard permits an exact owner-local addition without changing the shared root', t => {
        const f = additionFixture(t);
        f.added.path = 'node_modules/body-parser/node_modules/@scope/helper';
        delete f.manifest.overrides[f.added.name];
        f.manifest.overrides['body-parser'] = {'.': '1.20.9', '@scope/helper': '4.4.2'};
        guards.prepare(f.root, f.manifest, f.snapshot);
        f.install();
        guards.verify(f.root, f.manifest, f.snapshot);
    });
    test('guard rejects additions under an unrelated owner', t => {
        const f = additionFixture(t);
        f.added.path = 'node_modules/other/node_modules/@scope/helper';
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /exact root or owner path/);
    });
    test('guard rejects an unpinned owner-local addition', t => {
        const f = additionFixture(t);
        f.added.path = 'node_modules/body-parser/node_modules/@scope/helper';
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /pinned exactly/);
    });
    test('guard rejects an already present added dependency before writing', t => {
        const f = additionFixture(t);
        write(path.join(f.root, f.added.path, 'package.json'), {version: '4.4.2'});
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /already exists/);
    });
    test('guard rejects an addition not required by its migrated owner', t => {
        const f = additionFixture(t);
        guards.prepare(f.root, f.manifest, f.snapshot);
        f.install();
        write(path.join(f.root, 'node_modules/body-parser/package.json'), {name: 'body-parser', version: '1.20.9'});
        assert.throws(() => guards.verify(f.root, f.manifest, f.snapshot), /not required by migrated owner/);
    });
    test('guard rejects a changed scoped archive URL or integrity', t => {
        const f = additionFixture(t);
        guards.prepare(f.root, f.manifest, f.snapshot);
        f.install();
        f.lock.packages[f.added.path].resolved = 'https://example.invalid/helper.tgz';
        write(path.join(f.root, 'package-lock.json'), f.lock);
        assert.throws(() => guards.verify(f.root, f.manifest, f.snapshot), /registry.npmjs.org/);
        f.lock.packages[f.added.path].integrity = 'tampered';
        write(path.join(f.root, 'package-lock.json'), f.lock);
        assert.throws(() => guards.verify(f.root, f.manifest, f.snapshot), /Unexpected archive integrity/);
    });
    test('guard still rejects an unlisted installed package addition', t => {
        const f = additionFixture(t);
        guards.prepare(f.root, f.manifest, f.snapshot);
        f.install();
        write(path.join(f.root, 'node_modules/unlisted/package.json'), {version: '1.0.0'});
        f.lock.packages['node_modules/unlisted'] = {version: '1.0.0', integrity: 'unlisted'};
        write(path.join(f.root, 'package-lock.json'), f.lock);
        assert.throws(() => guards.verify(f.root, f.manifest, f.snapshot), /Unrelated package changed/);
    });

    function sharedFixture(t) {
        const f = fixture(t);
        const shared = {name: 'body-parser', from: 'node_modules/other/node_modules/body-parser', to: 'node_modules/body-parser', version: '1.20.9', integrity: 'expected'};
        f.manifest.deduplicatedDependencies = [shared];
        write(path.join(f.root, 'node_modules/other/package.json'), {name: 'other', version: '1.0.0', dependencies: {'body-parser': '~1.20.0'}});
        write(path.join(f.root, shared.from, 'package.json'), {name: 'body-parser', version: '1.20.9'});
        f.lock.packages[shared.from] = {version: '1.20.9', integrity: 'expected', resolved: 'https://registry.npmjs.org/body-parser/-/body-parser-1.20.9.tgz'};
        write(path.join(f.root, 'package-lock.json'), f.lock);
        function install() {
            f.install();
            fs.rmSync(path.join(f.root, shared.from), {recursive: true});
            delete f.lock.packages[shared.from];
            write(path.join(f.root, 'package-lock.json'), f.lock);
        }
        return {...f, shared, install};
    }
    test('guard permits an identical shared package with verified consumer resolution', t => {
        const f = sharedFixture(t);
        guards.prepare(f.root, f.manifest, f.snapshot);
        f.install();
        guards.verify(f.root, f.manifest, f.snapshot);
    });
    test('guard rejects a shared consumer whose range excludes the target', t => {
        const f = sharedFixture(t);
        write(path.join(f.root, 'node_modules/other/package.json'), {version: '1.0.0', dependencies: {'body-parser': '1.20.8'}});
        assert.throws(() => guards.prepare(f.root, f.manifest, f.snapshot), /Unsupported shared range/);
    });
    test('guard rejects a changed shared package identity', t => {
        const f = sharedFixture(t);
        guards.prepare(f.root, f.manifest, f.snapshot);
        f.install();
        write(path.join(f.root, f.shared.to, 'package.json'), {name: 'body-parser', version: '1.20.9', changed: true});
        assert.throws(() => guards.verify(f.root, f.manifest, f.snapshot), /Shared package identity changed/);
    });
    test('guard rejects shared consumers that still resolve a nested copy', t => {
        const f = sharedFixture(t);
        guards.prepare(f.root, f.manifest, f.snapshot);
        f.install();
        write(path.join(f.root, f.shared.from, 'package.json'), {name: 'body-parser', version: '1.20.9'});
        f.lock.packages[f.shared.from] = {version: '1.20.9', integrity: 'expected'};
        write(path.join(f.root, 'package-lock.json'), f.lock);
        assert.throws(() => guards.verify(f.root, f.manifest, f.snapshot), /Duplicate package remains/);
    });

    function request(port, payload, headers = {}) {
        return new Promise((resolve, reject) => {
            const req = http.request({host: '127.0.0.1', port, method: 'POST', path: '/', headers: {'Content-Type': 'application/json', 'Content-Length': payload.length, ...headers}}, res => {
                const parts = [];
                res.on('data', part => parts.push(part));
                res.on('error', reject);
                res.on('end', () => resolve({status: res.statusCode, body: Buffer.concat(parts).toString()}));
            });
            req.setTimeout(2000, () => req.destroy(new Error('fixture request timeout')));
            req.on('error', reject);
            req.end(payload);
        });
    }

    test('body parser handles JSON, gzip, forms, malformed bodies and limits', async t => {
        const express = appRequire('express'), parser = appRequire('body-parser');
        const app = express();
        app.use(parser.json({limit: '1kb'}), parser.urlencoded({extended: false, limit: '1kb'}));
        app.post('/', (req, res) => res.json(req.body));
        app.use((error, req, res, next) => res.status(error.status || 500).end());
        const server = http.createServer(app);
        t.after(() => { server.closeAllConnections(); server.close(); });
        server.listen(0, '127.0.0.1');
        await once(server, 'listening');
        const port = server.address().port;
        assert.deepEqual(await request(port, Buffer.from('{"value":7}')), {status: 200, body: '{"value":7}'});
        assert.equal((await request(port, zlib.gzipSync('{"compressed":true}'), {'Content-Encoding': 'gzip'})).body, '{"compressed":true}');
        assert.equal((await request(port, Buffer.from('name=NumForge'), {'Content-Type': 'application/x-www-form-urlencoded'})).body, '{"name":"NumForge"}');
        assert.equal((await request(port, Buffer.from('{broken'))).status, 400);
        assert.equal((await request(port, Buffer.from(JSON.stringify({long: 'x'.repeat(2048)})))).status, 413);
    });

    test('compressed trailing bytes cannot crash the parser process', async t => {
        const program = `const http=require('http'),parser=require('/app/node_modules/body-parser').json({limit:'1mb'});
const server=http.createServer((req,res)=>parser(req,res,error=>{res.statusCode=error?.status||200;res.end(JSON.stringify(req.body||{}));}));
server.listen(0,'127.0.0.1',()=>process.send(server.address().port));`;
        const child = spawn(process.execPath, ['-e', program], {stdio: ['ignore', 'ignore', 'pipe', 'ipc']});
        t.after(() => child.kill());
        let errors = '';
        child.stderr.on('data', part => { errors += part; });
        const [port] = await once(child, 'message');
        const payload = Buffer.concat([zlib.gzipSync('{"probe":17}'), Buffer.alloc(1), Buffer.alloc(64 * 1024, 88)]);
        const response = await request(port, payload, {'Content-Encoding': 'gzip'});
        assert.equal(response.status, 200);
        assert.equal(response.body, '{"probe":17}');
        await new Promise(resolve => setTimeout(resolve, 300));
        assert.equal(child.exitCode, null, errors);
        assert.equal((await request(port, Buffer.from('{"stillAlive":true}'))).status, 200);
    });

    test('redirects reach the target, preserve safe headers and bound loops', async t => {
        const redirects = appRequire('follow-redirects').http;
        const server = http.createServer((req, res) => {
            if (req.url === '/loop') { res.writeHead(302, {Location: '/loop'}); res.end(); }
            else if (req.url === '/start') { res.writeHead(302, {Location: '/target'}); res.end(); }
            else res.end(JSON.stringify({url: req.url, custom: req.headers['x-fixture']}));
        });
        t.after(() => {server.closeAllConnections(); server.close();});
        server.listen(0, '127.0.0.1'); await once(server, 'listening');
        const get = route => new Promise((resolve, reject) => {
            const req = redirects.get({host: '127.0.0.1', port: server.address().port, path: route, maxRedirects: 2, headers: {'X-Fixture': 'retained'}}, res => {
                let data = ''; res.on('data', part => {data += part;}); res.on('end', () => resolve(JSON.parse(data)));
            });
            req.setTimeout(2000, () => req.destroy(new Error('redirect timeout'))); req.on('error', reject);
        });
        assert.deepEqual(await get('/start'), {url: '/target', custom: 'retained'});
        await assert.rejects(get('/loop'), {code: 'ERR_FR_TOO_MANY_REDIRECTS'});
    });

    test('glob consumer uses patched minimatch and preserves matching behavior', () => {
        const globRequire = createRequire(appRequire.resolve('redbean-node/node_modules/glob/package.json'));
        const mm = globRequire('minimatch');
        assert(mm.minimatch('src/core/value.c', 'src/**/*.c'));
        assert(!mm.minimatch('src/core/value.h', 'src/**/*.c'));
        assert(mm.minimatch('report.json', '*.{json,csv}'));
        const resolved = guards.resolvePackage('/app', 'node_modules/redbean-node/node_modules/glob', 'minimatch');
        const receipt = '/opt/numforge/monitor-patches.json';
        const expected = fs.existsSync(receipt) ? JSON.parse(fs.readFileSync(receipt)).packages.find(p => p.name === 'minimatch').version : '9.0.9';
        assert.equal(JSON.parse(fs.readFileSync('/app/' + resolved + '/package.json')).version, expected);
    });

    test('documented wildcard and nested-extglob cases finish within two seconds', () => {
        const program = `const {createRequire}=require('module');const r=createRequire(require.resolve('/app/node_modules/redbean-node/node_modules/glob/package.json'));const m=r('minimatch').minimatch;
if(m('a'.repeat(96), '*'.repeat(32)+'Z'+'*'.repeat(5)))process.exit(2);
if(m('a'.repeat(128), '*('.repeat(18)+'a'+')'.repeat(18)+'b'))process.exit(3);`;
        const result = spawnSync(process.execPath, ['-e', program], {timeout: 2000, encoding: 'utf8'});
        assert.equal(result.error, undefined);
        assert.equal(result.status, 0, result.stderr);
    });
} else {
    console.log('Upstream image: targeted patch regression checks deferred to patched candidate.');
}
