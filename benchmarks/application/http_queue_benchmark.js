'use strict';
// Observe head-of-line blocking on a dedicated loopback server. Never touches
// the user's running server/tunnel. Timing differences are not CI assertions.
const assert = require('node:assert/strict');
const http = require('node:http');
const net = require('node:net');
const path = require('node:path');
const {spawn, execFile, execFileSync} = require('node:child_process');
const {promisify} = require('node:util');
const fs = require('node:fs');
const os = require('node:os');
const {createHash} = require('node:crypto');
const executable = process.argv[2];
const args = process.argv.slice(3), load = args.includes('--load'), quick = args.includes('--quick');
if (!executable || args.some(arg => !['--load', '--quick'].includes(arg)) || new Set(args).size !== args.length || quick && !load)
    throw Error('Usage: node benchmarks/application/http_queue_benchmark.js server-executable [--load [--quick]]');
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
function exchange(target, expression = null) {
    return new Promise((resolve, reject) => {
        const begin = process.hrtime.bigint();
        const req = http.request({hostname: '127.0.0.1', port, method: expression === null ? 'GET' : 'POST', agent: false,
            path: target, headers: expression === null ? {} :
                {'Content-Type': 'text/plain', 'Content-Length': Buffer.byteLength(expression)}}, res => {
            let text = '', bytes = 0;
            res.setEncoding('utf8');
            res.on('data', chunk => {
                bytes += Buffer.byteLength(chunk);
                if (bytes > 256 * 1024) {
                    const error = Error('Response exceeds diagnostic limit'); error.code = 'RESPONSE_LIMIT'; req.destroy(error);
                }
                else text += chunk;
            });
            res.on('error', error => {clearTimeout(deadline); reject(error);});
            res.on('end', () => {
                clearTimeout(deadline);
                try {resolve({status: res.statusCode, data: JSON.parse(text), ms: Number(process.hrtime.bigint() - begin) / 1e6});}
                catch (error) {error.code = 'INVALID_JSON'; reject(error);}
            });
        });
        const deadline = setTimeout(() => {
            const error = Error('Queue diagnostic request timed out'); error.code = 'DEADLINE'; req.destroy(error);
        }, 15000);
        req.on('error', error => {clearTimeout(deadline); reject(error);});
        req.end(expression === null ? undefined : expression);
    });
}
function request(expression, client, options = {}) {
    return exchange('/api/evaluate?' + new URLSearchParams({precision: '10', angle: 'rad', notation: 'auto',
        client, revision: '1', ...options}), expression);
}
async function withServer(work) {
    port = await availablePort();
    const server = spawn(path.resolve(executable), ['--no-browser', '--port', String(port)],
        {windowsHide: true, stdio: ['ignore', 'ignore', 'pipe']});
    let spawnError, diagnostics = '';
    server.on('error', error => {spawnError = error;});
    server.stderr.on('data', data => {diagnostics = (diagnostics + data).slice(-8192);});
    try {
        let ready = false;
        for (let i = 0; i < 100; i++) {
            if (spawnError) throw spawnError;
            if (server.exitCode !== null) throw Error('Server exited: ' + diagnostics);
            try {const r = await request('1+2', 'a'.repeat(32)); assert.equal(r.data.result, '3'); ready = true; break;}
            catch {await delay(20);}
        }
        assert.ok(ready, 'Dedicated server did not start');
        return await work(server.pid);
    } finally {
        if (server.exitCode === null && !spawnError) {
            const exited = new Promise(resolve => server.once('exit', resolve));
            server.kill();
            const force = setTimeout(() => server.kill('SIGKILL'), 2000);
            try { await exited; } finally { clearTimeout(force); }
        }
    }
}
async function sample(index) {
    return withServer(async () => {
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
    });
}

