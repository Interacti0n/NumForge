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

async function run(publicOrigin) {
    const reservation = net.createServer();
    await new Promise(resolve => reservation.listen(0, '127.0.0.1', resolve));
    const port = reservation.address().port;
    await new Promise(resolve => reservation.close(resolve));
    const args = ['--port', String(port), '--no-browser'];
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
    } finally {
        child.kill();
        await exited;
    }
}

(async () => {
    for (const value of ['*', 'https://example.com/', 'https://example.com:443']) {
        assert.equal(spawnSync(executable, ['--origin', value], { windowsHide: true }).status, 2);
    }
    await run(null);
    await run(origin);
    console.log('Default and public-origin server integration checks passed.');
})().catch(error => { console.error(error); process.exitCode = 1; });
