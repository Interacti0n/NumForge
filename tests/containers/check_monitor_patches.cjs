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
        assert.equal(JSON.parse(fs.readFileSync('/app/' + resolved + '/package.json')).version, '9.0.9');
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