function summary(records, elapsed) {
    const durations = records.map(record => record.ms).sort((a, b) => a - b);
    const percentile = p => durations.length ? durations[Math.ceil(p * durations.length) - 1] : null;
    const errors = {};
    for (const record of records) if (record.error) errors[record.error] = (errors[record.error] || 0) + 1;
    return {requests: records.length, ok: records.filter(record => !record.error).length,
        elapsed_ms: elapsed, completed_per_second: records.length * 1000 / elapsed,
        verified_per_second: records.filter(record => !record.error).length * 1000 / elapsed,
        latency_ms: {p50: percentile(.5), p95: percentile(.95), p99: percentile(.99), max: durations.at(-1) ?? null}, errors};
}
async function measured(records, operation, call, validate) {
    const begin = performance.now();
    let response;
    try { response = await call(); }
    catch (error) {
        const invalid = ['INVALID_JSON', 'RESPONSE_LIMIT'].includes(error.code);
        if (invalid) process.exitCode = 1;
        records.push({operation, ms: performance.now() - begin, error: (invalid ? 'verification:' : 'transport:') + (error.code || error.message)});
        return;
    }
    let error;
    if (response.status !== 200 || response.data.ok !== true)
        error = `http:${response.status}:` + (response.data.status || 'unspecified');
    else {
        try { validate(response.data); }
        catch (failure) { error = 'verification:' + failure.message; process.exitCode = 1; }
    }
    records.push({operation, ms: response.ms, error});
}
const clientId = index => (index + 1).toString(16).padStart(32, '0');
async function peakMemory(pid) {
    try {
        if (process.platform === 'win32') {
            const {stdout} = await promisify(execFile)('powershell.exe', ['-NoProfile', '-NonInteractive', '-Command',
                `(Get-Process -Id ${pid} -ErrorAction Stop).PeakWorkingSet64`], {windowsHide: true, timeout: 5000});
            const bytes = Number(stdout.trim());
            assert.ok(Number.isSafeInteger(bytes) && bytes > 0);
            return {server_peak_resident_bytes: bytes, memory_source: 'windows_peak_working_set'};
        }
        if (process.platform === 'linux') {
            const status = await fs.promises.readFile(`/proc/${pid}/status`, 'utf8');
            const bytes = Number(status.match(/^VmHWM:\s+(\d+) kB$/m)?.[1]) * 1024;
            assert.ok(Number.isSafeInteger(bytes) && bytes > 0);
            return {server_peak_resident_bytes: bytes, memory_source: 'linux_vm_hwm'};
        }
    } catch { /* Unavailable process metrics must not be reported as zero. */ }
    return {server_peak_resident_bytes: null, memory_source: 'unavailable'};
}
async function loadSample(clients, repetition) {
    return withServer(async pid => {
        const records = [], begin = performance.now(), turns = quick ? 2 : 6;
        // Closed-loop load: each virtual user waits for their own response. No retries.
        await Promise.all(Array.from({length: clients}, async (_, index) => {
            for (let turn = 0; turn < turns; ++turn) {
                const client = clientId(index), options = {revision: String(turn + 1)};
                switch ((index + turn) % 5) {
                    case 0:
                        await measured(records, 'arithmetic', () => request('2+2', client, options), data => assert.equal(data.result, '4')); break;
                    case 1:
                        await measured(records, 'fraction', () => request('1/3+1/6', client, {...options, notation: 'fraction'}),
                            data => assert.equal(data.result, '1/2')); break;
                    case 2:
                        await measured(records, 'equation', () => exchange('/api/equations/solve?mode=expression&precision=16', '(x-2)*(x+3)=0'),
                            data => assert.deepEqual(data.roots.map(root => root.exact), ['-3', '2'])); break;
                    case 3:
                        await measured(records, 'graph', () => exchange('/api/graph/sample?min=0&max=1&points=17', 'x*x'), data => {
                            assert.equal(data.count, 17); assert.equal(data.points.length, 17);
                            data.points.forEach((point, i) => {
                                assert.equal(point.kind, 'real'); assert.equal(Number(point.x), i / 16);
                                assert.equal(Number(point.y), (i / 16) ** 2);
                            });
                        }); break;
                    case 4:
                        // Independent integer midpoint test for the transmitted sqrt(2).
                        await measured(records, 'sqrt1000', () => request('sqrt(2)', client, {...options, precision: '1000', notation: 'plain'}), data => {
                            assert.match(data.result, /^1\.\d{999,1000}$/);
                            const scale = data.result.length - 2, n = BigInt(data.result.replace('.', ''));
                            const d = 10n ** BigInt(scale), square = 8n * d * d;
                            assert.ok((2n * n - 1n) ** 2n < square && square < (2n * n + 1n) ** 2n);
                        }); break;
                }
            }
        }));
        const elapsed = performance.now() - begin;
        const byOperation = {};
        for (const operation of new Set(records.map(record => record.operation)))
            byOperation[operation] = summary(records.filter(record => record.operation === operation), elapsed);
        // Recovery is outside the measured interval and must return a correct result.
        const recovery = await request('2+2', 'f'.repeat(32));
        assert.equal(recovery.status, 200); assert.equal(recovery.data.ok, true); assert.equal(recovery.data.result, '4');
        console.log(JSON.stringify({scenario: 'mixed', clients, repetition, turns, ...summary(records, elapsed),
            by_operation: byOperation, ...await peakMemory(pid)}));
    });
}
async function sessionSample(clients, repetition) {
    return withServer(async pid => {
        // Serial setup intentionally separates capacity eviction from transport load.
        for (let i = 0; i < clients; ++i) {
            const started = await request('', clientId(i), {precision: 'full', action: 'start'});
            assert.equal(started.data.ok, true);
            const assigned = await request('x=' + (1000 + i), clientId(i), {precision: 'full', action: 'commit'});
            assert.equal(assigned.data.ok, true); assert.equal(assigned.data.result, String(1000 + i));
        }
        let retained = 0, expired = 0;
        for (let i = 0; i < clients; ++i) {
            const response = await exchange('/api/session/value?' + new URLSearchParams({client: clientId(i), name: 'x'}));
            if (response.data.ok) { assert.equal(response.data.value.text, String(1000 + i)); ++retained; }
            else { assert.equal(response.status, 400); assert.match(response.data.status, /^session expired/); ++expired; }
        }
        const records = [], begin = performance.now();
        await Promise.all(Array.from({length: clients}, (_, i) => measured(records, 'session_read',
            () => exchange('/api/session/value?' + new URLSearchParams({client: clientId(i), name: 'x'})),
            data => assert.equal(data.value.text, String(1000 + i)))));
        const elapsed = performance.now() - begin;
        const recovery = await request('2+2', 'f'.repeat(32));
        assert.equal(recovery.data.ok, true); assert.equal(recovery.data.result, '4');
        console.log(JSON.stringify({scenario: 'sessions', clients, repetition, retained_before_load: retained,
            expired_before_load: expired, ...summary(records, elapsed), ...await peakMemory(pid)}));
    });
}
(async () => {
    if (load) {
        let revision = 'unavailable', dirty = [];
        try {
            const root = path.resolve(__dirname, '../..');
            revision = execFileSync('git', ['-C', root, 'rev-parse', 'HEAD'], {encoding: 'utf8', windowsHide: true}).trim();
            dirty = execFileSync('git', ['-C', root, 'diff', '--name-only', 'HEAD'], {encoding: 'utf8', windowsHide: true}).trim().split('\n').filter(Boolean);
        } catch { /* Standalone diagnostics can run without a Git checkout. */ }
        console.log(JSON.stringify({schema_version: 1, scenario: 'environment', revision, dirty,
            executable: path.resolve(executable), executable_sha256: createHash('sha256').update(fs.readFileSync(executable)).digest('hex'),
            platform: process.platform, architecture: process.arch, node: process.version,
            cpu: os.cpus()[0]?.model, logical_cpus: os.cpus().length, host_memory_bytes: os.totalmem(),
            request_deadline_ms: 15000, connection_reuse: false, memory_scope: 'process_peak_through_recovery_when_available'}));
        for (let repetition = 1; repetition <= (quick ? 1 : 3); ++repetition)
            for (const clients of quick ? [2, 10] : [10, 30, 60]) {
                await loadSample(clients, repetition); await sessionSample(clients, repetition);
            }
        return;
    }
    console.log('sample,fast_idle_ms,slow_ms,fast_queued_ms,slow_status');
    for (let sampleIndex = 1; sampleIndex <= 3; sampleIndex++) await sample(sampleIndex);
})().catch(error => {console.error(error); process.exitCode = 1;});
