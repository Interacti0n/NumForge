// Preserve the application and existing shared dependencies; update genuine lock entries.
const fs = require('node:fs'), path = require('node:path'), crypto = require('node:crypto');
const assert = require('node:assert/strict');
const semver = require('/app/node_modules/semver');
const read = file => JSON.parse(fs.readFileSync(file, 'utf8'));
const manifest = read('/tmp/monitor-xml.json');
const prefix = 'node_modules/fast-xml-parser';
const target = '/app/' + prefix;
const before = read('/app/package-lock.json');
assert.equal(before.packages[prefix].version, manifest.fromVersion);
assert.equal(before.packages[prefix].integrity, manifest.fromIntegrity);
assert.equal(read(target + '/package.json').version, manifest.fromVersion);
const pin = manifest.packages.find(p => p.name === 'fast-xml-parser');
const owner = manifest.consumer;
assert.equal(before.packages[owner.path].version, owner.version);
assert.equal(before.packages[owner.path].integrity, owner.integrity);
assert.equal(read('/app/' + owner.path + '/package.json').dependencies['fast-xml-parser'], owner.fromRange);
const application = read('/app/package.json');
assert(!Object.hasOwn(application.overrides, 'gamedig'));
const consumers = [];
for (const [location, entry] of Object.entries(before.packages)) {
    const range = {...entry.dependencies, ...entry.optionalDependencies}['fast-xml-parser'];
    if (!range) continue;
    if (location === owner.path) {
        assert.equal(range, owner.fromRange);
        assert(semver.major(pin.version) === semver.major(manifest.fromVersion));
    } else {
        assert(semver.satisfies(pin.version, range), 'Unsupported XML consumer range: ' + location);
    }
    consumers.push({location, range, effectiveRange: location === owner.path ? pin.version : range});
}
assert(consumers.length);
for (const entry of manifest.packages) {
    for (const [name, range] of Object.entries(entry.dependencies)) {
        const dependency = manifest.packages.find(p => p.name === name);
        assert(dependency && semver.satisfies(dependency.version, range), 'Incomplete XML dependency closure: ' + name);
    }
}
fs.rmSync(target, {recursive: true});
fs.cpSync('/tmp/xml-source', target, {recursive: true});
application.overrides.gamedig = {'fast-xml-parser': pin.version};
fs.writeFileSync('/app/package.json', JSON.stringify(application, null, 2) + '\n');
for (const filename of ['/app/package-lock.json', '/app/node_modules/.package-lock.json']) {
    const lock = read(filename);
    for (const key of Object.keys(lock.packages)) if (key.startsWith(prefix + '/node_modules/')) delete lock.packages[key];
    for (const entry of manifest.packages) {
        const location = entry.name === 'fast-xml-parser' ? prefix : prefix + '/node_modules/' + entry.name;
        const pkg = read('/app/' + location + '/package.json');
        const value = {version: entry.version, resolved: entry.url, integrity: entry.integrity};
        for (const key of ['dependencies', 'optionalDependencies', 'engines', 'bin', 'license', 'funding']) {
            if (pkg[key] !== undefined) value[key] = pkg[key];
        }
        lock.packages[location] = value;
    }
    fs.writeFileSync(filename, JSON.stringify(lock, null, 2) + '\n');
}
const cli = '/app/node_modules/.bin/fxparser';
assert(fs.lstatSync(cli).isSymbolicLink());
assert.equal(fs.readlinkSync(cli), '../fast-xml-parser/src/cli/cli.js');
fs.unlinkSync(cli);
const bin = read(target + '/package.json').bin.fxparser;
assert(!path.isAbsolute(bin) && !bin.split('/').includes('..'));
assert(fs.statSync(path.join(target, bin)).isFile());
fs.symlinkSync('../fast-xml-parser/' + bin, cli);
let files = [];
function visit(root) {
    for (const name of fs.readdirSync(root).sort()) {
        const file = path.join(root, name), info = fs.lstatSync(file);
        assert(!info.isSymbolicLink());
        if (info.isDirectory()) visit(file);
        else files.push([path.relative(target, file), info.mode & 511, crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex')]);
    }
}
visit(target);
fs.writeFileSync('/opt/numforge/monitor-xml.json', JSON.stringify({manifest, consumers, files}, null, 2));
assert.equal(require(target + '/package.json').version, pin.version);
