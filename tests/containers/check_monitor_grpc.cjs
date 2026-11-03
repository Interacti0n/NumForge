// All endpoints and generated certificates are disposable and container-local.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');
const appRequire = createRequire('/app/package.json');
const receipt = '/opt/numforge/monitor-patches.json';
const manifest = fs.existsSync(receipt) ? JSON.parse(fs.readFileSync(receipt, 'utf8')) : {};

if (manifest.directMigrations?.some(p => p.name === '@grpc/grpc-js') || process.env.NUMFORGE_FORCE_GRPC_CHECKS) {
    const grpc = appRequire('@grpc/grpc-js');
    const service = {read: {path: '/nf.Probe/Read', requestStream: false, responseStream: false,
        requestSerialize: x => x, requestDeserialize: x => x, responseSerialize: x => x, responseDeserialize: x => x}};
    const bind = (server, credentials) => new Promise((resolve, reject) => server.bindAsync('127.0.0.1:0', credentials,
        (error, port) => error ? reject(error) : resolve(port)));
    const query = (client, data = Buffer.from('ok'), options = {}) => new Promise((resolve, reject) =>
        client.makeUnaryRequest('/nf.Probe/Read', x => x, x => x, data,
            {deadline: Date.now() + 2000, ...options}, (error, value) => error ? reject(error) : resolve(value)));

    test('gRPC migration is exact and retains the existing proto-loader', () => {
        assert.equal(appRequire('@grpc/grpc-js/package.json').version, '1.13.7');
        const scopedRequire = createRequire(appRequire.resolve('@grpc/grpc-js'));
        assert.equal(scopedRequire.resolve('@grpc/proto-loader'), appRequire.resolve('@grpc/proto-loader'));
        assert.equal(appRequire('@grpc/proto-loader/package.json').version, '0.7.15');
        assert.equal(scopedRequire('@js-sdsl/ordered-map/package.json').version, '4.4.2');
    });

    // Old asynchronous decompressor exceptions must not kill the test runner.
    function compressedCase(side, encoding) {
        const program = `(${async function(side, encoding) {
            const assert = require('node:assert/strict');
            const grpc = require('/app/node_modules/@grpc/grpc-js');
            const http2 = require('node:http2');
            const definition = {read: {path: '/nf.Probe/Read', requestStream: false, responseStream: false,
                requestDeserialize: x => x, responseSerialize: x => x}};
            const frame = Buffer.from([1, 0, 0, 0, 4, 1, 2, 3, 4]);
            let server, client, session;
            const sessions = new Set();
            try {
                if (side === 'client') {
                    server = http2.createServer();
                    server.on('session', s => { sessions.add(s); s.on('error', () => {}); });
                    server.on('stream', stream => {
                        stream.on('error', () => {});
                        stream.respond({':status': 200, 'content-type': 'application/grpc', 'grpc-encoding': encoding}, {waitForTrailers: true});
                        stream.on('wantTrailers', () => stream.sendTrailers({'grpc-status': '0'}));
                        stream.end(frame);
                    });
                    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
                    client = new grpc.Client('127.0.0.1:' + server.address().port, grpc.credentials.createInsecure());
                    await new Promise((resolve, reject) => client.makeUnaryRequest('/nf.Probe/Read', x => x, x => x, Buffer.from('x'),
                        {deadline: Date.now() + 2000}, error => {
                            try { assert.equal(error?.code, grpc.status.INTERNAL); assert.match(error.details, /decompress/i); resolve(); }
                            catch (failure) { reject(failure); }
                        }));
                } else {
                    server = new grpc.Server();
                    let handled = 0;
                    server.addService(definition, {read: (call, done) => {handled++; done(null, call.request);}});
                    const port = await new Promise((resolve, reject) => server.bindAsync('127.0.0.1:0', grpc.ServerCredentials.createInsecure(),
                        (error, port) => error ? reject(error) : resolve(port)));
                    server.start();
                    session = http2.connect('http://127.0.0.1:' + port);
                    const headers = await new Promise((resolve, reject) => {
                        const stream = session.request({':method': 'POST', ':path': '/nf.Probe/Read', 'content-type': 'application/grpc', 'te': 'trailers', 'grpc-encoding': encoding});
                        const headers = {};
                        stream.on('response', h => Object.assign(headers, h));
                        stream.on('trailers', h => Object.assign(headers, h));
                        stream.on('error', reject); stream.resume();
                        stream.on('end', () => resolve(headers)); stream.end(frame);
                    });
                    assert.equal(Number(headers['grpc-status']), grpc.status.INTERNAL);
                    assert.equal(handled, 0);
                    client = new grpc.Client('127.0.0.1:' + port, grpc.credentials.createInsecure());
                    const result = await new Promise((resolve, reject) => client.makeUnaryRequest('/nf.Probe/Read', x => x, x => x, Buffer.from('ok'),
                        {deadline: Date.now() + 2000}, (error, value) => error ? reject(error) : resolve(value)));
                    assert.equal(result.toString(), 'ok'); assert.equal(handled, 1);
                }
            } finally {
                client?.close(); session?.destroy();
                for (const s of sessions) s.destroy();
                if (side === 'client') server?.close(); else server?.forceShutdown();
            }
        }.toString()})(${JSON.stringify(side)},${JSON.stringify(encoding)}).catch(e=>{console.error(e);process.exitCode=1;});`;
        const result = spawnSync(process.execPath, ['--max-old-space-size=64', '-e', program], {timeout: 5000, encoding: 'utf8', maxBuffer: 65536});
        assert.equal(result.error, undefined, 'Compressed-message check must terminate');
        assert.equal(result.status, 0, result.stderr.slice(-2500));
    }
    for (const side of ['client', 'server']) for (const encoding of ['gzip', 'deflate']) {
        test(`malformed ${encoding} ${side} messages report an RPC error without process failure`, () => compressedCase(side, encoding));
    }

    test('receive limits and deadlines reject calls while the server remains usable', async t => {
        const server = new grpc.Server({'grpc.max_receive_message_length': 32});
        server.addService(service, {read: (call, done) => {
            if (call.request.toString() !== 'wait') done(null, call.request);
        }});
        const port = await bind(server, grpc.ServerCredentials.createInsecure());
        server.start();
        const client = new grpc.Client('127.0.0.1:' + port, grpc.credentials.createInsecure());
        t.after(() => {client.close(); server.forceShutdown();});
        await assert.rejects(query(client, Buffer.alloc(64)), e => e.code === grpc.status.RESOURCE_EXHAUSTED);
        await assert.rejects(query(client, Buffer.from('wait'), {deadline: Date.now() + 100}), e => e.code === grpc.status.DEADLINE_EXCEEDED);
        assert.equal((await query(client)).toString(), 'ok');
    });

    test('handler exceptions return an error without leaking internal details', async t => {
        const server = new grpc.Server();
        server.addService(service, {read: (call, done) => {
            if (call.request.toString() === 'throw') throw new Error('PRIVATE_HANDLER_DETAIL');
            done(null, call.request);
        }});
        const port = await bind(server, grpc.ServerCredentials.createInsecure());
        server.start();
        const client = new grpc.Client('127.0.0.1:' + port, grpc.credentials.createInsecure());
        t.after(() => {client.close(); server.forceShutdown();});
        await assert.rejects(query(client, Buffer.from('throw')), e => e.code === grpc.status.UNKNOWN && !e.details.includes('PRIVATE_HANDLER_DETAIL'));
        assert.equal((await query(client)).toString(), 'ok');
    });

    test('TLS enforces trusted server and required client certificates', async t => {
        const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'nf-grpc-tls-'));
        t.after(() => fs.rmSync(temporary, {recursive: true}));
        for (const name of ['trusted', 'untrusted']) {
            const result = spawnSync('openssl', ['req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
                '-subj', '/CN=localhost', '-addext', 'subjectAltName=IP:127.0.0.1,DNS:localhost',
                '-keyout', path.join(temporary, name + '.key'), '-out', path.join(temporary, name + '.crt')], {encoding: 'utf8', timeout: 5000});
            assert.equal(result.status, 0, result.stderr);
        }
        const cert = name => fs.readFileSync(path.join(temporary, name + '.crt'));
        const key = name => fs.readFileSync(path.join(temporary, name + '.key'));
        const server = new grpc.Server();
        server.addService(service, {read: (call, done) => done(null, call.request)});
        const port = await bind(server, grpc.ServerCredentials.createSsl(cert('trusted'), [{private_key: key('trusted'), cert_chain: cert('trusted')}], true));
        server.start();
        const clients = [];
        t.after(() => {for (const client of clients) client.close(); server.forceShutdown();});
        for (const name of ['trusted', 'untrusted', null]) {
            const credentials = grpc.credentials.createSsl(cert('trusted'), name ? key(name) : undefined, name ? cert(name) : undefined);
            const client = new grpc.Client('127.0.0.1:' + port, credentials, {'grpc.ssl_target_name_override': 'localhost'});
            clients.push(client);
            if (name === 'trusted') assert.equal((await query(client)).toString(), 'ok');
            else await assert.rejects(query(client), e => e.code === grpc.status.UNAVAILABLE, name || 'no certificate');
        }
        const badClient = new grpc.Client('127.0.0.1:' + port, grpc.credentials.createSsl(cert('untrusted')), {'grpc.ssl_target_name_override': 'localhost'});
        clients.push(badClient);
        await assert.rejects(query(badClient), e => e.code === grpc.status.UNAVAILABLE);
    });
} else {
    console.log('Original gRPC range: migration regressions deferred to the qualified candidate.');
}
