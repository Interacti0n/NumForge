// Native engine verification uses only in-memory or disposable fixture databases.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs'), os = require('node:os'), path = require('node:path'), crypto = require('node:crypto');
const {createRequire} = require('node:module');
const app = createRequire('/app/package.json'), sqlite = app('@louislam/sqlite3');
const receiptPath = '/opt/numforge/monitor-sqlite.json';
const guards = globalThis.monitorSqliteGuards;
const hash = data => crypto.createHash('sha256').update(data).digest('hex');
const cleanups = Symbol('SQLite fixture cleanup');
function cleanup(t, callback) {
    if (!t[cleanups]) {t[cleanups] = []; t.after(async () => {for (const fn of t[cleanups].reverse()) await fn();});}
    t[cleanups].push(callback);
}
function directory(t) {const root = fs.mkdtempSync(path.join(os.tmpdir(), 'nf-sqlite-')); cleanup(t, () => fs.rmSync(root, {recursive: true, force: true})); return root;}
if (guards) {
    function fixture(t) {
        const root = directory(t);
        const files = {'package.json': JSON.stringify({version: '15.1.6'}), 'binding.gyp': 'binding',
            'src/binding.cc': 'source', 'lib/sqlite3.js': 'wrapper', 'deps/sqlite3.gyp': 'gyp', 'deps/extract.js': 'extract',
            'deps/common-sqlite.gypi': "'sqlite_version%':'3410100'", 'deps/sqlite-autoconf-3410100.tar.gz': 'original archive',
            'lib/binding/napi-v6-linux-x64/node_sqlite3.node': 'original binary'};
        for (const [name, data] of Object.entries(files)) {const file = path.join(root, name); fs.mkdirSync(path.dirname(file), {recursive: true}); fs.writeFileSync(file, data);}
        const manifest = {bindingVersion: '15.1.6', wrapperSha256: guards.wrapperHash(root),
            beforeSha256: Object.fromEntries(['deps/common-sqlite.gypi', 'deps/sqlite-autoconf-3410100.tar.gz', 'lib/binding/napi-v6-linux-x64/node_sqlite3.node'].map(name => [name, hash(files[name])]))};
        return {root, manifest, files};
    }
    for (const name of ['src/binding.cc', 'deps/sqlite-autoconf-3410100.tar.gz', 'lib/binding/napi-v6-linux-x64/node_sqlite3.node']) {
        test('native rebuild rejects changed original ' + name, t => {
            const f = fixture(t); guards.validateBase(f.root, f.manifest);
            fs.appendFileSync(path.join(f.root, name), 'changed');
            assert.throws(() => guards.validateBase(f.root, f.manifest), /Binding source changed|Upstream SQLite file changed/);
        });
    }
    test('native rebuild rejects a corrupt source download before replacing old files', async t => {
        const f = fixture(t);
        Object.assign(f.manifest, {arch: process.arch, node: process.version, archiveVersion: '3540100',
            url: 'https://www.sqlite.org/2026/sqlite-autoconf-3540100.tar.gz', archiveSha3: '0'.repeat(64)});
        const download = async () => ({ok: true, body: (async function* () {yield Buffer.from('not official source');})()});
        await assert.rejects(guards.prepare(f.root, f.manifest, download), /archive integrity mismatch/);
        for (const [name, original] of Object.entries(f.files)) assert.equal(fs.readFileSync(path.join(f.root, name), 'utf8'), original);
        assert(!fs.existsSync(path.join(f.root, 'deps/sqlite-autoconf-3540100.tar.gz')));
    });
}
const get = (db, sql, params = []) => new Promise((resolve, reject) => db.get(sql, params, (error, row) => error ? reject(error) : resolve(row)));
const all = (db, sql, params = []) => new Promise((resolve, reject) => db.all(sql, params, (error, rows) => error ? reject(error) : resolve(rows)));
const exec = (db, sql) => new Promise((resolve, reject) => db.exec(sql, error => error ? reject(error) : resolve()));
const run = (db, sql, params = []) => new Promise((resolve, reject) => db.run(sql, params, function (error) {error ? reject(error) : resolve({id: this.lastID, changes: this.changes});}));
function open(t, file = ':memory:', flags = sqlite.OPEN_READWRITE | sqlite.OPEN_CREATE) {
    const db = new sqlite.Database(file, flags); cleanup(t, () => new Promise((resolve, reject) => db.close(error => error ? reject(error) : resolve()))); return db;
}
if (fs.existsSync(receiptPath) || process.env.NUMFORGE_FORCE_SQLITE_CHECKS) {
    test('native SQLite is the pinned rebuilt engine with matching provenance', async t => {
        const db = open(t), identity = await get(db, 'SELECT sqlite_version() AS version, sqlite_source_id() AS source');
        assert.equal(identity.version, '3.54.1');
        assert.equal(identity.source, '2026-10-16 14:45:20 555e07ef33f69a0957d24d8e555b702464a131d6f1607f4c3c1e4551e8e6ca76');
        const receipt = JSON.parse(fs.readFileSync(receiptPath, 'utf8'));
        assert.deepEqual(receipt.runtime, identity);
        const root = path.dirname(app.resolve('@louislam/sqlite3/package.json'));
        assert.equal(app('@louislam/sqlite3/package.json').version, '15.1.6');
        assert.equal(hash(fs.readFileSync(path.join(root, receipt.binary))), receipt.nativeSha256);
        assert.equal(guards.wrapperHash(root), receipt.wrapperSha256);
        assert(!fs.existsSync(path.join(root, 'deps/sqlite-autoconf-3410100.tar.gz')));
        assert(!/libsqlite3|not found/.test(receipt.linkage));
    });
    test('SQLite preserves required compile features and original resource limits', async t => {
        const options = (await all(open(t), 'PRAGMA compile_options')).map(row => row.compile_options);
        for (const option of ['THREADSAFE=1', 'ENABLE_FTS3', 'ENABLE_FTS4', 'ENABLE_FTS5', 'ENABLE_RTREE', 'ENABLE_DBSTAT_VTAB',
            'ENABLE_MATH_FUNCTIONS', 'MAX_COLUMN=2000', 'MAX_EXPR_DEPTH=1000', 'MAX_FUNCTION_ARG=127', 'MAX_PAGE_COUNT=1073741823', 'MAX_MMAP_SIZE=0x7fff0000', 'MAX_VARIABLE_NUMBER=32766']) assert(options.includes(option), option);
    });
    test('SQLite prepared bindings preserve Unicode, blobs, nulls and mutation callbacks', async t => {
        const db = open(t); await exec(db, 'CREATE TABLE fixture(id INTEGER PRIMARY KEY, label TEXT UNIQUE, bytes BLOB, nullable TEXT)');
        const row = await run(db, 'INSERT INTO fixture(label,bytes,nullable) VALUES(?,?,?)', ['Výpočet 😀', Buffer.from([0, 255, 1]), null]);
        assert.equal(row.changes, 1); assert.equal(row.id, 1);
        assert.deepEqual(await get(db, 'SELECT * FROM fixture WHERE label=?', ['Výpočet 😀']), {id: 1, label: 'Výpočet 😀', bytes: Buffer.from([0, 255, 1]), nullable: null});
        await assert.rejects(run(db, 'INSERT INTO fixture(label) VALUES(?)', ['Výpočet 😀']), error => error.code === 'SQLITE_CONSTRAINT');
        assert.equal((await run(db, 'UPDATE fixture SET nullable=$value WHERE id=$id', {$value: 'stored', $id: 1})).changes, 1);
    });
    test('SQLite transactions and foreign-key failures preserve committed rows', async t => {
        const db = open(t); await exec(db, 'PRAGMA foreign_keys=ON; CREATE TABLE p(id INTEGER PRIMARY KEY); CREATE TABLE c(id INTEGER REFERENCES p(id)); INSERT INTO p VALUES(1); BEGIN; INSERT INTO p VALUES(2); ROLLBACK;');
        await assert.rejects(run(db, 'INSERT INTO c VALUES(9)'), error => error.code === 'SQLITE_CONSTRAINT');
        assert.deepEqual(await all(db, 'SELECT id FROM p'), [{id: 1}]);
        assert.equal((await get(db, 'PRAGMA integrity_check')).integrity_check, 'ok');
    });
    test('SQLite WAL connections preserve snapshot reads across writes', async t => {
        const file = path.join(directory(t), 'wal.db'), writer = open(t, file), reader = open(t, file);
        assert.equal((await get(writer, 'PRAGMA journal_mode=WAL')).journal_mode, 'wal');
        await exec(writer, 'CREATE TABLE fixture(value INTEGER); INSERT INTO fixture VALUES(1)');
        await exec(reader, 'BEGIN'); assert.equal((await get(reader, 'SELECT value FROM fixture')).value, 1);
        await exec(writer, 'UPDATE fixture SET value=2'); assert.equal((await get(reader, 'SELECT value FROM fixture')).value, 1);
        await exec(reader, 'COMMIT'); assert.equal((await get(reader, 'SELECT value FROM fixture')).value, 2);
    });
    test('SQLite JSON, date/math, FTS and RTree features remain available', async t => {
        const db = open(t);
        assert.deepEqual(await get(db, `SELECT json_extract('{"label":"Výpočet"}', '$.label') AS label, sqrt(81) AS root, strftime('%Y-%m-%d','2026-11-03') AS day`), {label: 'Výpočet', root: 9, day: '2026-11-03'});
        await exec(db, "CREATE VIRTUAL TABLE search USING fts5(text); INSERT INTO search VALUES('fixture searchable'); CREATE VIRTUAL TABLE box USING rtree(id,x0,x1); INSERT INTO box VALUES(1,0,10)");
        assert.equal((await get(db, "SELECT count(*) AS n FROM search WHERE search MATCH 'searchable'")).n, 1);
        assert.equal((await get(db, 'SELECT id FROM box WHERE x0<=5 AND x1>=5')).id, 1);
    });
    test('SQLite consistent export opens read-only with intact rows and rejects writes', async t => {
        const root = directory(t), db = open(t), backup = path.join(root, 'backup.db');
        await exec(db, "CREATE TABLE fixture(value TEXT); INSERT INTO fixture VALUES('retained')");
        await run(db, 'VACUUM INTO ?', [backup]);
        const restored = open(t, backup, sqlite.OPEN_READONLY);
        assert.equal((await get(restored, 'SELECT value FROM fixture')).value, 'retained');
        assert.equal((await get(restored, 'PRAGMA integrity_check')).integrity_check, 'ok');
        await assert.rejects(run(restored, "INSERT INTO fixture VALUES('forbidden')"), error => error.code === 'SQLITE_READONLY');
    });
    for (const nested of [false, true]) {
        test('SQLite schema changes work through real Knex ' + (nested ? '2' : '3') + ' dialect', async t => {
            const owner = nested ? createRequire(app.resolve('redbean-node/package.json')) : app;
            const Dialect = owner('knex/lib/dialects/sqlite3/index.js');
            class FixtureDialect extends Dialect {_driver() {return sqlite;}}
            const knex = owner('knex')({client: FixtureDialect, connection: {filename: ':memory:'}, useNullAsDefault: true});
            t.after(() => knex.destroy());
            await knex.schema.createTable('fixture', table => {table.increments('id'); table.text('label'); table.text('obsolete');});
            await knex('fixture').insert({label: 'retained', obsolete: 'remove'});
            await knex.schema.alterTable('fixture', table => table.dropColumn('obsolete'));
            await knex.schema.alterTable('fixture', table => table.renameColumn('label', 'name'));
            assert.deepEqual(await knex('fixture').select('*'), [{id: 1, name: 'retained'}]);
            await assert.rejects(knex.transaction(async trx => {await trx('fixture').insert({name: 'rollback'}); throw new Error('fixture rollback');}), /fixture rollback/);
            assert.equal((await knex('fixture').count({n: '*'}))[0].n, 1);
        });
    }
} else console.log('Original native SQLite: engine upgrade checks deferred to the qualified candidate.');
