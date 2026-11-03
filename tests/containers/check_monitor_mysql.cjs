// Disposable protocol endpoints use only a fixture password and container loopback.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const net = require('node:net');
const zlib = require('node:zlib');
const {createRequire} = require('node:module');
const appRequire = createRequire('/app/package.json');
const receipt = '/opt/numforge/monitor-patches.json';
const manifest = fs.existsSync(receipt) ? JSON.parse(fs.readFileSync(receipt, 'utf8')) : {};
if (manifest.directMigrations?.some(p => p.name === 'mysql2') || process.env.NUMFORGE_FORCE_MYSQL_CHECKS) {
    const mysql = appRequire('mysql2');
    const mysqlRequire = createRequire(appRequire.resolve('mysql2'));

    test('MySQL migration pins the driver and isolates the new charset dependency', () => {
        assert.equal(appRequire('mysql2/package.json').version, '3.23.1');
        assert.equal(mysqlRequire('iconv-lite/package.json').version, '0.7.2');
        assert.equal(appRequire('iconv-lite/package.json').version, '0.6.3');
        assert.equal(JSON.parse(fs.readFileSync('/app/node_modules/sql-escaper/package.json', 'utf8')).version, '1.5.1');
        assert.notEqual(mysqlRequire.resolve('iconv-lite'), appRequire.resolve('iconv-lite'));
        assert.throws(() => appRequire.resolve('seq-queue'), {code: 'MODULE_NOT_FOUND'});
    });

    const packet = (sequence, payload) => {
        const header = Buffer.alloc(4); header.writeUIntLE(payload.length, 0, 3); header[3] = sequence;
        return Buffer.concat([header, payload]);
    };
    function greeting() {
        // Independent protocol-v10 bytes, not encoded by the client under test.
        const tail = Buffer.alloc(18);
        tail.writeUInt16LE(0x820d, 0); tail[2] = 45; tail.writeUInt16LE(2, 3);
        tail.writeUInt16LE(8, 5); tail[7] = 21;
        return packet(0, Buffer.concat([Buffer.from([10]), Buffer.from('8.0.0-fixture\0'), Buffer.from([1, 0, 0, 0]),
            Buffer.from('abcdefgh\0'), tail, Buffer.from('ijklmnopqrst\0mysql_native_password\0')]));
    }
    async function downgrade(t, invoke) {
        const sockets = new Set();
        const password = 'only-a-disposable-fixture-password';
        let switched = false, leaked = false;
        const server = net.createServer(socket => {
            sockets.add(socket); socket.on('error', () => {}); socket.on('close', () => sockets.delete(socket));
            socket.write(greeting());
            let pending = Buffer.alloc(0), received = 0;
            socket.on('data', chunk => {
                pending = Buffer.concat([pending, chunk]);
                while (pending.length >= 4) {
                    const length = pending.readUIntLE(0, 3);
                    if (length > 4096) {socket.destroy(); return;}
                    if (pending.length < length + 4) return;
                    const payload = pending.subarray(4, 4 + length); pending = pending.subarray(4 + length);
                    if (++received === 1) {
                        switched = true;
                        socket.write(packet(2, Buffer.from('\xfemysql_clear_password\0challenge\0', 'latin1')));
                    } else {
                        leaked = payload.equals(Buffer.from(password + '\0'));
                        const error = Buffer.concat([Buffer.from([0xff, 0x15, 0x04]), Buffer.from('#28000fixture rejected')]);
                        socket.write(packet(4, error));
                    }
                }
            });
        });
        t.after(() => {for (const socket of sockets) socket.destroy(); server.close();});
        await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
        const error = await invoke(server.address().port, password);
        assert.equal(switched, true, 'The malicious auth switch must actually be delivered');
        assert.equal(leaked, false, 'The peer must not receive the fixture password in cleartext');
        assert.match(error.message, /mysql_clear_password|cleartext/i);
    }
    test('client refuses an unsolicited clear-password downgrade before sending credentials', {timeout: 4000}, async t => {
        await downgrade(t, (port, password) => new Promise(resolve => {
            const connection = mysql.createConnection({host: '127.0.0.1', port, user: 'fixture', password, connectTimeout: 1000});
            connection.on('error', () => {});
            connection.connect(error => {connection.destroy(); resolve(error);});
        }));
    });
    test('actual Kuma MySQL monitor also refuses a clear-password downgrade', {timeout: 4000}, async t => {
        const {MysqlMonitorType} = appRequire('./server/monitor-types/mysql');
        await downgrade(t, async (port, password) => {
            try {
                await new MysqlMonitorType().check({databaseConnectionString: 'mysql://fixture@127.0.0.1:' + port + '/fixture',
                    radiusPassword: password, databaseQuery: 'SELECT 1'}, {}, {});
                throw new Error('The malicious endpoint must be rejected');
            } catch (error) { return error; }
        });
    });

    async function inflate(advertised, compressed) {
        const {enableCompression} = mysqlRequire(path.join(path.dirname(appRequire.resolve('mysql2')), 'lib/compressed_protocol.js'));
        let timer;
        try {
            return await new Promise((resolve, reject) => {
                timer = setTimeout(() => reject(new Error('Compression fixture did not complete')), 1000);
                const connection = {write() {}, handlePacket() {}, _bumpCompressedSequenceId() {}, _handleNetworkError: error => resolve({error})};
                enableCompression(connection);
                connection._inflatedPacketsParser.execute = data => resolve({data});
                connection._handleCompressedPacket({readInt24: () => advertised, readBuffer: () => compressed, numPackets: 1});
            });
        } finally { clearTimeout(timer); }
    }
    test('compressed packets cannot inflate beyond their advertised size', async () => {
        const result = await inflate(8, zlib.deflateSync(Buffer.alloc(1024, 65)));
        assert.equal(result.error?.code, 'ERR_BUFFER_TOO_LARGE');
        assert.equal(result.data, undefined);
    });
    test('valid compressed, uncompressed and corrupt packets keep expected behavior', async () => {
        const plain = Buffer.from('fixture-data');
        assert.deepEqual((await inflate(plain.length, zlib.deflateSync(plain))).data, plain);
        assert.deepEqual((await inflate(0, plain)).data, plain);
        assert.equal((await inflate(8, Buffer.from([1, 2, 3]))).error?.code, 'Z_DATA_ERROR');
    });
    test('SQL formatting keeps literal values and named placeholders separate from SQL syntax', () => {
        assert.equal(mysql.escape("O'Reilly\0"), "'O\\'Reilly\\0'");
        assert.equal(mysql.format('SELECT ? AS value, ?? AS column_name', ['a; DROP TABLE fixture', 'column`name']),
            "SELECT 'a; DROP TABLE fixture' AS value, `column``name` AS column_name");
        assert.equal(mysql.escape(Buffer.from([0, 255, 65])), "X'00ff41'");
    });
} else {
    console.log('Original MySQL range: migration regressions deferred to the qualified candidate.');
}
