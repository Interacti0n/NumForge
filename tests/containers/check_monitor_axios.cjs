// Only disposable-container loopback endpoints and dummy credentials are used.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const zlib = require('node:zlib');
const {once} = require('node:events');
const {spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');
const appRequire = createRequire('/app/package.json');
const receipt = '/opt/numforge/monitor-patches.json';
const manifest = fs.existsSync(receipt) ? JSON.parse(fs.readFileSync(receipt, 'utf8')) : {};
if (manifest.directMigrations?.some(p => p.name === 'axios') || process.env.NUMFORGE_FORCE_AXIOS_CHECKS) {
    const axios = appRequire('axios');
    async function fixture(t, handler) {
        const server = http.createServer(handler);
        server.listen(0, '127.0.0.1'); await once(server, 'listening');
        t.after(() => {server.closeAllConnections(); server.close();});
        return 'http://127.0.0.1:' + server.address().port;
    }
    // Pollute only a short-lived child, so Node internals and other tests are isolated.
    function isolated(body) {
        const result = spawnSync(process.execPath, ['-e', "const assert=require('node:assert/strict');const axios=require('/app/node_modules/axios');\n" + body],
            {encoding: 'utf8', timeout: 7000, maxBuffer: 128 * 1024});
        assert.equal(result.error, undefined, result.error?.message);
        assert.equal(result.status, 0, result.stdout + result.stderr);
    }
    test('Axios uses the exact legacy pin without installing development wait-on', () => {
        assert.equal(appRequire('axios/package.json').version, '0.34.0');
        assert.equal(appRequire('follow-redirects/package.json').version, '1.16.2');
        assert.equal(fs.existsSync('/app/node_modules/wait-on'), false);
        const lock = appRequire('./package-lock.json');
        assert.equal(lock.packages['node_modules/wait-on'].dependencies.axios, '^1.6.1');
    });
    test('form serializer ignores an inherited visitor instead of exposing field values', () => isolated(`
        const seen=[];const fields=[];
        Object.prototype.visitor=function(value,key,path,helpers){seen.push(value);return helpers.defaultVisitor.call(this,value,key,path)};
        try {axios.toFormData({token:'dummy-token',profile:{name:'pilot'}},{append:(k,v)=>fields.push([k,v])});}
        finally {delete Object.prototype.visitor;}
        assert.deepEqual(seen,[]);assert.deepEqual(fields,[['token','dummy-token'],['profile[name]','pilot']]);
    `));
    test('inherited form depth cannot reject an otherwise valid small nested form', () => isolated(`
        Object.prototype.maxDepth=1;let fields=[];
        try {axios.toFormData({a:{b:{c:'value'}}},{append:(k,v)=>fields.push([k,v])});}
        finally {delete Object.prototype.maxDepth;}
        assert.deepEqual(fields,[['a[b][c]','value']]);
    `));
    test('serializer ignores inherited caller options and preserves explicit own options', () => {
        const fields = [];
        axios.toFormData({profile: {name: 'pilot'}}, {append: (k, v) => fields.push([k, v])}, Object.create({dots: true}));
        assert.deepEqual(fields, [['profile[name]', 'pilot']]);
        const explicit = [];
        axios.toFormData({profile: {name: 'pilot'}, items: ['a', 'b']}, {append: (k, v) => explicit.push([k, v])}, {dots: true, indexes: true});
        assert.deepEqual(explicit, [['profile.name', 'pilot'], ['items.0', 'a'], ['items.1', 'b']]);
        assert.throws(() => axios.toFormData({a: {b: {c: 'value'}}}, {append() {}}, {maxDepth: 1}), /depth/i);
    });
    for (const synchronous of [false, true]) {
        test('HTTP adapter ignores inherited proxy after ' + (synchronous ? 'synchronous' : 'asynchronous') + ' interceptor cloning', () => isolated(`
            const http=require('node:http');
            for(const key of Object.keys(process.env))if(/^(https?|all|no)_proxy$/i.test(key))delete process.env[key];
            let targetHits=0,proxyHits=0;const target=http.createServer((req,res)=>{targetHits++;res.end('target')});
            const proxy=http.createServer((req,res)=>{proxyHits++;res.end('proxy')});
            (async()=>{await new Promise(r=>target.listen(0,'127.0.0.1',r));await new Promise(r=>proxy.listen(0,'127.0.0.1',r));
                const proxyConfig={host:'127.0.0.1',port:proxy.address().port,protocol:'http'};
                const api=axios.create();api.interceptors.request.use(c=>({...c,headers:{...c.headers}}),undefined,{synchronous:${synchronous}});
                Object.prototype.proxy=proxyConfig;
                let response;
                try {response=await api.post('http://127.0.0.1:'+target.address().port,{token:'dummy'},
                    {timeout:1500,headers:{Authorization:'Bearer dummy'},auth:{username:'fixture',password:'dummy'}});}
                finally {delete Object.prototype.proxy;}
                assert.equal(response.data,'target');assert.equal(targetHits,1);assert.equal(proxyHits,0);
                const explicit=await api.get('http://127.0.0.1:'+target.address().port,{proxy:proxyConfig,timeout:1500});
                assert.equal(explicit.data,'proxy');assert.equal(proxyHits,1);
            })().catch(e=>{console.error(e);process.exitCode=1}).finally(()=>{target.closeAllConnections();proxy.closeAllConnections();target.close();proxy.close()});
        `));
    }
    test('inherited default method cannot change a request while explicit POST still works', () => isolated(`
        (async()=>{const api=axios.create({adapter:c=>Promise.resolve({data:c.method,status:200,statusText:'OK',headers:{},config:c})});
            Object.prototype.method='post';let method;
            try {method=(await api('/fixture')).data;} finally {delete Object.prototype.method;}
            assert.equal(method,'get');assert.equal((await api('/fixture',{method:'post'})).data,'post');
        })().catch(e=>{console.error(e);process.exitCode=1});
    `));
    test('NO_PROXY CIDR bypasses the environment proxy while an own proxy remains explicit', () => isolated(`
        const http=require('node:http');
        for(const key of Object.keys(process.env))if(/^(https?|all|no)_proxy$/i.test(key))delete process.env[key];
        let proxyHits=0;const target=http.createServer((req,res)=>res.end('target'));
        const proxy=http.createServer((req,res)=>{proxyHits++;res.end('proxy')});
        (async()=>{await new Promise(r=>target.listen(0,'127.0.0.1',r));await new Promise(r=>proxy.listen(0,'127.0.0.1',r));
            process.env.HTTP_PROXY='http://127.0.0.1:'+proxy.address().port;process.env.NO_PROXY='127.0.0.0/8';
            const url='http://127.0.0.1:'+target.address().port;
            assert.equal((await axios.get(url,{timeout:1500})).data,'target');assert.equal(proxyHits,0);
            assert.equal((await axios.get(url,{timeout:1500,proxy:{host:'127.0.0.1',port:proxy.address().port,protocol:'http'}})).data,'proxy');
            assert.equal(proxyHits,1);
        })().catch(e=>{console.error(e);process.exitCode=1}).finally(()=>{target.closeAllConnections();proxy.closeAllConnections();target.close();proxy.close()});
    `));
    test('HTTP JSON, Basic auth, headers and redirects preserve monitor request behavior', async t => {
        const calls = [];
        const url = await fixture(t, async (req, res) => {
            let body = ''; for await (const chunk of req) body += chunk;
            calls.push({url: req.url, method: req.method, auth: req.headers.authorization, custom: req.headers['x-fixture'], body});
            if (req.url === '/redirect') {res.writeHead(307, {location: '/target'}); res.end();}
            else {res.setHeader('content-type', 'application/json'); res.end(JSON.stringify({ok: true}));}
        });
        const response = await axios.post(url + '/redirect', {label: 'Výpočet 😀'}, {proxy: false, timeout: 1500,
            auth: {username: 'fixture', password: 'dummy'}, headers: {'X-Fixture': 'owned'}});
        assert.deepEqual(response.data, {ok: true}); assert.equal(calls.length, 2);
        assert.equal(calls[0].auth, 'Basic ' + Buffer.from('fixture:dummy').toString('base64'));
        // The existing follow-redirects adapter drops config.auth on redirect.
        assert.equal(calls[1].auth, undefined);
        for (const call of calls) {
            assert.equal(call.method, 'POST');
            assert.equal(call.custom, 'owned'); assert.deepEqual(JSON.parse(call.body), {label: 'Výpočet 😀'});
        }
    });
    test('HTTP timeout, cancellation and decompressed response limits remain enforced', async t => {
        const url = await fixture(t, (req, res) => {
            if (req.url === '/gzip') {res.setHeader('content-encoding', 'gzip'); res.end(zlib.gzipSync('x'.repeat(256)));}
        });
        await assert.rejects(axios.get(url, {proxy: false, timeout: 50}), e => e.code === 'ECONNABORTED');
        const source = axios.CancelToken.source();
        const pending = axios.get(url, {proxy: false, timeout: 1500, cancelToken: source.token}); source.cancel('fixture cancelled');
        await assert.rejects(pending, e => axios.isCancel(e));
        await assert.rejects(axios.get(url + '/gzip', {proxy: false, timeout: 1500, maxContentLength: 32}), /maxContentLength/);
    });
    for (const kind of ['json', 'get', 'form-data', 'custom']) {
        test('actual Kuma webhook provider delivers ' + kind + ' to a local fixture', async t => {
            let received;
            const url = await fixture(t, async (req, res) => {
                let body = ''; for await (const chunk of req) body += chunk;
                received = {method: req.method, url: req.url, headers: req.headers, body}; res.end('{}');
            });
            const Webhook = appRequire('./server/notification-providers/webhook.js');
            const notification = {webhookURL: url, httpMethod: kind === 'get' ? 'GET' : 'POST', webhookContentType: kind,
                webhookAdditionalHeaders: JSON.stringify({'X-Fixture': 'owned'}), webhookCustomBody: '{{ name }}|{{ msg }}'};
            const monitor = {name: 'Výpočet', type: 'http', url}, heartbeat = {status: 1};
            assert.equal(await new Webhook().send(notification, 'fixture 😀', monitor, heartbeat), 'Sent Successfully.');
            assert.equal(received.headers['x-fixture'], 'owned');
            const payload = {heartbeat, monitor, msg: 'fixture 😀'};
            if (kind === 'get') {
                assert.equal(received.method, 'GET'); const params = new URL(received.url, url).searchParams;
                assert.equal(params.get('msg'), payload.msg); assert.deepEqual(JSON.parse(params.get('monitor')), monitor);
                assert.deepEqual(JSON.parse(params.get('heartbeat')), heartbeat);
            } else if (kind === 'form-data') {
                assert.match(received.headers['content-type'], /^multipart\/form-data; boundary=/);
                assert.match(received.body, /name="data"/); assert(received.body.includes(JSON.stringify(payload)));
            } else if (kind === 'custom') {assert.equal(received.body, 'Výpočet|fixture 😀');}
            else {assert.deepEqual(JSON.parse(received.body), payload);}
        });
    }
    test('actual webhook reports HTTP failures and rejects malformed headers before sending', async t => {
        let calls = 0;
        const url = await fixture(t, (req, res) => {calls++; res.writeHead(503); res.end('fixture unavailable');});
        const Webhook = appRequire('./server/notification-providers/webhook.js');
        await assert.rejects(new Webhook().send({webhookURL: url}, 'fixture'), /HTTP 503.*fixture unavailable/);
        await assert.rejects(new Webhook().send({webhookURL: url, webhookAdditionalHeaders: '{'}, 'fixture'), /Additional Headers is not a valid JSON/);
        assert.equal(calls, 1);
    });
} else {
    console.log('Original Axios range: migration checks deferred to the qualified candidate.');
}
