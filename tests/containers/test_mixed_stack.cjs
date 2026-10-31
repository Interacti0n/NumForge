const assert = require('node:assert/strict');
const path = require('node:path');
const {isolate, decimal, bytes, statsLine, summary} = require('../../benchmarks/application/mixed_stack_benchmark.cjs');
const project = 'numforge-test-mixed-abc123';
const source = {name: 'numforge', volumes: {data: {name: 'numforge_data'}}, networks: {default: {name: 'numforge_default'}},
    secrets: {app: {name: 'numforge_app', file: '/production/app.json'}}, services: {}};
const images = {};
for (const name of ['web', 'accounts', 'gateway', 'uptime', 'setup', 'db']) {
    source.services[name] = {build: {context: 'production'}, image: 'mutable', restart: 'unless-stopped', environment: {},
        ports: [{published: '18876'}], volumes: [{type: 'volume', source: 'data', target: '/data'}]};
    images[name] = 'pinned-fixture-' + name;
}
const original = structuredClone(source);
const profile = isolate(source, project, path.resolve('build/fixture-private'), images);
assert.deepEqual(source, original);
assert.equal(profile.volumes.data.name, project + '_data');
assert.equal(profile.networks.default.name, project + '_default');
assert.notEqual(profile.secrets.app.file, source.secrets.app.file);
assert.equal(profile.services.db.ports.length, 0);
assert.equal(profile.services.gateway.ports[0].host_ip, '127.0.0.1');
for (const service of Object.values(profile.services)) {assert.equal(service.restart, 'no'); assert.ok(!service.build);}
const separate = isolate(source, project, path.resolve('build/fixture-private'), images, 19883);
assert.equal(separate.services.web.ports[0].published, '19883');
assert.equal(separate.services.gateway.ports[0].published, '19884');
assert.equal(separate.services.uptime.ports[0].published, '19886');
for (const name of ['accounts', 'gateway']) {
    assert.equal(separate.services[name].environment.NF_ACCOUNT_PUBLIC_ORIGIN, 'https://localhost:19884');
    assert.equal(separate.services[name].environment.NF_LOCAL_ORIGIN, 'https://localhost:19884');
}
assert.deepEqual(source, original);
for (const port of [0, 1023, 65533, 19883.5, NaN])
    assert.throws(() => isolate(source, project, '/tmp', images, port));
assert.throws(() => isolate(source, 'numforge', '/tmp', images));
source.volumes.data.external = true;
assert.throws(() => isolate(source, project, '/tmp', images)); delete source.volumes.data.external;
source.services.web.volumes = [{type: 'bind', source: '/production/database', target: '/data', read_only: false}];
assert.throws(() => isolate(source, project, '/tmp', images));
assert.equal(decimal('100000'), decimal('1.00000E+5'));
assert.equal(decimal('0.001200'), decimal('12E-4'));
assert.equal(decimal('-0'), '0');
assert.throws(() => decimal('NaN'));
assert.equal(bytes('1.5GiB'), 1.5 * 1024 ** 3);
assert.equal(bytes('10 MB'), 10000000);
assert.equal(statsLine('\x1b[H\x1b[K'), null);
assert.deepEqual(statsLine('\x1b[H'+JSON.stringify({Name:'db',CPUPerc:'12.5%',MemUsage:'1MiB / 4GiB',PIDs:'7'})+'\x1b[K'),
    {name:'db',cpu_percent:12.5,memory_bytes:1048576,pids:7});
const measured = summary([{kind: 'login', ms: 5}, {kind: 'login', ms: 20}, {kind: 'login', ms: 1, error: 'HTTP_429'}]);
assert.equal(measured.login.p95_ms, 20);
assert.equal(measured.login.verified, 2);
assert.deepEqual(measured.login.errors, {HTTP_429: 1});
console.log('Mixed-stack isolation, response normalization and result summaries verified.');
