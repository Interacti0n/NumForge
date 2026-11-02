// npm performs installation/integrity checks; these guards bound the change.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');

const read = file => JSON.parse(fs.readFileSync(file, 'utf8'));
const hash = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');

function inventory(root) {
    const lock = read(path.join(root, 'package-lock.json'));
    const installed = {};
    for (const [location, entry] of Object.entries(lock.packages)) {
        const file = path.join(root, location, 'package.json');
        if (!location || !fs.existsSync(file)) continue;
        const actual = read(file);
        assert.equal(actual.version, entry.version, `Installed/locked version mismatch: ${location}`);
        installed[location] = { version: actual.version, integrity: entry.integrity, packageHash: hash(file) };
    }
    return installed;
}

function resolvePackage(root, parent, name) {
    let current = path.resolve(root, parent);
    while (current === root || current.startsWith(root + path.sep)) {
        const candidate = path.join(current, 'node_modules', name, 'package.json');
        if (fs.existsSync(candidate)) return path.relative(root, path.dirname(candidate)).split(path.sep).join('/');
        current = path.dirname(current);
    }
    throw new Error(`Cannot resolve ${name} from ${parent}`);
}

function prepare(root, manifest, snapshot) {
    const packageFile = path.join(root, 'package.json');
    const original = read(packageFile);
    assert.equal(original.version, manifest.upstreamVersion, 'Upstream version changed; requalify patches');
    const semver = require(path.join(root, 'node_modules/semver'));
    const before = inventory(root);
    const lock = read(path.join(root, 'package-lock.json'));
    const omissions = {};
    for (const location of manifest.keepAbsent) {
        assert(!before[location], `Upstream optional omission changed: ${location}`);
        assert.equal(lock.packages[location]?.optional, true, `Not an upstream optional package: ${location}`);
        omissions[location] = lock.packages[location];
    }
    for (const patch of manifest.packages) {
        assert.equal(before[patch.before]?.version, patch.from, `Unexpected base: ${patch.name}`);
        let consumers = 0;
        for (const location of ['', ...Object.keys(before)]) {
            const parent = read(path.join(root, location, 'package.json'));
            const range = { ...parent.dependencies, ...parent.optionalDependencies }[patch.name];
            if (!range || resolvePackage(root, location, patch.name) !== patch.before) continue;
            assert(semver.satisfies(patch.version, range), `Unsupported range ${range}: ${location} -> ${patch.name}`);
            consumers++;
        }
        assert(consumers > 0, `No installed consumer: ${patch.name}`);
    }
    for (const key of Object.keys(manifest.overrides)) {
        assert(!Object.hasOwn(original.overrides || {}, key), `Upstream override changed: ${key}`);
    }
    fs.writeFileSync(snapshot, JSON.stringify({ before, original, omissions }));
    original.overrides = { ...original.overrides, ...manifest.overrides };
    fs.writeFileSync(packageFile, JSON.stringify(original, null, 2) + '\n');
}

function verify(root, manifest, snapshot) {
    const { before, original, omissions } = read(snapshot);
    const lock = read(path.join(root, 'package-lock.json'));
    // npm reifies these already-locked native build helpers even with scripts
    // disabled. Preserve the upstream image's exact optional omissions.
    for (const location of manifest.keepAbsent) {
        const entry = lock.packages[location];
        assert.equal(entry?.optional, true);
        for (const key of ['version', 'resolved', 'integrity']) assert.equal(entry[key], omissions[location][key]);
        const target = path.resolve(root, location);
        assert(target.startsWith(path.join(root, 'node_modules') + path.sep));
        if (fs.existsSync(target)) {
            assert(fs.lstatSync(target).isDirectory() && !fs.lstatSync(target).isSymbolicLink());
            assert.equal(read(path.join(target, 'package.json')).version, entry.version);
            fs.rmSync(target, { recursive: true });
        }
    }
    const hiddenFile = path.join(root, 'node_modules/.package-lock.json');
    const hidden = read(hiddenFile);
    for (const location of manifest.keepAbsent) delete hidden.packages[location];
    fs.writeFileSync(hiddenFile, JSON.stringify(hidden, null, 2) + '\n');
    const after = inventory(root);
    const current = read(path.join(root, 'package.json'));
    assert.deepEqual(current, { ...original, overrides: { ...original.overrides, ...manifest.overrides } });
    const allowed = new Set(manifest.packages.flatMap(p => [p.before, p.after]));
    for (const location of new Set([...Object.keys(before), ...Object.keys(after)])) {
        if (!allowed.has(location)) assert.deepEqual(after[location], before[location], `Unrelated package changed: ${location}`);
    }
    for (const patch of manifest.packages) {
        assert.equal(after[patch.after]?.version, patch.version, `Patch not installed: ${patch.name}`);
        assert.equal(after[patch.after].integrity, patch.integrity, `Unexpected archive integrity: ${patch.name}`);
        assert.equal(lock.packages[patch.after].resolved,
            `https://registry.npmjs.org/${patch.name}/-/${patch.name}-${patch.version}.tgz`);
        if (patch.before !== patch.after) assert(!after[patch.before], `Old package remains: ${patch.before}`);
    }
    const semver = require(path.join(root, 'node_modules/semver'));
    for (const patch of manifest.packages) {
        const actual = read(path.join(root, patch.after, 'package.json'));
        for (const [name, range] of Object.entries(actual.dependencies || {})) {
            const resolved = resolvePackage(root, patch.after, name);
            assert(semver.satisfies(after[resolved]?.version, range), `Unsatisfied patch dependency: ${patch.name} -> ${name}`);
        }
    }
    console.log('Only the three qualified patch packages changed; versions, archive integrity and dependency ranges verified.');
}

if (require.main === module) {
    const [operation, manifestFile, snapshot] = process.argv.slice(2);
    assert(['prepare', 'verify'].includes(operation));
    ({ prepare, verify })[operation](process.cwd(), read(manifestFile), snapshot);
}
module.exports = { prepare, verify, inventory, resolvePackage };
