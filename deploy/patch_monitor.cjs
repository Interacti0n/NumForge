// npm performs installation/integrity checks; these guards bound the change.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');

const read = file => JSON.parse(fs.readFileSync(file, 'utf8'));
const hash = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const archiveUrl = entry => `https://registry.npmjs.org/${entry.name}/-/${entry.name.split('/').at(-1)}-${entry.version}.tgz`;

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

function configuredPackage(original, manifest, semver) {
    const configured = structuredClone(original);
    const seen = new Set();
    for (const migration of manifest.directMigrations || []) {
        assert(!seen.has(migration.name), `Duplicate direct migration: ${migration.name}`);
        seen.add(migration.name);
        const patch = manifest.packages.find(p => p.name === migration.name);
        assert(patch && patch.before === `node_modules/${migration.name}` && patch.after === patch.before,
            `Direct migration must identify a pinned root package: ${migration.name}`);
        assert.equal(original.dependencies?.[migration.name], migration.fromRange, `Upstream direct range changed: ${migration.name}`);
        assert.equal(migration.version, patch.version, 'Direct migration must use the pinned exact version');
        assert(semver.valid(patch.version) && semver.major(patch.from) > 0 && semver.major(patch.from) === semver.major(patch.version),
            `Direct migration must stay within a stable major: ${migration.name}`);
        configured.dependencies[migration.name] = patch.version;
    }
    configured.overrides = { ...original.overrides, ...manifest.overrides };
    return configured;
}

function prepare(root, manifest, snapshot) {
    const packageFile = path.join(root, 'package.json');
    const original = read(packageFile);
    assert.equal(original.version, manifest.upstreamVersion, 'Upstream version changed; requalify patches');
    const semver = require(path.join(root, 'node_modules/semver'));
    const configured = configuredPackage(original, manifest, semver);
    for (const patch of manifest.sourcePatches || []) {
        assert.equal(hash(path.join(root, patch.path)), patch.beforeHash, `Upstream source changed: ${patch.path}`);
    }
    const before = inventory(root);
    const deduplicated = {};
    for (const shared of manifest.deduplicatedDependencies || []) {
        assert(!Object.hasOwn(deduplicated, shared.from), `Duplicate shared package: ${shared.from}`);
        const destination = manifest.packages.find(p => p.name === shared.name && p.after === shared.to);
        assert(destination && shared.from !== shared.to, `Shared package has no migrated destination: ${shared.name}`);
        assert.equal(shared.version, destination.version, 'Shared version must match the pinned destination');
        assert.equal(shared.integrity, destination.integrity, 'Shared integrity must match the pinned destination');
        assert.equal(before[shared.from]?.version, shared.version, `Unexpected shared version: ${shared.from}`);
        assert.equal(before[shared.from]?.integrity, shared.integrity, `Unexpected shared integrity: ${shared.from}`);
        const consumers = [];
        for (const location of ['', ...Object.keys(before)]) {
            const parent = read(path.join(root, location, 'package.json'));
            const range = {...parent.dependencies, ...parent.optionalDependencies}[shared.name];
            if (!range || resolvePackage(root, location, shared.name) !== shared.from) continue;
            assert(semver.satisfies(shared.version, range), `Unsupported shared range ${range}: ${location}`);
            consumers.push(location);
        }
        assert(consumers.length, `Shared package has no original consumer: ${shared.from}`);
        deduplicated[shared.from] = consumers;
    }
    const additions = new Set();
    for (const added of manifest.addedDependencies || []) {
        const owner = manifest.packages.find(p => p.name === added.addedBy);
        const rootPath = `node_modules/${added.name}`;
        const ownerPath = owner && `${owner.after}/node_modules/${added.name}`;
        assert(added.path === rootPath || added.path === ownerPath, 'Added dependency must use its exact root or owner path');
        assert(!additions.has(added.path), `Duplicate added dependency: ${added.name}`);
        additions.add(added.path);
        assert(!before[added.path] && !fs.existsSync(path.join(root, added.path)), `Added dependency already exists: ${added.name}`);
        assert(manifest.directMigrations?.some(m => m.name === added.addedBy), `Added dependency must belong to a direct migration: ${added.name}`);
        const pin = added.path === rootPath ? manifest.overrides[added.name] : manifest.overrides[added.addedBy]?.[added.name];
        assert.equal(pin, added.version, 'Added dependency must be pinned exactly');
    }
    for (const pruned of manifest.prunedDependencies || []) {
        assert.equal(before[pruned.path]?.version, pruned.version);
        assert.equal(before[pruned.path]?.integrity, pruned.integrity);
        const owner = manifest.packages.find(p => p.name === pruned.removedBy);
        assert(owner, `Pruned dependency has no migrated owner: ${pruned.name}`);
        assert(read(path.join(root, owner.before, 'package.json')).dependencies?.[pruned.name],
            `Pruned package is not an old owner dependency: ${pruned.name}`);
    }
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
            const consumer = location === '' ? configured : parent;
            const range = { ...consumer.dependencies, ...consumer.optionalDependencies }[patch.name];
            if (!range || resolvePackage(root, location, patch.name) !== patch.before) continue;
            assert(semver.satisfies(patch.version, range), `Unsupported range ${range}: ${location} -> ${patch.name}`);
            consumers++;
        }
        assert(consumers > 0, `No installed consumer: ${patch.name}`);
    }
    for (const key of Object.keys(manifest.overrides)) {
        assert(!Object.hasOwn(original.overrides || {}, key), `Upstream override changed: ${key}`);
    }
    fs.writeFileSync(snapshot, JSON.stringify({ before, original, omissions, deduplicated }));
    fs.writeFileSync(packageFile, JSON.stringify(configured, null, 2) + '\n');
}

