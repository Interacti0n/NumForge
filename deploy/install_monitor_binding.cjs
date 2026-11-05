// Keep the old import path as an honest npm alias, with no application shim.
const fs = require('node:fs'), path = require('node:path'), assert = require('node:assert/strict'), crypto = require('node:crypto');
const guards = require('/tmp/build_monitor_sqlite.cjs');
const manifest = require('/tmp/monitor-binding.json'), engine = require('/tmp/monitor-sqlite.json');
const root = '/app/node_modules/@louislam/sqlite3';
assert.equal(process.platform, 'linux');
assert.equal(process.version, engine.node);
assert.equal(process.arch, engine.arch);
const hash = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const read = file => JSON.parse(fs.readFileSync(file));
if (process.argv[2] === 'check-base') {
    assert.equal(guards.wrapperHash(root), manifest.oldWrapperSha256);
    assert.equal(read(root + '/package.json').version, '15.1.6');
} else if (process.argv[2] === 'install') {
    assert.equal(read(root + '/package.json').name, manifest.name);
    assert.equal(read(root + '/package.json').version, manifest.version);
    const packageFile = '/app/package.json', pkg = read(packageFile);
    assert.equal(pkg.dependencies['@louislam/sqlite3'], '15.1.6');
    pkg.dependencies['@louislam/sqlite3'] = manifest.alias;
    fs.writeFileSync(packageFile, JSON.stringify(pkg, null, 2) + '\n');
    for (const file of ['/app/package-lock.json', '/app/node_modules/.package-lock.json']) {
        const lock = read(file), key = 'node_modules/@louislam/sqlite3';
        assert.equal(lock.packages[key].version, '15.1.6');
        lock.packages[key] = {name: manifest.name, version: manifest.version, resolved: manifest.url,
            integrity: manifest.integrity, hasInstallScript: true, license: 'BSD-3-Clause',
            dependencies: read(root + '/package.json').dependencies};
        lock.packages[key + '/node_modules/node-addon-api'] = {version: manifest.addon.version,
            resolved: manifest.addon.url, integrity: manifest.addon.integrity, license: 'MIT',
            engines: read(root + '/node_modules/node-addon-api/package.json').engines};
        if (lock.packages['']) lock.packages[''].dependencies['@louislam/sqlite3'] = manifest.alias;
        assert(!lock.dependencies, 'Unsupported legacy lock representation');
        fs.writeFileSync(file, JSON.stringify(lock, null, 2) + '\n');
    }
    const next = {...engine, bindingVersion: manifest.version, binary: manifest.binary,
        wrapperSha256: guards.wrapperHash(root), gypSha256: hash(root + '/deps/common-sqlite.gypi'), replacement: manifest};
    delete next.beforeSha256;
    fs.writeFileSync('/tmp/binding-engine.json', JSON.stringify(next, null, 2));
} else if (process.argv[2] === 'verify') {
    guards.verify(root, read('/tmp/binding-engine.json'), '/tmp/monitor-sqlite-receipt.json')
        .catch(error => {console.error(error); process.exitCode = 1;});
} else throw Error('Unknown binding operation');
