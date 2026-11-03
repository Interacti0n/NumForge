// Small, bounded fixtures; no deliberate OOM or traffic outside this container.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {once} = require('node:events');
const {createRequire} = require('node:module');
const appRequire = createRequire('/app/package.json');
const receipt = '/opt/numforge/monitor-patches.json';
const manifest = fs.existsSync(receipt) ? JSON.parse(fs.readFileSync(receipt, 'utf8')) : {};
if (manifest.directMigrations?.some(p => p.name === 'ws') || process.env.NUMFORGE_FORCE_WS_CHECKS) {
    const WebSocket = appRequire('ws');

    test('WebSocket migration shares exact ws 8.21.3 with Socket.IO and retains MQTT major', () => {
        assert.equal(appRequire('ws/package.json').version, '8.21.3');
        for (const consumer of ['engine.io', 'engine.io-client', 'socket.io-adapter']) {
            assert.equal(createRequire(appRequire.resolve(consumer)).resolve('ws'), appRequire.resolve('ws'), consumer);
        }
        const mqttRequire = createRequire(appRequire.resolve('mqtt'));
        assert.equal(mqttRequire('ws/package.json').version, '7.5.13');
        assert.notEqual(mqttRequire.resolve('ws'), appRequire.resolve('ws'));
    });

    async function receive(chunks, options) {
        const receiver = new WebSocket.Receiver({isServer: false, maxPayload: 4096, ...options});
        let failure;
        receiver.on('error', error => {failure = error;});
        try {
            for (const chunk of chunks) {
                const error = await new Promise(resolve => receiver.write(chunk, resolve));
                if (error) return error;
            }
            return failure;
        } finally { receiver.destroy(); }
    }
    test('tiny message fragments respect a configured structural limit', async () => {
        const frames = [Buffer.from([0x02, 1, 65]), ...Array.from({length: 4}, () => Buffer.from([0, 1, 66]))];
        const error = await receive(frames, {maxFragments: 4});
        assert.equal(error?.code, 'WS_ERR_TOO_MANY_BUFFERED_PARTS');
        assert.match(error.message, /fragments/i);
    });
    test('empty message fragments count toward the structural limit', async () => {
        const frames = [Buffer.from([0x02, 0]), ...Array.from({length: 4}, () => Buffer.from([0, 0]))];
        const error = await receive(frames, {maxFragments: 4});
        assert.equal(error?.code, 'WS_ERR_TOO_MANY_BUFFERED_PARTS');
        assert.match(error.message, /fragments/i);
    });
    test('tiny buffered chunks respect a configured structural limit', async () => {
        // Announce a 16-byte payload, then supply only five one-byte chunks.
        const error = await receive([Buffer.from([0x82, 16]), ...Array.from({length: 5}, () => Buffer.from([65]))], {maxBufferedChunks: 4});
        assert.equal(error?.code, 'WS_ERR_TOO_MANY_BUFFERED_PARTS');
        assert.match(error.message, /chunks/i);
    });
    test('payload size limits remain independent from fragment limits', async () => {
        const error = await receive([Buffer.from([0x82, 17])], {maxPayload: 16, maxFragments: 4});
        assert.equal(error?.code, 'WS_ERR_UNSUPPORTED_MESSAGE_LENGTH');
    });

    async function serverFixture(t, options = {}) {
        const server = new WebSocket.WebSocketServer({host: '127.0.0.1', port: 0, ...options});
        t.after(() => {for (const socket of server.clients) socket.terminate(); server.close();});
        await once(server, 'listening');
        return {server, url: 'ws://127.0.0.1:' + server.address().port};
    }
    test('default client and server fragment limits close small unfinished streams', {timeout: 4000}, async t => {
        for (const targetIsServer of [false, true]) {
            const {server, url} = await serverFixture(t, {perMessageDeflate: false});
            const connected = once(server, 'connection');
            const client = new WebSocket(url, {perMessageDeflate: false});
            t.after(() => client.terminate());
            await once(client, 'open');
            const [peer] = await connected;
            const target = targetIsServer ? peer : client;
            const sender = targetIsServer ? client : peer;
            let failure;
            target.on('error', error => {failure = error;});
            sender.on('error', () => {});
            let timer;
            const closed = new Promise((resolve, reject) => {
                timer = setTimeout(() => reject(new Error('Default fragment limit did not close the unfinished stream')), 1200);
                target.once('close', code => {clearTimeout(timer); resolve(code);});
            });
            const peerClosed = new Promise(resolve => sender.once('close', resolve));
            try {
                // Only 32 KiB unmasked / 96 KiB masked frame headers, no payload.
                for (let i = 0; i <= 16384; i++) sender.send(Buffer.alloc(0), {fin: false, compress: false});
                // The rejecting side may report local abnormal closure (1006)
                // after stopping its parser; the peer sees wire status 1008.
                await closed;
                assert.equal(await peerClosed, 1008);
                assert.equal(failure?.code, 'WS_ERR_TOO_MANY_BUFFERED_PARTS');
            } finally {clearTimeout(timer); client.terminate(); peer.terminate();}
        }
    });
    test('real WebSocket round trips preserve compression, fragments, binary and ping/close', {timeout: 5000}, async t => {
        const {server, url} = await serverFixture(t, {perMessageDeflate: {threshold: 0}});
        server.on('connection', socket => socket.on('message', (data, binary) => socket.send(data, {binary, compress: true})));
        const client = new WebSocket(url, {perMessageDeflate: {threshold: 0}});
        t.after(() => client.terminate());
        await once(client, 'open');
        assert.equal(client.extensions, 'permessage-deflate');
        const text = once(client, 'message');
        client.send('Výpo', {fin: false, compress: true});
        client.send('čet', {fin: true, compress: true});
        const [textData, textBinary] = await text;
        assert.equal(textData.toString(), 'Výpočet'); assert.equal(textBinary, false);
        const binary = once(client, 'message');
        client.send(Buffer.from([0, 1, 255]));
        const [binaryData, isBinary] = await binary;
        assert.deepEqual(binaryData, Buffer.from([0, 1, 255])); assert.equal(isBinary, true);
        const pong = once(client, 'pong'); client.ping('probe');
        assert.equal((await pong)[0].toString(), 'probe');
        const closed = once(client, 'close'); client.close(1000, 'done');
        const [code, reason] = await closed;
        assert.equal(code, 1000); assert.equal(reason.toString(), 'done');
    });

    test('close frames reject non-byte typed arrays and preserve Uint8Array wire bytes', () => {
        const chunks = [];
        const socket = {cork() {}, uncork() {}, write(data, callback) {chunks.push(Buffer.from(data)); if (callback) callback(); return true;}};
        const sender = new WebSocket.Sender(socket);
        assert.throws(() => sender.close(1000, new Uint16Array([65, 66]), false, () => {}), TypeError);
        assert.equal(chunks.length, 0, 'Rejected input must not transmit any bytes');
        sender.close(1000, new Uint8Array([65, 66]), false, () => {});
        assert.equal(Buffer.concat(chunks).toString('hex'), '880403e84142');
    });

    test('compression negotiation rejects offers below the configured client window', () => {
        const PerMessageDeflate = appRequire(path.join(path.dirname(appRequire.resolve('ws')), 'lib/permessage-deflate.js'));
        const extension = new PerMessageDeflate({clientMaxWindowBits: 12, isServer: true, maxPayload: 1024}, true, 1024);
        try {
            assert.throws(() => extension.accept([{client_max_window_bits: ['8']}]), /None of the extension offers can be accepted/);
        } finally { extension.cleanup(); }
    });

    test('real Kuma WebSocket monitor preserves authenticated upgrade and rejects failed auth', {timeout: 5000}, async t => {
        const authorization = 'Basic ' + Buffer.from('fixture:password').toString('base64');
        const {server, url} = await serverFixture(t, {verifyClient: (info, done) => done(info.req.headers.authorization === authorization, 401)});
        const accepted = [];
        server.on('connection', (socket, request) => accepted.push({protocol: socket.protocol, header: request.headers['x-fixture']}));
        const {WebSocketMonitorType} = appRequire('./server/monitor-types/websocket-upgrade');
        const type = new WebSocketMonitorType();
        const monitor = {url, timeout: 1, authMethod: 'basic', basic_auth_user: 'fixture', basic_auth_pass: 'password',
            headers: '{"X-Fixture":"owned"}', wsSubprotocol: 'nf-fixture', accepted_statuscodes_json: '["1000"]'};
        const heartbeat = {};
        await type.check(monitor, heartbeat, {});
        assert.equal(heartbeat.status, 1); assert.equal(heartbeat.msg, '1000 - OK');
        assert.deepEqual(accepted, [{protocol: 'nf-fixture', header: 'owned'}]);
        await assert.rejects(type.check({...monitor, basic_auth_pass: 'wrong'}, {}, {}), /401/);
        assert.equal(accepted.length, 1);
    });
} else {
    console.log('Original WebSocket range: migration regressions deferred to the qualified candidate.');
}