function verify(root, manifest, snapshot) {
    const { before, original, omissions, deduplicated = {} } = read(snapshot);
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
    const semver = require(path.join(root, 'node_modules/semver'));
    assert.deepEqual(current, configuredPackage(original, manifest, semver));
    const allowed = new Set(manifest.packages.flatMap(p => [p.before, p.after]));
    for (const shared of manifest.deduplicatedDependencies || []) {
        allowed.add(shared.from);
        assert(!after[shared.from], `Duplicate package remains: ${shared.from}`);
        assert.deepEqual(after[shared.to], before[shared.from], `Shared package identity changed: ${shared.from}`);
        assert(deduplicated[shared.from]?.length, `Shared package has no verified consumers: ${shared.from}`);
        for (const location of deduplicated[shared.from]) {
            assert.equal(resolvePackage(root, location, shared.name), shared.to, `Shared consumer resolves elsewhere: ${location}`);
            const parent = read(path.join(root, location, 'package.json'));
            const range = {...parent.dependencies, ...parent.optionalDependencies}[shared.name];
            assert(range && semver.satisfies(shared.version, range), `Unsupported shared consumer: ${location}`);
        }
    }
    for (const added of manifest.addedDependencies || []) {
        allowed.add(added.path);
        assert(!before[added.path], `Added dependency already existed: ${added.name}`);
        assert.equal(after[added.path]?.version, added.version, `Added dependency not installed: ${added.name}`);
        assert.equal(after[added.path].integrity, added.integrity, `Unexpected archive integrity: ${added.name}`);
        assert.equal(lock.packages[added.path].resolved, archiveUrl(added));
        const owner = manifest.packages.find(p => p.name === added.addedBy);
        const range = read(path.join(root, owner.after, 'package.json')).dependencies?.[added.name];
        assert(range && semver.satisfies(added.version, range), `Added dependency not required by migrated owner: ${added.name}`);
        assert.equal(resolvePackage(root, owner.after, added.name), added.path);
    }
    for (const pruned of manifest.prunedDependencies || []) {
        allowed.add(pruned.path);
        assert(!after[pruned.path], `Unused upstream dependency remains: ${pruned.name}`);
        for (const location of ['', ...Object.keys(after)]) {
            const parent = read(path.join(root, location, 'package.json'));
            assert(!parent.dependencies?.[pruned.name], `Removed package still required: ${location} -> ${pruned.name}`);
        }
    }
    for (const location of new Set([...Object.keys(before), ...Object.keys(after)])) {
        if (!allowed.has(location)) assert.deepEqual(after[location], before[location], `Unrelated package changed: ${location}`);
    }
    for (const patch of manifest.packages) {
        assert.equal(after[patch.after]?.version, patch.version, `Patch not installed: ${patch.name}`);
        assert.equal(after[patch.after].integrity, patch.integrity, `Unexpected archive integrity: ${patch.name}`);
        assert.equal(lock.packages[patch.after].resolved, archiveUrl(patch));
        if (patch.before !== patch.after) assert(!after[patch.before], `Old package remains: ${patch.before}`);
    }
    for (const patch of [...manifest.packages, ...(manifest.addedDependencies || []).map(p => ({...p, after: p.path}))]) {
        const actual = read(path.join(root, patch.after, 'package.json'));
        for (const [name, range] of Object.entries(actual.dependencies || {})) {
            const resolved = resolvePackage(root, patch.after, name);
            assert(semver.satisfies(after[resolved]?.version, range), `Unsatisfied patch dependency: ${patch.name} -> ${name}`);
        }
    }
    for (const patch of manifest.sourcePatches || []) {
        const file = path.resolve(root, patch.path);
        assert(file.startsWith(path.join(root, 'server') + path.sep));
        assert.equal(hash(file), patch.beforeHash, `Upstream source changed: ${patch.path}`);
        const source = fs.readFileSync(file, 'utf8');
        assert.equal(source.split(patch.before).length, 2, `Source patch must match once: ${patch.path}`);
        const changed = source.replace(patch.before, patch.after);
        assert.equal(crypto.createHash('sha256').update(changed).digest('hex'), patch.afterHash);
        fs.writeFileSync(file, changed);
    }
    console.log(`The ${manifest.packages.length} qualified packages, archive integrity, consumer ranges and pinned source adaptations verified.`);
}

if (require.main === module) {
    const [operation, manifestFile, snapshot] = process.argv.slice(2);
    assert(['prepare', 'verify'].includes(operation));
    ({ prepare, verify })[operation](process.cwd(), read(manifestFile), snapshot);
}
module.exports = { prepare, verify, inventory, resolvePackage };
