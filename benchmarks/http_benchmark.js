/* Real loopback requests, one fresh server per scenario/sample. Node is a
 * benchmark driver only; the installed application does not require it. */
const { spawn } = require('node:child_process');
const http = require('node:http');
const path = require('node:path');
const assert = require('node:assert/strict');
const phases = ['other', 'parse', 'evaluate', 'format', 'convert', 'round', 'compose', 'serialize', 'send'];
const scenarios = ['cold', 'hit', 'precision', 'notation', 'angle', 'expression',
    'approx_precision', 'stale', 'eviction', 'session_cold', 'session_hit',
    'commit', 'random', 'session_expiration', 'math_copy', 'output_limit'];
const args = process.argv.slice(2);
const executable = args.shift();
const quick = args.includes('--quick');
const selection = args.includes('--case') ? args[args.indexOf('--case') + 1] : null;
if (!executable || (selection && !scenarios.includes(selection)) ||
    args.some((v, i) => !['--quick', '--case'].includes(v) && args[i - 1] !== '--case')) {
    throw Error('Usage: node benchmarks/http_benchmark.js server-executable [--quick] [--case name]');
}
const port = Number(process.env.NUMFORGE_BENCH_PORT || 18771);
assert(Number.isInteger(port) && port >= 1024 && port <= 65535);
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
const median = values => [...values].sort((a, b) => a - b)[Math.floor(values.length / 2)];

function post(query, input) {
    return new Promise((resolve, reject) => {
        const begin = process.hrtime.bigint();
        const req = http.request({ hostname: '127.0.0.1', port, method: 'POST',
            path: '/api/evaluate?' + new URLSearchParams(query), agent: false,
            headers: { 'Content-Type': 'text/plain', 'Content-Length': Buffer.byteLength(input) } }, response => {
            let body = '';
            response.setEncoding('utf8');
            response.on('data', chunk => { body += chunk; });
            response.on('error', reject);
            response.on('end', () => {
                try { resolve({ status: response.statusCode, data: JSON.parse(body),
                    ns: Number(process.hrtime.bigint() - begin), bytes: Buffer.byteLength(body) }); }
                catch (error) { reject(error); }
            });
        });
        req.setTimeout(10000, () => req.destroy(Error('HTTP benchmark timeout')));
        req.on('error', reject);
        req.end(input);
    });
}

