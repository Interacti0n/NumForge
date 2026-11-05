// Exercise the real callback API; all databases and files are disposable.
const test = require('node:test'), assert = require('node:assert/strict');
const fs = require('node:fs'), os = require('node:os'), path = require('node:path');
const {createRequire} = require('node:module');
const sqlite = createRequire('/app/package.json')('@louislam/sqlite3');
function fixture(t) {
    const cleanups = [];
    t.after(async () => {while (cleanups.length) await cleanups.pop()();});
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'nf-binding-'));
    cleanups.push(() => fs.rmSync(dir, {recursive: true}));
    return {dir, open(file = ':memory:', mode = sqlite.OPEN_READWRITE | sqlite.OPEN_CREATE) {
        const db = new sqlite.Database(file, mode);
        cleanups.push(() => new Promise((resolve, reject) => db.close(e => e ? reject(e) : resolve())));
        return db;
    }};
}
const exec = (db, sql) => new Promise((resolve, reject) => db.exec(sql, e => e ? reject(e) : resolve()));
const get = (db, sql, params = []) => new Promise((resolve, reject) => db.get(sql, params, (e, r) => e ? reject(e) : resolve(r)));
const run = (db, sql, params = []) => new Promise((resolve, reject) => db.run(sql, params, function (e) {e ? reject(e) : resolve({id: this.lastID, changes: this.changes});}));
test('binding prepared statement binds, resets, reuses and finalizes', async t => {
    const db = fixture(t).open();
    await exec(db, 'CREATE TABLE t(id INTEGER PRIMARY KEY, value TEXT)');
    const stmt = await new Promise((resolve, reject) => {const s = db.prepare('INSERT INTO t(value) VALUES(?)', e => e ? reject(e) : resolve(s));});
    for (const value of ['first', 'výpočet 😀']) {
        await new Promise((resolve, reject) => stmt.bind(value, e => e ? reject(e) : resolve()));
        await new Promise((resolve, reject) => stmt.run(function (e) {e ? reject(e) : resolve(assert.equal(this.changes, 1));}));
        await new Promise((resolve, reject) => stmt.reset(e => e ? reject(e) : resolve()));
    }
    await new Promise((resolve, reject) => stmt.finalize(e => e ? reject(e) : resolve()));
    assert.equal((await get(db, 'SELECT value FROM t WHERE id=2')).value, 'výpočet 😀');
});
test('binding serialized operations retain order and method chaining', async t => {
    const db = fixture(t).open();
    const rows = await new Promise((resolve, reject) => {
        assert.equal(db.serialize(() => {
            db.run('CREATE TABLE t(v)'); db.run('INSERT INTO t VALUES(1)'); db.run('INSERT INTO t VALUES(2)');
            db.all('SELECT v FROM t ORDER BY rowid', (e, r) => e ? reject(e) : resolve(r));
        }), db);
    });
    assert.deepEqual(rows, [{v: 1}, {v: 2}]);
    assert.equal(db.parallelize(), db);
    assert.equal((await get(db, 'SELECT count(*) AS n FROM t')).n, 2);
});
test('binding each invokes row callbacks before the completion count', async t => {
    const db = fixture(t).open(), values = [];
    await new Promise((resolve, reject) => {
        assert.equal(db.each('SELECT 1 AS v UNION ALL SELECT 2 UNION ALL SELECT 3',
            (e, row) => {if (e) reject(e); else values.push(row.v);},
            (e, count) => {if (e) reject(e); else {assert.equal(count, 3); resolve();}}), db);
    });
    assert.deepEqual(values, [1, 2, 3]);
});
test('binding native backup API preserves rows in a read-only destination', async t => {
    const f = fixture(t), db = f.open(), file = path.join(f.dir, 'backup.sqlite');
    await exec(db, "CREATE TABLE t(v); INSERT INTO t VALUES('backup value')");
    const backup = await new Promise((resolve, reject) => {const b = db.backup(file, e => e ? reject(e) : resolve(b));});
    await new Promise((resolve, reject) => backup.step(-1, e => e ? reject(e) : resolve()));
    await new Promise((resolve, reject) => backup.finish(e => e ? reject(e) : resolve()));
    const restored = f.open(file, sqlite.OPEN_READONLY);
    assert.equal((await get(restored, 'SELECT v FROM t')).v, 'backup value');
    await assert.rejects(run(restored, "INSERT INTO t VALUES('forbidden')"), e => e.code === 'SQLITE_READONLY');
});
test('binding busy timeout reports a writer lock and recovers after rollback', async t => {
    const f = fixture(t), file = path.join(f.dir, 'busy.sqlite'), a = f.open(file), b = f.open(file);
    await exec(a, 'CREATE TABLE t(v); BEGIN IMMEDIATE; INSERT INTO t VALUES(1)');
    b.configure('busyTimeout', 50);
    await assert.rejects(run(b, 'INSERT INTO t VALUES(2)'), e => e.code === 'SQLITE_BUSY');
    await exec(a, 'ROLLBACK'); await run(b, 'INSERT INTO t VALUES(3)');
    assert.equal((await get(b, 'SELECT v FROM t')).v, 3);
});
test('binding errors arrive once and empty get returns undefined', async t => {
    const db = fixture(t).open(); let callbacks = 0;
    await new Promise(resolve => db.get('SELECT * FROM missing_table', e => {
        callbacks++; assert.equal(e.code, 'SQLITE_ERROR'); resolve();
    }));
    await new Promise(resolve => setImmediate(resolve)); assert.equal(callbacks, 1);
    assert.equal(await get(db, 'SELECT 1 WHERE 0'), undefined);
});
test('binding parameter values remain data and report mutation context', async t => {
    const db = fixture(t).open(); await exec(db, 'CREATE TABLE t(id INTEGER PRIMARY KEY, v TEXT)');
    const value = "'); DROP TABLE t; --";
    assert.deepEqual(await run(db, 'INSERT INTO t(v) VALUES(?)', [value]), {id: 1, changes: 1});
    assert.equal((await get(db, 'SELECT v FROM t')).v, value);
});
test('binding interrupt cancels work and leaves the connection usable', {timeout: 5000}, async t => {
    const db = fixture(t).open();
    const query = get(db, 'WITH RECURSIVE t(n) AS (VALUES(1) UNION ALL SELECT n+1 FROM t WHERE n<100000000) SELECT sum(n) AS total FROM t');
    const timer = setTimeout(() => db.interrupt(), 20);
    try {await assert.rejects(query, e => e.code === 'SQLITE_INTERRUPT');} finally {clearTimeout(timer);}
    assert.equal((await get(db, 'SELECT 42 AS v')).v, 42);
});
