'use strict';
// Isolated loopback diagnostics by default. Explicit --remote-state opts into
// sustained HTTPS checks against an existing Windows preview, without restarting it.
const assert = require('node:assert/strict');
const http = require('node:http');
const https = require('node:https');
const net = require('node:net');
const path = require('node:path');
const {spawn, execFile, execFileSync} = require('node:child_process');
const {promisify} = require('node:util');
const fs = require('node:fs');
const os = require('node:os');
const {createHash, randomBytes} = require('node:crypto');
const executable = process.argv[2];
const args = process.argv.slice(3), remoteIndex = args.indexOf('--remote-state');
const remotePath = remoteIndex < 0 ? null : args.splice(remoteIndex, 2)[1];
const load = args.includes('--load'), quick = args.includes('--quick'), sustained = args.includes('--sustained');
const heavy = args.includes('--mixed-heavy');
if (!executable || args.some(arg => !['--load', '--sustained', '--quick', '--mixed-heavy'].includes(arg)) ||
    new Set(args).size !== args.length || Number(load)+Number(sustained)+Number(heavy)>1 || quick && !load && !sustained || remoteIndex >= 0 && (!remotePath || !sustained && !heavy))
    throw Error('Usage: node benchmarks/application/http_queue_benchmark.js server-executable [--load|--sustained|--mixed-heavy] [--quick] [--remote-state state.json]');
