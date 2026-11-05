// Rebuild only the pinned SQLite engine; never run npm install hooks or use a prebuilt download.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const {execFileSync} = require('node:child_process');
const digest = (data, algorithm = 'sha256') => crypto.createHash(algorithm).update(data).digest('hex');
const read = file => JSON.parse(fs.readFileSync(file, 'utf8'));
const moduleRoot = '/app/node_modules/@louislam/sqlite3';

function wrapperHash(root) {
    const files = ['package.json', 'binding.gyp', 'deps/sqlite3.gyp', 'deps/extract.js'];
    function walk(relative) {
        for (const entry of fs.readdirSync(path.join(root, relative), {withFileTypes: true})) {
            const name = relative + '/' + entry.name;
            if (name === 'lib/binding') continue;
            assert(!entry.isSymbolicLink(), 'Unexpected binding source symlink');
            if (entry.isDirectory()) walk(name);
            else {assert(entry.isFile()); files.push(name);}
        }
    }
    walk('src'); walk('lib');
    return digest(JSON.stringify(files.sort().map(name => [name, digest(fs.readFileSync(path.join(root, name)))])));
}

function validateBase(root, manifest) {
    assert.equal(read(path.join(root, 'package.json')).version, manifest.bindingVersion);
    assert.equal(wrapperHash(root), manifest.wrapperSha256, 'Binding source changed; requalify');
    for (const [name, expected] of Object.entries(manifest.beforeSha256)) {
        assert.equal(digest(fs.readFileSync(path.join(root, name))), expected, 'Upstream SQLite file changed: ' + name);
    }
}

async function prepare(root, manifest, download = fetch) {
    assert.equal(process.platform, 'linux');
    assert.equal(process.arch, manifest.arch);
    assert.equal(process.version, manifest.node);
    validateBase(root, manifest);
    assert(/^\d{7}$/.test(manifest.archiveVersion));
    const url = new URL(manifest.url);
    assert.equal(url.origin, 'https://www.sqlite.org');
    assert.equal(url.pathname, '/2026/sqlite-autoconf-' + manifest.archiveVersion + '.tar.gz');
    const response = await download(url, {signal: AbortSignal.timeout(60000), redirect: 'error'});
    assert(response.ok, 'Official SQLite source download failed');
    const chunks = []; let length = 0;
    for await (const chunk of response.body) {
        length += chunk.length;
        assert(length <= 12 * 1024 * 1024, 'SQLite source archive exceeds bound');
        chunks.push(chunk);
    }
    const archive = Buffer.concat(chunks);
    assert.equal(digest(archive, 'sha3-256'), manifest.archiveSha3, 'SQLite archive integrity mismatch');
    const prefix = 'sqlite-autoconf-' + manifest.archiveVersion;
    const target = path.join(root, 'deps', prefix + '.tar.gz');
    fs.writeFileSync(target, archive, {flag: 'wx'});
    let source;
    await require('/app/node_modules/tar').t({file: target, onentry(entry) {
        if (entry.path !== prefix + '/sqlite3.c') return;
        assert.equal(source, undefined, 'Duplicate SQLite amalgamation');
        const parts = []; source = parts;
        entry.on('data', chunk => parts.push(chunk));
    }});
    assert(source, 'SQLite amalgamation missing');
    assert.equal(digest(Buffer.concat(source), 'sha3-256'), manifest.amalgamationSha3, 'Amalgamation integrity mismatch');
    const configPath = path.join(root, 'deps/common-sqlite.gypi');
    const config = fs.readFileSync(configPath, 'utf8');
    const before = "'sqlite_version%':'3410100'";
    assert.equal(config.split(before).length, 2, 'SQLite source version must match once');
    const changed = config.replace(before, "'sqlite_version%':'" + manifest.archiveVersion + "'");
    assert.equal(digest(changed), manifest.gypSha256);
    fs.writeFileSync(configPath, changed);
    fs.unlinkSync(path.join(root, 'deps/sqlite-autoconf-3410100.tar.gz'));
    // Force a source build; successful verification cannot load the old binary.
    fs.unlinkSync(path.join(root, manifest.binary));
    console.log('Pinned SQLite source archive and amalgamation verified; original binding source preserved.');
}

async function verify(root, manifest, output) {
    assert.equal(wrapperHash(root), manifest.wrapperSha256, 'Binding source changed during compilation');
    assert.equal(digest(fs.readFileSync(path.join(root, 'deps/common-sqlite.gypi'))), manifest.gypSha256);
    assert.equal(digest(fs.readFileSync(path.join(root, 'deps/sqlite-autoconf-' + manifest.archiveVersion + '.tar.gz')), 'sha3-256'), manifest.archiveSha3);
    assert(!fs.existsSync(path.join(root, 'deps/sqlite-autoconf-3410100.tar.gz')));
    const sqlite = require(root);
    const database = new sqlite.Database(':memory:');
    const get = sql => new Promise((resolve, reject) => database.get(sql, (error, row) => error ? reject(error) : resolve(row)));
    const all = sql => new Promise((resolve, reject) => database.all(sql, (error, rows) => error ? reject(error) : resolve(rows)));
    let identity, options;
    try {
        identity = await get('SELECT sqlite_version() AS version, sqlite_source_id() AS source');
        assert.equal(identity.version, manifest.version); assert.equal(identity.source, manifest.sourceId);
        options = (await all('PRAGMA compile_options')).map(row => row.compile_options);
        for (const option of manifest.requiredOptions) assert(options.includes(option), 'Missing SQLite option: ' + option);
        assert.equal((await get("SELECT json_extract('{\"value\":4}', '$.value') AS value")).value, 4);
    } finally {await new Promise((resolve, reject) => database.close(error => error ? reject(error) : resolve()));}
    const binary = path.join(root, manifest.binary);
    const linkage = execFileSync('ldd', [binary], {encoding: 'utf8'});
    assert(!/libsqlite3|not found/.test(linkage), 'Unexpected native SQLite linkage');
    const receipt = {...manifest, nativeSha256: digest(fs.readFileSync(binary)),
        compiler: execFileSync('g++', ['-dumpfullversion', '-dumpversion'], {encoding: 'utf8'}).trim(),
        runtime: identity, compileOptions: options, linkage};
    fs.writeFileSync(output, JSON.stringify(receipt, null, 2) + '\n');
    console.log('Rebuilt native SQLite identity, binding source, features and linkage verified.');
}

if (require.main === module) {
    const [operation, manifestFile, output] = process.argv.slice(2);
    assert(['prepare', 'verify'].includes(operation));
    ({prepare, verify})[operation](moduleRoot, read(manifestFile), output).catch(error => {console.error(error); process.exitCode = 1;});
}
module.exports = {wrapperHash, validateBase, prepare, verify};
