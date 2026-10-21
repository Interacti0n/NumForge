// Exercise the actual server, including POST origin enforcement behind a proxy.
const assert = require('node:assert/strict');
const { spawn, spawnSync } = require('node:child_process');
const net = require('node:net');
const http = require('node:http');
const executable = process.argv[2];
if (!executable) throw Error('Pass the numforge_web executable');
const origin = 'https://calculator.example.com';
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
function request(url, options = {}) {
    return new Promise((resolve, reject) => {
        const req = http.request(url, { method: options.method || 'GET', headers: {
            ...options.headers, Connection: 'close',
            ...(options.body !== undefined ? { 'Content-Length': Buffer.byteLength(options.body) } : {}),
        } }, response => {
            let body = '';
            response.setEncoding('utf8');
            response.on('data', chunk => { body += chunk; });
            response.on('end', () => resolve({ status: response.statusCode, body }));
            response.on('error', reject);
        });
        req.on('error', reject);
        req.setTimeout(5000, () => req.destroy(Error('HTTP timeout')));
        req.end(options.body);
    });
}

async function run(publicOrigin, overload = false) {
    const reservation = net.createServer();
    await new Promise(resolve => reservation.listen(0, '127.0.0.1', resolve));
    const port = reservation.address().port;
    await new Promise(resolve => reservation.close(resolve));
    const args = ['--port', String(port), '--no-browser'];
    if (overload) args.push('--workers', '1', '--queue-capacity', '1');
    if (publicOrigin) args.push('--origin', publicOrigin);
    const child = spawn(executable, args, { windowsHide: true, stdio: 'ignore' });
    const exited = new Promise(resolve => child.once('exit', resolve));
    const base = `http://127.0.0.1:${port}`;
    try {
        let ready = false;
        for (let attempt = 0; attempt < 100; attempt++) {
            try { ready = (await request(base)).status === 200; } catch {}
            if (ready) break;
            if (child.exitCode !== null) throw Error('Server exited during startup');
            await delay(50);
        }
        assert.ok(ready, 'Server started');
        if (overload) {
            const slow = net.createConnection({host: '127.0.0.1', port});
            slow.on('error', () => {}); slow.resume();
            try {
                await new Promise((resolve, reject) => {slow.once('connect', resolve); slow.once('error', reject);});
                slow.write('G');
                await delay(50);
                const responses = await Promise.all(Array.from({length: 4}, () =>
                    request(base + '/api/evaluate', {method: 'POST', body: '2+2'}).catch(error => {
                        // Immediate overload closure can race an unread TCP body.
                        assert.ok(['ECONNRESET', 'EPIPE'].includes(error.code), error.code); return null;
                    })));
                assert.ok(responses.some(Boolean), 'At least the queued request receives an expiry reply');
                for (const response of responses) {
                    if (!response) continue;
                    assert.equal(response.status, 503, 'Queue expires without waiting for the occupied worker');
                    assert.equal(JSON.parse(response.body).code, 'server_busy');
                }
            } finally {slow.destroy();}
            let recovered = false;
            for (let attempt = 0; attempt < 30; ++attempt) {
                const response = await request(base + '/api/evaluate', {method: 'POST', body: '2+2'});
                if (response.status === 200) {assert.equal(JSON.parse(response.body).result, '4'); recovered = true; break;}
                await delay(50);
            }
            assert.ok(recovered, 'Server recovers after overload');
            return;
        }
        for (const [header, expected] of [
            [origin, publicOrigin ? 200 : 403],
            [origin + '.evil.test', 403],
            ['http://calculator.example.com', 403],
            ['https://calculator.example.com:8443', 403],
            ['null', 403],
            [`http://127.0.0.1:${port}`, 200],
            [`http://localhost:${port}`, 200],
        ]) {
            const response = await request(base + '/api/evaluate?precision=10&angle=rad', {
                method: 'POST', headers: { Origin: header }, body: '2+2',
            });
            assert.equal(response.status, expected, header);
            const body = JSON.parse(response.body);
            if (expected === 200) { assert.equal(body.ok, true); assert.equal(body.result, '4'); }
        }
        const foreign = await request(base + '/api/evaluate?precision=10&angle=rad', {
            method: 'POST', headers: { Origin: 'https://evil.test', 'X-Forwarded-Host': 'calculator.example.com' }, body: '2+2',
        });
        assert.equal(foreign.status, 403, 'Forwarded headers do not grant access');
        const replies = await Promise.all(Array.from({length: 12}, (_, index) =>
            request(base + '/api/evaluate', {method: 'POST', body: `${index}^2+1`})));
        replies.forEach((reply, index) => {
            assert.equal(reply.status, 200); assert.equal(JSON.parse(reply.body).result, String(index * index + 1));
        });
        const ids = ['a'.repeat(32), 'b'.repeat(32)];
        const evaluate = (id, revision, body, action = 'commit') => request(
            `${base}/api/evaluate?precision=full&angle=rad&client=${id}&revision=${revision}&action=${action}`,
            {method: 'POST', body});
        for (const [index, id] of ids.entries()) {
            assert.equal((await evaluate(id, 1, '', 'start')).status, 200);
            assert.equal((await evaluate(id, 1, `x=${index + 7}`)).status, 200);
        }
        const repeated = await Promise.all([evaluate(ids[0], 2, 'x=x+1'), evaluate(ids[0], 2, 'x=x+1')]);
        repeated.forEach(reply => {
            assert.equal(reply.status, 200); assert.equal(JSON.parse(reply.body).result, '8');
        });
        const states = await Promise.all(ids.map(id => request(`${base}/api/session?client=${id}`)));
        assert.equal(JSON.parse(states[0].body).history_count, 2, 'Concurrent retry commits once');
        assert.equal(JSON.parse(states[1].body).revision, '1', 'Other session is unchanged');
        assert.equal(JSON.parse((await evaluate(ids[1], 2, 'x+10')).body).result, '18');
    } finally {
        child.kill();
        const code = await exited;
        if (process.platform !== 'win32') assert.equal(code, 0, 'SIGTERM joins workers and exits cleanly');
    }
}

(async () => {
    for (const value of ['*', 'https://example.com/', 'https://example.com:443']) {
        assert.equal(spawnSync(executable, ['--origin', value], { windowsHide: true }).status, 2);
    }
    for (const args of [['--workers', '0'], ['--workers', '17'], ['--workers'],
        ['--queue-capacity', '0'], ['--queue-capacity', '65'], ['--queue-capacity']])
        assert.equal(spawnSync(executable, args, {windowsHide: true}).status, 2);
    await run(null);
    await run(origin);
    await run(null, true);
    console.log('Origin, concurrent sessions, bounded overload and recovery checks passed.');
})().catch(error => { console.error(error); process.exitCode = 1; });