const remoteState = remotePath ? JSON.parse(fs.readFileSync(remotePath, 'utf8').replace(/^\uFEFF/, '')) : null;
if (remoteState) {
    assert.equal(process.platform, 'win32', 'Remote-state metrics require the Windows preview helper');
    assert.equal(remoteState.SchemaVersion, 2);
    assert.match(remoteState.Url, /^https:\/\/[a-z0-9-]+\.trycloudflare\.com$/);
    assert.equal(path.resolve(executable).toLowerCase(), remoteState.Server.Path.toLowerCase());
    for (const entry of [remoteState.Server, remoteState.Tunnel]) assert.ok(Number.isSafeInteger(entry.Id) && entry.Id > 0);
}
const requestedPort = Number(process.env.NUMFORGE_QUEUE_BENCH_PORT || 0);
assert.ok(Number.isInteger(requestedPort) && (requestedPort === 0 || requestedPort >= 1024 && requestedPort <= 65535));
const queueCapacity = Number(process.env.NUMFORGE_QUEUE_BENCH_CAPACITY || 32);
assert.ok(Number.isInteger(queueCapacity) && queueCapacity >= 1 && queueCapacity <= 64);
const workers = Number(process.env.NUMFORGE_QUEUE_BENCH_WORKERS || 4);
assert.ok(Number.isInteger(workers) && workers >= 1 && workers <= 16);
let port;
let clientNamespace;
const remoteSessions = new Map();
function assertRemoteState() {
    const current = JSON.parse(fs.readFileSync(remotePath, 'utf8').replace(/^\uFEFF/, ''));
    for (const key of ['Url', 'Server', 'Tunnel', 'Workers', 'QueueCapacity']) assert.deepEqual(current[key], remoteState[key], 'Preview identity/settings changed during measurement');
}
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
        const transport = remoteState ? https : http;
        const req = transport.request({hostname: remoteState ? new URL(remoteState.Url).hostname : '127.0.0.1',
            port: remoteState ? 443 : port, method: expression === null ? 'GET' : 'POST', agent: false,
            path: target, headers: {...(remoteState ? {Origin: remoteState.Url} : {}), ...(expression === null ? {} :
                {'Content-Type': 'text/plain', 'Content-Length': Buffer.byteLength(expression)})}}, res => {
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
    if (remoteState) {
        await processMemory(remoteState.Server.Id); // Check paths/start times before sending load.
        clientNamespace = randomBytes(16).toString('hex');
        remoteSessions.clear();
        try { return await work(remoteState.Server.Id); }
        finally {
            assertRemoteState();
            const releases = await Promise.allSettled([...remoteSessions].map(async ([client, revision]) => {
                const reply = await exchange('/api/session?' + new URLSearchParams({client, revision: String(revision + 1), action: 'release'}), '');
                assert.equal(reply.status, 200); assert.equal(reply.data.ok, true);
            }));
            const errors = releases.filter(value => value.status === 'rejected').map(value => String(value.reason));
            console.log(JSON.stringify({scenario: 'remote_cleanup', sessions: releases.length, errors, ...await processMemory(remoteState.Server.Id)}));
            assert.equal(errors.length, 0, 'Test session release failed');
        }
    }
    port = await availablePort();
    const server = spawn(path.resolve(executable), ['--no-browser', '--port', String(port),
        '--workers', String(workers), '--queue-capacity', String(queueCapacity)],
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
        if (process.platform === 'win32') await promisify(execFile)('powershell.exe', ['-NoProfile', '-NonInteractive', '-Command',
            `(Get-Process -Id ${server.pid} -ErrorAction Stop).PriorityClass='BelowNormal'`], {windowsHide:true, timeout:5000});
        return await work(server.pid);
    } finally {
        if (server.exitCode === null && !spawnError) {
            const exited = new Promise(resolve => server.once('exit', resolve));
            server.kill();
            const force = setTimeout(() => server.kill('SIGKILL'), 8000);
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
        error = `http:${response.status}:` + (response.data.code || response.data.status || 'unspecified');
    else {
        try { validate(response.data); }
        catch (failure) { error = 'verification:' + failure.message; process.exitCode = 1; }
    }
    records.push({operation, ms: response.ms, error});
}
const clientId = index => remoteState ? createHash('sha256').update(`${clientNamespace}:${index}`).digest('hex').slice(0, 32) :
    (index + 1).toString(16).padStart(32, '0');
async function processMemory(pid) {
    if (remoteState) {
        assertRemoteState();
        const {stdout} = await promisify(execFile)('powershell.exe', ['-NoProfile', '-NonInteractive', '-Command',
            `Get-Process -Id ${pid},${remoteState.Tunnel.Id} -ErrorAction Stop | ForEach-Object { @{id=$_.Id; path=$_.Path; start=$_.StartTime.ToUniversalTime().ToString('o'); resident=$_.WorkingSet64; peak=$_.PeakWorkingSet64; private=$_.PrivateMemorySize64; cpu_seconds=$_.TotalProcessorTime.TotalSeconds} } | ConvertTo-Json -Compress`],
            {windowsHide: true, timeout: 5000});
        const processes = JSON.parse(stdout.replace(/^\uFEFF/, ''));
        const canonicalTime = value => value.replace(/(\.\d*?)0+Z$/, '$1Z').replace('.Z', 'Z');
        for (const entry of [remoteState.Server, remoteState.Tunnel]) {
            const actual = processes.find(value => value.id === entry.Id);
            assert.ok(actual, 'Recorded process disappeared');
            assert.equal(actual.path.toLowerCase(), entry.Path.toLowerCase());
            assert.equal(canonicalTime(actual.start), canonicalTime(entry.StartTime));
            assert.ok([actual.resident, actual.peak, actual.private].every(value => Number.isSafeInteger(value) && value > 0));
        }
        const server = processes.find(value => value.id === pid), tunnel = processes.find(value => value.id === remoteState.Tunnel.Id);
        return {server_resident_bytes: server.resident, server_peak_resident_bytes: server.peak,
            server_private_bytes: server.private, server_cpu_seconds: server.cpu_seconds,
            tunnel_resident_bytes: tunnel.resident, tunnel_peak_resident_bytes: tunnel.peak,
            tunnel_private_bytes: tunnel.private, tunnel_cpu_seconds: tunnel.cpu_seconds,
            memory_source: 'windows_working_set_and_private_bytes'};
    }
    try {
        if (process.platform === 'win32') {
            const {stdout} = await promisify(execFile)('powershell.exe', ['-NoProfile', '-NonInteractive', '-Command',
                `$p = Get-Process -Id ${pid} -ErrorAction Stop; "$($p.WorkingSet64),$($p.PeakWorkingSet64),$($p.PrivateMemorySize64),$($p.TotalProcessorTime.TotalSeconds.ToString([Globalization.CultureInfo]::InvariantCulture))"`],
                {windowsHide: true, timeout: 5000});
            const [current, bytes, privateBytes, cpuSeconds] = stdout.trim().split(',').map(Number);
            assert.ok([current, bytes].every(value => Number.isSafeInteger(value) && value > 0));
            return {server_resident_bytes: current, server_peak_resident_bytes: bytes,
                server_private_bytes: privateBytes, server_cpu_seconds: cpuSeconds, memory_source: 'windows_working_set_and_private_bytes'};
        }
        if (process.platform === 'linux') {
            const status = await fs.promises.readFile(`/proc/${pid}/status`, 'utf8');
            const bytes = Number(status.match(/^VmHWM:\s+(\d+) kB$/m)?.[1]) * 1024;
            assert.ok(Number.isSafeInteger(bytes) && bytes > 0);
            const current = Number(status.match(/^VmRSS:\s+(\d+) kB$/m)?.[1]) * 1024;
            assert.ok(Number.isSafeInteger(current) && current > 0);
            return {server_resident_bytes: current, server_peak_resident_bytes: bytes, memory_source: 'linux_proc_status'};
        }
    } catch { /* Unavailable process metrics must not be reported as zero. */ }
    return {server_resident_bytes: null, server_peak_resident_bytes: null, memory_source: 'unavailable'};
}
async function mixedOperation(records, selector, client, options) {
    switch (selector % 5) {
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
async function loadSample(clients, repetition) {
    return withServer(async pid => {
        const records = [], begin = performance.now(), turns = quick ? 2 : 6;
        // Closed-loop load: each virtual user waits for their own response. No retries.
        await Promise.all(Array.from({length: clients}, async (_, index) => {
            for (let turn = 0; turn < turns; ++turn) {
                const client = clientId(index), options = {revision: String(turn + 1)};
                await mixedOperation(records, index + turn, client, options);
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
            by_operation: byOperation, ...await processMemory(pid)}));
    });
}
async function heavySample(clients) {
    return withServer(async pid => {
        if(remoteState) for(let index=0;index<clients;index++) {
            const client=clientId(index);remoteSessions.set(client,0);
            const started=await request('',client,{action:'start'});
            assert.equal(started.status,200);assert.equal(started.data.ok,true);
        }
        const records=[], initial=await processMemory(pid), begin=performance.now();
        await Promise.all(Array.from({length:clients},async(_,index)=>{
            for(let turn=0;turn<2;turn++) {
                if(index<2) await measured(records,'factorial246000',()=>request(`246000!+${turn}*0`,clientId(index),{revision:String(turn+1)}),
                    data=>assert.equal(data.result,'4.8792926407E+1219336'));
                else await mixedOperation(records,index+turn,clientId(index),{revision:String(turn+1)});
            }
        }));
        const elapsed=performance.now()-begin, final=await processMemory(pid);
        const recovery=await request('2+2',clientId(clients));
        assert.equal(recovery.status,200);assert.equal(recovery.data.result,'4');
        const byOperation={};
        for(const operation of new Set(records.map(r=>r.operation)))
            byOperation[operation]=summary(records.filter(r=>r.operation===operation),elapsed);
        console.log(JSON.stringify({scenario:'mixed_heavy',clients,turns:2,heavy_clients:2,...summary(records,elapsed),
            by_operation:byOperation,initial_memory:initial,...final,
            server_cpu_delta_seconds:final.server_cpu_seconds-initial.server_cpu_seconds,
            recovery:true,recovery_ms:recovery.ms}));
        assert.ok(records.every(r=>!r.error || /^http:503:server_busy$/.test(r.error)), 'Unexpected mixed-heavy failure');
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
            expired_before_load: expired, ...summary(records, elapsed), ...await processMemory(pid)}));
    });
}
async function sustainedSample(clients) {
    return withServer(async pid => {
        for (let i = 0; i < clients; ++i) {
            if (remoteState) remoteSessions.set(clientId(i), 0);
            const started = await request('', clientId(i), {precision: 'full', action: 'start'});
            assert.equal(started.status, 200); assert.equal(started.data.ok, true);
        }
        const duration = quick ? 2000 : remoteState ? 300000 : 180000;
        const think = remoteState && !quick ? 1000 : 50, window = quick ? 1000 : remoteState ? 30000 : 10000;
        const records = [], states = [];
        let previous = 0, requests = 0, ok = 0, windows = 0;
        const initialMemory = await processMemory(pid);
        const errors = {}, begin = performance.now();
        let stopped = false;
        console.log(JSON.stringify({scenario: 'sustained_start', clients, duration_ms: duration,
            think_time_ms: think, ...initialMemory}));
        async function flush() {
            if (!records.length) return;
            // Preserve the array identity: in-flight requests already hold it.
            const snapshot = records.splice(0);
            const elapsed = performance.now() - begin, result = summary(snapshot, elapsed - previous);
            previous = elapsed; requests += result.requests; ok += result.ok; ++windows;
            for (const [error, count] of Object.entries(result.errors)) errors[error] = (errors[error] || 0) + count;
            const memory = await processMemory(pid);
            console.log(JSON.stringify({scenario: 'sustained_window', clients, window: windows,
                through_ms: elapsed, ...result, ...memory, memory_sample_elapsed_ms: performance.now() - begin}));
        }
        const users = Promise.all(Array.from({length: clients}, async (_, index) => {
            let revision = 0, value;
            for (let turn = 0; !stopped && performance.now() - begin < duration; ++turn) {
                const client = clientId(index);
                if (turn % 3 === 0) {
                    value = (index + 1) * 100000 + turn;
                    const expected = String(value);
                    const options = {precision: 'full', action: 'commit', revision: String(++revision)};
                    if (remoteState) remoteSessions.set(client, revision);
                    await measured(records, 'session_commit', () => request('x=' + expected, client, options),
                        data => assert.equal(data.result, expected));
                    if (revision % 10 === 0)
                        await measured(records, 'session_retry', () => request('x=' + expected, client, options),
                            data => assert.equal(data.result, expected));
                } else if (turn % 3 === 1) {
                    const expected = String(value);
                    await measured(records, 'session_read', () => exchange('/api/session/value?' +
                        new URLSearchParams({client, name: 'x'})), data => assert.equal(data.value.text, expected));
                } else await mixedOperation(records, index + Math.floor(turn / 3), client, {revision: String(turn + 1)});
                await delay(think);
            }
            states[index] = {revision, value: String(value)};
        }));
        // Fixed-size time windows discard request records after summarizing. The
        // load generator must not accumulate minutes of per-request heap state.
        const monitor = (async () => {
            while (!stopped && performance.now() - begin < duration) {
                await delay(Math.min(window, Math.max(1, duration - (performance.now() - begin))));
                await flush();
            }
        })();
        try { await Promise.all([users, monitor]); }
        finally { stopped = true; await Promise.allSettled([users, monitor]); }
        await flush();
        for (let i = 0; i < clients; ++i) {
            const state = await exchange('/api/session?' + new URLSearchParams({client: clientId(i)}));
            assert.equal(state.status, 200); assert.equal(state.data.ok, true);
            assert.equal(state.data.revision, String(states[i].revision));
            assert.equal(state.data.variable_count, 1);
            assert.equal(state.data.history_count, Math.min(16, states[i].revision));
            const value = await exchange('/api/session/value?' + new URLSearchParams({client: clientId(i), name: 'x'}));
            assert.equal(value.status, 200); assert.equal(value.data.value.text, states[i].value);
        }
        const recovery = await request('2+2', remoteState ? clientId(clients) : 'f'.repeat(32));
        assert.equal(recovery.status, 200); assert.equal(recovery.data.result, '4');
        if (ok !== requests) process.exitCode = 1;
        console.log(JSON.stringify({scenario: 'sustained', clients, requests, ok, errors, windows,
            elapsed_ms: performance.now() - begin, recovery: true, ...await processMemory(pid)}));
    });
}
async function overloadSample() {
    return withServer(async pid => {
        const holders = [];
        try {
            await Promise.all(Array.from({length: 4}, () => new Promise((resolve, reject) => {
                const socket = net.connect({host: '127.0.0.1', port}); holders.push(socket);
                socket.once('error', reject);
                socket.once('connect', () => { socket.write('G'); resolve(); });
            })));
            await delay(100);
            const records = [], begin = performance.now();
            await Promise.all(Array.from({length: 60}, (_, i) => measured(records, 'overload',
                () => request('2+2', clientId(i)), data => assert.equal(data.result, '4'))));
            const result = summary(records, performance.now() - begin);
            assert.ok(result.requests > result.ok, 'Held workers must cause observable rejection');
            assert.ok(records.every(record => !record.error || record.error === 'http:503:server_busy' ||
                /^transport:(ECONNRESET|EPIPE)$/.test(record.error)), 'Unexpected overload outcome');
            holders.forEach(socket => socket.destroy());
            await delay(250);
            const recovery = await request('2+2', 'f'.repeat(32));
            assert.equal(recovery.status, 200); assert.equal(recovery.data.result, '4');
            console.log(JSON.stringify({scenario: 'overload', clients: 60, incomplete_connections: 4,
                ...result, recovery: true, recovery_ms: recovery.ms, ...await processMemory(pid)}));
        } finally { holders.forEach(socket => socket.destroy()); }
    });
}
(async () => {
    if (load || sustained || heavy) {
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
            request_deadline_ms: 15000, connection_reuse: false, workers: remoteState ? remoteState.Workers : workers,
            queue_capacity: remoteState ? remoteState.QueueCapacity : queueCapacity,
            memory_scope: 'process_resident_and_peak_when_available', ...(remoteState ? {remote_state: path.resolve(remotePath),
                url: remoteState.Url, server: remoteState.Server, tunnel: remoteState.Tunnel} : {})}));
        if (heavy) {
            for(const clients of [10,30,60]) await heavySample(clients);
            if(!remoteState) await overloadSample();
            return;
        }
        if (sustained) {
            for (const clients of quick ? [2, 10] : [10, 30, 60]) await sustainedSample(clients);
            if (!remoteState) await overloadSample();
            return;
        }
        for (let repetition = 1; repetition <= (quick ? 1 : 3); ++repetition)
            for (const clients of quick ? [2, 10] : [10, 30, 60]) {
                await loadSample(clients, repetition); await sessionSample(clients, repetition);
            }
        return;
    }
    console.log('sample,fast_idle_ms,slow_ms,fast_queued_ms,slow_status');
    for (let sampleIndex = 1; sampleIndex <= 3; sampleIndex++) await sample(sampleIndex);
})().catch(error => {console.error(error); process.exitCode = 1;});
