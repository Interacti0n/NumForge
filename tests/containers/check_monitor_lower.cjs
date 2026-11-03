// Lower-severity regressions use dummy objects and container loopback only.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const {EventEmitter, once: eventOnce} = require('node:events');
const {createRequire} = require('node:module');
const appRequire = createRequire('/app/package.json');
const receipt = '/opt/numforge/monitor-patches.json';
const manifest = fs.existsSync(receipt) ? JSON.parse(fs.readFileSync(receipt, 'utf8')) : {};
if (manifest.packages?.some(p => p.name === 'qs') || process.env.NUMFORGE_FORCE_LOWER_CHECKS) {
    const qs = appRequire('qs');
    const exported = appRequire('@tootallnate/once');
    const waitOnce = typeof exported === 'function' ? exported : exported.default;
    async function server(t, app) {
        const listener = http.createServer(app), sockets = new Set();
        listener.on('connection', socket => {sockets.add(socket); socket.on('close', () => sockets.delete(socket));});
        listener.listen(0, '127.0.0.1'); await eventOnce(listener, 'listening');
        t.after(() => {for (const socket of sockets) socket.destroy(); listener.close();});
        return 'http://127.0.0.1:' + listener.address().port;
    }
    async function request(url, options = {}, body) {
        return new Promise((resolve, reject) => {
            const req = http.request(url, options, res => {
                let text = ''; res.setEncoding('utf8'); res.on('data', chunk => text += chunk);
                res.on('end', () => resolve({status: res.statusCode, body: text}));
            });
            req.setTimeout(1500, () => req.destroy(new Error('Fixture request timeout')));
            req.on('error', reject); req.end(body);
        });
    }
    test('root query parser and legacy proxy once use exact reviewed versions', () => {
        assert.equal(appRequire('qs/package.json').version, '6.16.1');
        assert.equal(appRequire('@tootallnate/once/package.json').version, '2.0.1');
        for (const name of ['express', 'body-parser']) {
            assert.equal(createRequire(appRequire.resolve(name + '/package.json')).resolve('qs'), appRequire.resolve('qs'));
        }
    });
    test('qs comma serialization handles null and undefined without an exception', () => {
        const result = qs.stringify({a: [null, undefined, 'value']}, {arrayFormat: 'comma', encodeValuesOnly: true});
        assert.equal(result, 'a=,,value');
    });
    test('qs does not invoke a non-callable constructor isBuffer property', () => {
        assert.equal(qs.stringify({a: {constructor: {isBuffer: 'owned'}}}), 'a%5Bconstructor%5D%5BisBuffer%5D=owned');
        assert.equal(qs.stringify({a: Buffer.from('fixture')}), 'a=fixture');
    });
    for (const encodeValuesOnly of [false, true]) {
        test('qs comma serialization survives attacker-shaped toString fields: encodeValuesOnly=' + encodeValuesOnly, () => {
            for (const value of [{toString: 'owned'}, Object.assign(Object.create(null), {nested: 'owned'})]) {
                const result = qs.stringify({a: [value, 'safe']}, {arrayFormat: 'comma', encodeValuesOnly});
                assert.equal(typeof result, 'string');
                assert(result.includes('safe'));
            }
        });
    }
    test('qs enforces array limits for repeated keys and bracket-key comma values', () => {
        const options = {arrayLimit: 2, throwOnLimitExceeded: true};
        assert.deepEqual(qs.parse('a[]=one&a[]=two', options), {a: ['one', 'two']});
        assert.throws(() => qs.parse('a[]=one&a[]=two&a[]=three', options), /array.*limit|limit.*array/i);
        assert.throws(() => qs.parse('a=one,two,three', {...options, comma: true}), /array.*limit|limit.*array/i);
        assert.throws(() => qs.parse('a[]=one,two,three', {...options, comma: true}), /array.*limit|limit.*array/i);
    });
    test('query parsing preserves nested data and does not modify Object.prototype', () => {
        assert.deepEqual(qs.parse('a[0][label]=first&a[1][label]=second&lang=sk'), {a: [{label: 'first'}, {label: 'second'}], lang: 'sk'});
        const parsed = qs.parse('__proto__[nfPolluted]=yes&constructor[prototype][nfPolluted]=yes&safe=ok');
        assert.deepEqual(parsed, {safe: 'ok'}); assert.equal({}.nfPolluted, undefined);
        assert.equal(qs.stringify({a: ['first', 'second'], lang: 'sk'}), 'a%5B0%5D=first&a%5B1%5D=second&lang=sk');
    });
    test('actual Express query/body parsing keeps Unicode and parameter/body limits', async t => {
        const express = appRequire('express'), app = express();
        app.use(express.urlencoded({extended: true, parameterLimit: 3, limit: '128b'}));
        app.get('/query', (req, res) => res.json(req.query)); app.post('/body', (req, res) => res.json(req.body));
        app.use((error, req, res, next) => res.status(error.status || 500).json({type: error.type}));
        const url = await server(t, app);
        const result = await request(url + '/query?a[0]=one&a[1]=two&lang=sk');
        assert.equal(result.status, 200); assert.deepEqual(JSON.parse(result.body), {a: ['one', 'two'], lang: 'sk'});
        const headers = {'Content-Type': 'application/x-www-form-urlencoded'};
        const body = await request(url + '/body', {method: 'POST', headers}, 'label=' + encodeURIComponent('Výpočet 😀'));
        assert.equal(body.status, 200); assert.deepEqual(JSON.parse(body.body), {label: 'Výpočet 😀'});
        assert.equal((await request(url + '/body', {method: 'POST', headers}, 'a=1&b=2&c=3&d=4')).status, 413);
        assert.equal((await request(url + '/body', {method: 'POST', headers}, 'a=' + 'x'.repeat(256))).status, 413);
    });
    async function abortCase(t, alreadyAborted) {
        const emitter = new EventEmitter(), controller = new AbortController();
        if (alreadyAborted) controller.abort();
        const pending = waitOnce(emitter, 'done', {signal: controller.signal}).then(value => ({value}), error => ({error}));
        if (!alreadyAborted) controller.abort();
        let timer;
        const result = await Promise.race([pending, new Promise(resolve => {timer = setTimeout(() => resolve({timeout: true}), 150);})]);
        clearTimeout(timer); t.after(() => emitter.removeAllListeners());
        assert.equal(result.timeout, undefined, 'Aborted promise must settle instead of hanging');
        assert.equal(result.error?.name, 'AbortError');
        assert.equal(emitter.listenerCount('done'), 0); assert.equal(emitter.listenerCount('error'), 0);
    }
    test('once settles and removes listeners when AbortSignal aborts', t => abortCase(t, false));
    test('once settles even when AbortSignal was already aborted', t => abortCase(t, true));
    test('once still resolves events, rejects errors and removes listeners', async () => {
        const emitter = new EventEmitter();
        const result = waitOnce(emitter, 'done'); emitter.emit('done', 'fixture');
        const value = await result; assert.equal(Array.isArray(value) ? value[0] : value, 'fixture');
        assert.equal(emitter.listenerCount('done'), 0); assert.equal(emitter.listenerCount('error'), 0);
        const failed = waitOnce(emitter, 'done'); emitter.emit('error', new Error('fixture failure'));
        await assert.rejects(failed, /fixture failure/);
        assert.equal(emitter.listenerCount('done'), 0); assert.equal(emitter.listenerCount('error'), 0);
    });
    test('actual legacy http-proxy-agent preserves routing and dummy Basic proxy auth', async t => {
        let received;
        const proxy = await server(t, (req, res) => {received = {url: req.url, auth: req.headers['proxy-authorization']}; res.end('fixture');});
        const proxyURL = new URL(proxy); proxyURL.username = 'fixture'; proxyURL.password = 'DUMMY';
        const legacyRequire = createRequire(appRequire.resolve('make-fetch-happen/package.json'));
        const exportedAgent = legacyRequire('http-proxy-agent'), Agent = exportedAgent.HttpProxyAgent || exportedAgent;
        const agent = new Agent(proxyURL.toString()); t.after(() => agent.destroy());
        const result = await request('http://fixture.example.test/path?key=value', {agent});
        assert.equal(result.status, 200); assert.equal(result.body, 'fixture');
        assert.equal(received.url, 'http://fixture.example.test/path?key=value');
        assert.equal(received.auth, 'Basic ' + Buffer.from('fixture:DUMMY').toString('base64'));
    });
} else {
    console.log('Original query/once packages: lower-severity checks deferred to the qualified candidate.');
}