async function sample(name, memory) {
    const server = spawn(path.resolve(executable), ['--no-browser', '--port', String(port)],
        { env: { ...process.env, NUMFORGE_BENCH_TRACE: '1', NUMFORGE_BENCH_MEMORY: memory ? '1' : '0' },
          stdio: ['ignore', 'ignore', 'pipe'], windowsHide: true });
    const records = [];
    let pending = '', diagnostics = '', spawnError;
    server.on('error', error => { spawnError = error; });
    server.stderr.setEncoding('utf8');
    server.stderr.on('data', chunk => {
        pending += chunk;
        for (;;) {
            const at = pending.indexOf('\n');
            if (at < 0) break;
            const line = pending.slice(0, at).trim(); pending = pending.slice(at + 1);
            if (line.startsWith('BENCH,')) records.push(line.split(',').slice(1).map(Number));
            else diagnostics += line + '\n';
        }
    });
    async function send(query, input) {
        const index = records.length;
        const response = await post(query, input);
        for (let i = 0; records.length <= index && i < 100; i++) await delay(5);
        assert.equal(records.length, index + 1, 'Missing benchmark trace; use a benchmark-enabled server');
        return { ...response, record: records[index] };
    }
    const id = 'a'.repeat(32);
    let query = { precision: '10', angle: 'rad', notation: 'auto', client: id, revision: '1' };
    let input = name === 'approx_precision' ? 'sqrt(2)' : '1/3';
    const session = name.startsWith('session_') || ['commit', 'random'].includes(name);
    try {
        let ready = false;
        for (let i = 0; i < 100; i++) {
            if (spawnError) throw spawnError;
            if (server.exitCode !== null) throw Error('Server exited: ' + diagnostics);
            try {
                await new Promise((resolve, reject) => {
                    const req = http.get({ hostname: '127.0.0.1', port, path: '/', agent: false }, res => {
                        res.resume(); res.on('end', () => res.statusCode === 200 ? resolve() : reject(Error('Not ready')));
                    }); req.on('error', reject);
                });
                ready = true; break;
            } catch { await delay(20); }
        }
        assert(ready, 'Server readiness timeout');
        if (session) {
            assert((await send({ ...query, action: 'start' }, '')).data.ok);
            query.action = 'preview';
        }
        let previous;
        if (!['cold', 'session_cold', 'math_copy', 'output_limit'].includes(name)) {
            if (name === 'random') input = 'rand()+rand()';
            previous = (await send(query, input)).data;
            assert(previous.ok); assert.equal(previous.cached, false);
            query.revision = '3';
        }
        let expectedHit = ['hit', 'precision', 'notation', 'session_hit', 'commit', 'random'].includes(name);
        if (['precision', 'approx_precision'].includes(name)) query.precision = '100';
        if (name === 'notation') query.notation = 'scientific';
        if (name === 'angle') query.angle = 'deg';
        if (name === 'expression') input = '2/3';
        if (name === 'commit') query.action = 'commit';
        if (name === 'stale') {
            assert((await send({ ...query, revision: '3' }, input)).data.cached);
            query.revision = '2'; input = '7';
        }
        if (name === 'eviction' || name === 'session_expiration') {
            for (let i = 1; i <= 8; i++) {
                const other = { ...query, client: i.toString(16).padStart(32, '0'), revision: '1' };
                if (session) other.action = 'start';
                assert((await send(other, session ? '' : '42')).data.ok);
            }
        }
        if (name === 'math_copy') { input = '123.456'; query.precision = '2'; query.notation = 'math'; }
        if (name === 'output_limit') { input = '1E100000'; query.notation = 'plain'; }
        // Independent cold-path reference, obtained before the measured request.
        let reference;
        if (!['random', 'session_expiration', 'output_limit'].includes(name)) {
            const fresh = { precision: query.precision, angle: query.angle, notation: query.notation };
            reference = (await send(fresh, input)).data;
            assert(reference.ok);
        }
        const response = await send(query, input);
        const record = response.record;
        assert(record && record.length === 15 && record[0] === 1, 'Missing/incomplete phase or memory trace');
        if (name === 'session_expiration') {
            assert.equal(response.status, 400); assert.match(response.data.status, /session expired/);
        } else if (name === 'output_limit') {
            assert.equal(response.data.ok, false); assert.match(response.data.status, /value too large/);
        } else {
            assert(response.data.ok); assert.equal(response.data.cached, expectedHit);
            assert.equal(response.data.result, name === 'random' ? previous.result : reference.result);
            if (response.data.result.includes('/')) assert.equal(typeof response.data.approx, 'string');
            if (name === 'math_copy') assert.equal(response.data.copy, '1.2346E+2');
        }
        if (name === 'stale') assert((await send({ ...query, revision: '4' }, '1/3')).data.cached);
        if (name === 'commit') assert.equal((await send({ ...query, revision: '4', action: 'preview' }, 'ans')).data.result, response.data.result);
        return { ...response, record };
    } finally {
        const exited = new Promise(resolve => server.once('exit', resolve));
        if (server.exitCode === null && !spawnError) { server.kill(); await exited; }
    }
}

(async () => {
    console.error('process_peak_metric=' + (process.platform === 'win32' ? 'peak_working_set' : 'peak_rss') +
        ' process_peak_scope=isolated_scenario profile=exclusive live_tracking=separate_sample');
    console.log('case,samples,min_roundtrip_ns,median_roundtrip_ns,max_roundtrip_ns,response_bytes,' +
        'alloc_calls,requested_bytes,live_after_bytes,peak_tracked_payload_bytes,process_peak_bytes,' + phases.map(p => p + '_ns').join(','));
    for (const name of selection ? [selection] : scenarios) {
        const values = [];
        for (let i = 0; i < (quick ? 3 : 7); i++) values.push(await sample(name, false));
        const memory = await sample(name, true);
        const elapsed = values.map(v => v.ns);
        console.log([name, values.length, Math.min(...elapsed), median(elapsed), Math.max(...elapsed),
            memory.bytes, ...memory.record.slice(1, 6),
            ...phases.map((_, i) => median(values.map(v => v.record[6 + i] * 1e9)).toFixed(3))].join(','));
    }
})().catch(error => { console.error(error); process.exitCode = 1; });
