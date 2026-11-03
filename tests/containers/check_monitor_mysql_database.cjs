// Run on frozen monitor images against the separately owned disposable MySQL DB.
const test = require('node:test');
const assert = require('node:assert/strict');
const {createRequire} = require('node:module');
const appRequire = createRequire('/app/package.json');
const mysql = appRequire('mysql2/promise');
const {MysqlMonitorType} = appRequire('./server/monitor-types/mysql');
const uri = process.env.NUMFORGE_TEST_MYSQL_URI;
assert(uri && new URL(uri).hostname === 'mysql-fixture', 'Require the isolated fixture database');
const options = {uri, supportBigNumbers: true, bigNumberStrings: true, charset: 'utf8mb4'};

test('real MySQL binary protocol preserves exact integer, decimal, Unicode and binary values', async () => {
    const connection = await mysql.createConnection(options);
    try {
        const [rows] = await connection.execute("SELECT CAST('9007199254740993' AS UNSIGNED) AS exact_integer, CAST('1234.56789' AS DECIMAL(12,5)) AS exact_decimal, CAST(? AS CHAR CHARACTER SET utf8mb4) AS text_value, X'00FF41' AS binary_value", ['Zažltnutý výpočet 😀']);
        assert.equal(rows[0].exact_integer, '9007199254740993');
        assert.equal(rows[0].exact_decimal, '1234.56789');
        assert.equal(rows[0].text_value, 'Zažltnutý výpočet 😀');
        assert.deepEqual(rows[0].binary_value, Buffer.from([0, 255, 65]));
    } finally {await connection.end();}
});
test('real compressed MySQL query round trips a larger UTF-8 value', async () => {
    const connection = await mysql.createConnection({...options, compress: true});
    try {
        const value = 'výpočet-😀'.repeat(4096);
        const [rows] = await connection.execute('SELECT ? AS value', [value]);
        assert.equal(rows[0].value, value);
    } finally {await connection.end();}
});
test('prepared values, named placeholders and transaction rollback work through a pool', async () => {
    const pool = mysql.createPool({...options, connectionLimit: 1, namedPlaceholders: true});
    try {
        await pool.query('DROP TABLE IF EXISTS migration_fixture');
        await pool.query('CREATE TABLE migration_fixture (id INT PRIMARY KEY, value TEXT CHARACTER SET utf8mb4)');
        const connection = await pool.getConnection();
        try {
            await connection.beginTransaction();
            const value = "O'Reilly'; DROP TABLE migration_fixture; -- 😀";
            await connection.execute('INSERT INTO migration_fixture VALUES (:id, :value)', {id: 1, value});
            const [rows] = await connection.execute('SELECT value FROM migration_fixture WHERE id=:id', {id: 1});
            assert.equal(rows[0].value, value);
            await connection.rollback();
        } finally {connection.release();}
        const [rows] = await pool.query('SELECT COUNT(*) AS total FROM migration_fixture');
        assert.equal(rows[0].total, '0'); // COUNT is BIGINT; bigNumberStrings is enabled.
    } finally {await pool.end();}
});
test('actual Kuma MySQL monitor reports row counts, single values and query failures', async () => {
    const type = new MysqlMonitorType();
    const heartbeat = {};
    await type.check({databaseConnectionString: uri, databaseQuery: ''}, heartbeat, {});
    assert.equal(heartbeat.status, 1); assert.equal(heartbeat.msg, 'Rows: 1');
    assert.equal(await type.mysqlQuerySingleValue(uri, 'SELECT 42 AS value'), 42);
    await assert.rejects(type.mysqlQuerySingleValue(uri, 'SELECT 1 WHERE 0'), /no results/);
    await assert.rejects(type.mysqlQuerySingleValue(uri, 'SELECT 1 UNION ALL SELECT 2'), /Multiple values/);
    await assert.rejects(type.mysqlQuerySingleValue(uri, 'SELECT 1 AS a, 2 AS b'), /Multiple columns/);
    await assert.rejects(type.check({databaseConnectionString: uri, databaseQuery: 'SELECT * FROM missing_fixture_table'}, {}, {}), /Database connection\/query failed/);
});
test('real MySQL authentication failures cannot become healthy results', async () => {
    const wrong = new URL(uri); wrong.password = 'wrong-fixture-password';
    await assert.rejects(mysql.createConnection({uri: wrong.href}), error => error.code === 'ER_ACCESS_DENIED_ERROR');
    const heartbeat = {};
    await assert.rejects(new MysqlMonitorType().check({databaseConnectionString: wrong.href}, heartbeat, {}), /Access denied/);
    assert.notEqual(heartbeat.status, 1);
});
