'use strict';
// Observe head-of-line blocking on a dedicated loopback server. Never touches
// the user's running server/tunnel. Timing differences are not CI assertions.
const assert = require('node:assert/strict');
const http = require('node:http');
const net = require('node:net');
const path = require('node:path');
const {spawn} = require('node:child_process');
const executable = process.argv[2];
if (!executable || process.argv.length !== 3) throw Error('Usage: node benchmarks/http_queue_benchmark.js server-executable');
const requestedPort = Number(process.env.NUMFORGE_QUEUE_BENCH_PORT || 0);
assert.ok(Number.isInteger(requestedPort) && (requestedPort === 0 || requestedPort >= 1024 && requestedPort <= 65535));
let port;
function availablePort() {
    return new Promise((resolve, reject) => {
        const reservation = net.createServer();
        reservation.once('error', reject);
        reservation.listen(requestedPort, '127.0.0.1', () => {
            const selected = reservation.address().port;
            reservation.close(error => error ? reject(error) : resolve(selected));
        });
    });
}
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
function request(expression, client) {
    return new Promise((resolve, reject) => {
        const begin = process.hrtime.bigint();
        const req = http.request({hostname: '127.0.0.1', port, method: 'POST', agent: false,
            path: '/api/evaluate?' + new URLSearchParams({precision: '10', angle: 'rad', notation: 'auto', client, revision: '1'}),
            headers: {'Content-Type': 'text/plain', 'Content-Length': Buffer.byteLength(expression)}}, res => {
            let text = '';
            res.setEncoding('utf8');
            res.on('data', chunk => {text += chunk;});
            res.on('error', reject);
            res.on('end', () => {
                try {resolve({data: JSON.parse(text), ms: Number(process.hrtime.bigint() - begin) / 1e6});}
                catch (error) {reject(error);}
            });
        });
        req.setTimeout(15000, () => req.destroy(Error('Queue diagnostic request timed out')));
        req.on('error', reject);
        req.end(expression);
    });
}
async function sample(index) {
    port = await availablePort();
    const server = spawn(path.resolve(executable), ['--no-browser', '--port', String(port)],
        {windowsHide: true, stdio: ['ignore', 'ignore', 'pipe']});
    let spawnError, diagnostics = '';
    server.on('error', error => {spawnError = error;});
    server.stderr.on('data', data => {diagnostics += data;});
    try {
        let ready = false;
        for (let i = 0; i < 100; i++) {
            if (spawnError) throw spawnError;
            if (server.exitCode !== null) throw Error('Server exited: ' + diagnostics);
            try {const r = await request('1+2', 'a'.repeat(32)); assert.equal(r.data.result, '3'); ready = true; break;}
            catch {await delay(20);}
        }
        assert.ok(ready, 'Dedicated server did not start');
        const baseline = await request('1+2', 'b'.repeat(32));
        assert.equal(baseline.data.result, '3');
        const slowPending = request('100000!', 'c'.repeat(32));
        // Attach rejection handling immediately while scheduling the fast call.
        const slowSettled = slowPending.then(value => ({value}), error => ({error}));
        await delay(100);
        const fast = await request('1+2', 'd'.repeat(32));
        const slow = await slowSettled;
        if (slow.error) throw slow.error;
        assert.equal(fast.data.result, '3');
        if (slow.value.data.ok) assert.equal(slow.value.data.result, '2.824229408E+456573');
        console.log([index, baseline.ms.toFixed(3), slow.value.ms.toFixed(3), fast.ms.toFixed(3),
            slow.value.data.ok ? 'OK' : slow.value.data.error?.code || slow.value.data.error || 'error'].join(','));
    } finally {
        if (server.exitCode === null && !spawnError) {
            const exited = new Promise(resolve => server.once('exit', resolve));
            server.kill(); await exited;
        }
    }
}
(async () => {
    console.log('sample,fast_idle_ms,slow_ms,fast_queued_ms,slow_status');
    for (let sampleIndex = 1; sampleIndex <= 3; sampleIndex++) await sample(sampleIndex);
})().catch(error => {console.error(error); process.exitCode = 1;});
