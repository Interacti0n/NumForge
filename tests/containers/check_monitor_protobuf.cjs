// Run in the disposable monitor; all gRPC/HTTP traffic stays on its loopback.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const {createRequire} = require('node:module');
const {spawnSync} = require('node:child_process');
const appRequire = createRequire('/app/package.json');
const receiptFile = '/opt/numforge/monitor-patches.json';
const receipt = fs.existsSync(receiptFile) ? JSON.parse(fs.readFileSync(receiptFile)) : {};

if (receipt.directMigrations?.some(p => p.name === 'protobufjs') || process.env.NUMFORGE_FORCE_PROTOBUF_CHECKS) {
    const pb = appRequire('protobufjs');

    test('protobuf exact version and proto-loader share the qualified runtime', () => {
        assert.equal(appRequire('protobufjs/package.json').version, '7.6.6');
        const loaderRequire = createRequire(appRequire.resolve('@grpc/proto-loader'));
        assert.equal(loaderRequire.resolve('protobufjs'), appRequire.resolve('protobufjs'));
    });

    test('protobuf wire bytes preserve int64, packed signed values and oneof', () => {
        const schema = 'syntax="proto3"; message Value { string label=1; int64 count=2; repeated sint32 deltas=3 [packed=true]; oneof choice {string note=4;bool flag=5;} }';
        const type = pb.parse(schema).root.resolveAll().lookupType('Value');
        const value = type.fromObject({label: 'ok', count: '9223372036854775807', deltas: [-1, 0, 2], note: 'x'});
        const bytes = type.encode(value).finish();
        // Independent fixed wire reference, rather than using the decoder as oracle.
        assert.equal(Buffer.from(bytes).toString('hex'), '0a026f6b10ffffffffffffffff7f1a03010004220178');
        const decoded = type.toObject(type.decode(bytes), {longs: String, oneofs: true});
        assert.deepEqual(decoded, {label: 'ok', count: '9223372036854775807', deltas: [-1, 0, 2], note: 'x', choice: 'note'});
    });

    test('binary boundary cases retain Buffer and Uint8Array semantics', () => {
        const input = Buffer.from('0a054142', 'hex');
        const bytesType = pb.parse('syntax="proto3"; message Value {bytes text=1;}').root.lookupType('Value');
        assert.throws(() => bytesType.decode(input), RangeError);
        assert.throws(() => bytesType.decode(new Uint8Array(input)), RangeError);
        const stringType = pb.parse('syntax="proto3"; message Value {string text=1;}').root.lookupType('Value');
        assert.throws(() => stringType.decode(new Uint8Array(input)), RangeError);
        // Existing native Buffer.string behavior clamps to the available bytes.
        // This library is not a general strict-frame validator.
        assert.equal(stringType.decode(input).text, 'AB');
    });

    test('schema bytes defaults cannot execute generated JavaScript', () => {
        delete globalThis.numforgeProtoMarker;
        try {
            const root = pb.Root.fromJSON({nested: {Value: {fields: {data: {id: 1, type: 'bytes', options: {default: [0, '(globalThis.numforgeProtoMarker=23,0)']}}}}}});
            const type = root.lookupType('Value');
            try { type.toObject(type.create(), {defaults: true, bytes: Array}); } catch (error) {
                assert(error instanceof Error);
            }
            assert.equal(globalThis.numforgeProtoMarker, undefined);
        } finally {
            delete globalThis.numforgeProtoMarker;
        }
    });

    test('unfinished proto options terminate with a parse error', () => {
        const program = `const p=require('/app/node_modules/protobufjs');try{p.parse('syntax="proto3";option broken');process.exitCode=2;}catch(e){process.exitCode=0;}`;
        const result = spawnSync(process.execPath, ['-e', program], {timeout: 2000, encoding: 'utf8'});
        assert.equal(result.error, undefined);
        assert.equal(result.status, 0, result.stderr);
    });

    test('real Kuma gRPC monitor handles wire response, keyword failure and RPC error', {timeout: 10000}, async t => {
        const grpc = appRequire('@grpc/grpc-js');
        const {GrpcKeywordMonitorType} = appRequire('./server/monitor-types/grpc');
        const clients = new Set();
        const original = grpc.Client.prototype.makeUnaryRequest;
        t.mock.method(grpc.Client.prototype, 'makeUnaryRequest', function(...args) {
            clients.add(this);
            return original.apply(this, args);
        });
        const server = new grpc.Server();
        t.after(() => { for (const client of clients) client.close(); server.forceShutdown(); });
        let requests = 0;
        let unavailable = false;
        server.addService({read: {
            path: '/nf.health.Probe/Read', requestStream: false, responseStream: false,
            requestDeserialize: bytes => {
                assert.equal(bytes.toString('hex'), '0a0568656c6c6f1203010004');
                return {};
            },
            responseSerialize: () => Buffer.from('0a076865616c746879', 'hex'),
        }}, {read: (call, callback) => {
            requests++;
            if (unavailable) callback({code: grpc.status.UNAVAILABLE, details: 'fixture unavailable'});
            else callback(null, {});
        }});
        const port = await new Promise((resolve, reject) => server.bindAsync('127.0.0.1:0', grpc.ServerCredentials.createInsecure(), (error, selected) => error ? reject(error) : resolve(selected)));
        server.start();
        const schema = 'syntax="proto3";package nf.health;service Probe{rpc Read(Query)returns(Reply);}message Query{string message=1;repeated sint32 deltas=2 [packed=true];}message Reply{string value=1;}';
        const monitor = {
            grpcUrl: '127.0.0.1:' + port, grpcProtobuf: schema, grpcServiceName: 'nf.health.Probe',
            grpcEnableTls: false, grpcMethod: 'read', grpcBody: '{"message":"hello","deltas":[-1,0,2]}', keyword: 'healthy',
            isInvertKeyword: () => false,
        };
        const type = new GrpcKeywordMonitorType();
        const heartbeat = {};
        await type.check(monitor, heartbeat, {});
        assert.equal(heartbeat.status, 1);
        assert.match(heartbeat.msg, /healthy/);
        await assert.rejects(type.check({...monitor, keyword: 'missing-keyword'}, {}, {}), /keyword/);
        const service = type.constructGrpcService(monitor.grpcUrl, schema, monitor.grpcServiceName, false);
        await assert.rejects(type.grpcQuery(service, 'read', '{broken'), SyntaxError);
        assert.equal(requests, 2);
        unavailable = true;
        await assert.rejects(type.grpcQuery(service, 'read', monitor.grpcBody), error => error.code === grpc.status.UNAVAILABLE);
    });
} else {
    console.log('Original protobuf range: migration regressions deferred to the qualified candidate.');
}
