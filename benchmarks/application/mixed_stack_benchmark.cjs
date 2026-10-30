'use strict';
// Clone the deployed application into disposable volumes; never load-test its DB.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const https = require('node:https');
const os = require('node:os');
const path = require('node:path');
const readline = require('node:readline');
const {execFileSync, spawn} = require('node:child_process');
const ROOT = path.resolve(__dirname, '../..');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
const secretFiles = {admin: 'admin.json', migrate: 'migrate.json', app: 'app.json',
    admin_password: 'admin-password', gateway_token: 'gateway-token'};

function isolate(source, project, secrets, images) {
    assert.match(project, /^numforge-test-mixed-[a-f0-9-]+$/);
    const profile = structuredClone(source);
    profile.name = project;
    for (const [key, volume] of Object.entries(profile.volumes)) {
        assert.ok(!volume.external, 'External volumes cannot be benchmark fixtures');
        volume.name = `${project}_${key}`;
    }
    for (const [key, network] of Object.entries(profile.networks)) {
        assert.ok(!network.external, 'External networks cannot be benchmark fixtures');
        network.name = `${project}_${key}`;
    }
    for (const [key, secret] of Object.entries(profile.secrets)) {
        assert.ok(secretFiles[key] && !secret.external, 'Unknown/external secret');
        secret.file = path.join(secrets, secretFiles[key]);
        secret.name = `${project}_${key}`;
    }
    const binds = new Set(['deploy/Caddyfile', 'scripts/setup_monitor.cjs'].map(p => path.join(ROOT, p)));
    for (const [name, service] of Object.entries(profile.services)) {
        assert.ok(images[name], 'Missing pinned image for ' + name);
        service.image = images[name];
        delete service.build;
        service.restart = 'no';
        service.ports = [];
        for (const mount of service.volumes || []) {
            if (mount.type === 'bind') assert.ok(mount.read_only && binds.has(path.resolve(mount.source)), 'Unapproved host mount');
            else assert.ok(mount.type === 'volume' && profile.volumes[mount.source], 'Unowned data mount');
        }
    }
    const published = (target, port) => [{host_ip: '127.0.0.1', target, published: String(port), protocol: 'tcp'}];
    profile.services.web.ports = published(18876, 18883);
    profile.services.gateway.ports = published(8443, 18884);
    profile.services.uptime.ports = published(3001, 18886);
    for (const name of ['accounts', 'gateway']) Object.assign(profile.services[name].environment,
        {NF_ACCOUNT_PUBLIC_ORIGIN: 'https://localhost:18884', NF_LOCAL_ORIGIN: 'https://localhost:18884'});
    return profile;
}

function decimal(text) {
    const match = /^([+-]?)(\d+)(?:\.(\d*))?(?:[eE]([+-]?\d+))?$/.exec(text);
    assert.ok(match, 'Non-decimal response');
    const raw = match[2] + (match[3] || '');
    const first = raw.search(/[1-9]/);
    return first < 0 ? '0' : `${match[1] === '-' ? '-' : ''}${raw.slice(first).replace(/0+$/, '')}e${Number(match[4] || 0) + match[2].length - first - 1}`;
}

function bytes(text) {
    const match = /^([\d.]+)\s*(B|[kMGT]i?B)$/i.exec(text.trim());
    assert.ok(match, 'Unknown Docker memory unit: ' + text);
    const power = ({b: 0, kb: 1, kib: 1, mb: 2, mib: 2, gb: 3, gib: 3, tb: 4, tib: 4})[match[2].toLowerCase()];
    return Number(match[1]) * (match[2].includes('i') ? 1024 : 1000) ** power;
}

function statsLine(line) {
    const clean = line.replace(/\x1b\[[0-?]*[ -/]*[@-~]/g, '').trim();
    if (!clean) return null;
    const value = JSON.parse(clean);
    return {name: value.Name, cpu_percent: parseFloat(value.CPUPerc), memory_bytes: bytes(value.MemUsage.split('/')[0]), pids: Number(value.PIDs)};
}

function summary(records) {
    const groups = {};
    for (const record of records) (groups[record.kind] ||= []).push(record);
    return Object.fromEntries(Object.entries(groups).map(([kind, values]) => {
        const sorted = values.filter(v => !v.error).map(v => v.ms).sort((a, b) => a - b);
        const percentile = p => sorted.length ? sorted[Math.ceil(sorted.length * p) - 1] : null;
        const errors = {};
        for (const value of values) if (value.error) errors[value.error] = (errors[value.error] || 0) + 1;
        return [kind, {requests: values.length, verified: sorted.length, errors,
            p50_ms: percentile(.5), p95_ms: percentile(.95), max_ms: sorted.at(-1) ?? null}];
    }));
}

class Client {
    constructor(base, ca, ip) {
        this.base = new URL(base);
        this.ip = ip;
        this.agent = new https.Agent({ca, keepAlive: true, maxSockets: 1});
        this.cookies = {};
        this.csrf = null;
        this.id = crypto.randomBytes(16).toString('hex');
        this.revision = 0;
    }
    exchange(route, body, json = false) {
        return new Promise((resolve, reject) => {
            const payload = body === undefined ? null : json ? JSON.stringify(body) : body;
            const headers = {Origin: this.base.origin, Cookie: Object.entries(this.cookies).map(([k, v]) => `${k}=${v}`).join('; ')};
            if (this.ip) headers['Cf-Connecting-IP'] = this.ip; // Isolated trusted-edge simulation only.
            if (this.csrf) headers['X-CSRFToken'] = this.csrf;
            if (payload !== null) Object.assign(headers, {'Content-Type': json ? 'application/json' : 'text/plain', 'Content-Length': Buffer.byteLength(payload)});
            const request = https.request(new URL(route, this.base), {agent: this.agent,
                method: payload === null ? 'GET' : 'POST', headers}, response => {
                const chunks = []; let size = 0;
                response.on('data', chunk => {size += chunk.length; if (size > 262144) request.destroy(Error('response_limit')); else chunks.push(chunk);});
                response.on('error', reject);
                response.on('end', () => {
                    try {
                        for (const cookie of response.headers['set-cookie'] || []) {
                            const pair = cookie.split(';')[0], at = pair.indexOf('='), name = pair.slice(0, at), value = pair.slice(at + 1);
                            if (!name.startsWith('numforge_')) continue;
                            if (!value || /max-age=0(?:;|$)/i.test(cookie)) delete this.cookies[name];
                            else {assert.match(cookie, /;\s*secure(?:;|$)/i); this.cookies[name] = value;}
                        }
                        const data = JSON.parse(Buffer.concat(chunks).toString('utf8'));
                        if (data.csrf_token) this.csrf = data.csrf_token;
                        resolve({status: response.statusCode, headers: response.headers, data});
                    } catch (error) {reject(error);}
                });
            });
            const deadline = setTimeout(() => request.destroy(Error('deadline')), 20000);
            request.on('close', () => clearTimeout(deadline));
            request.on('error', reject);
            request.end(payload);
        });
    }
    calculate(expression, precision = '10', action) {
        let route = `/api/evaluate?precision=${precision}&angle=rad&notation=${precision === '1000' ? 'plain' : 'scientific'}`;
        if (action) route += `&client=${this.id}&revision=${action === 'start' ? 1 : ++this.revision}&action=${action}`;
        return this.exchange(route, expression);
    }
    close() {this.agent.destroy();}
}

async function main(args) {
    if (args.includes('--help')) return console.log('node benchmarks/application/mixed_stack_benchmark.cjs [--quick] [--seconds 2..60] [--sustain-seconds 0..300] [--python PATH] [--output NEW_BUILD_DIRECTORY] [--live-probes]');
    const seen = new Set();
    for (let i = 0; i < args.length; i++) {
        const flag = args[i];
        assert.ok(!seen.has(flag), 'Repeated option: ' + flag); seen.add(flag);
        if (['--seconds', '--sustain-seconds', '--python', '--output'].includes(flag)) assert.ok(args[++i] && !args[i].startsWith('--'), 'Missing option value');
        else assert.ok(['--quick', '--live-probes'].includes(flag), 'Unknown option: ' + flag);
    }
    const quick = args.includes('--quick'), liveProbes = args.includes('--live-probes');
    const option = (name, fallback) => args.includes(name) ? args[args.indexOf(name) + 1] : fallback;
    const duration = Number(option('--seconds', quick ? '2' : '20'));
    const sustained = Number(option('--sustain-seconds', quick ? '0' : '120'));
    assert.ok(Number.isInteger(duration) && duration >= 2 && duration <= 60);
    assert.ok(Number.isInteger(sustained) && sustained >= 0 && sustained <= 300);
    const python = option('--python', process.platform === 'win32' ? path.join(ROOT, '.local/accounts/venv/Scripts/python.exe') : 'python3');
    const output = path.resolve(option('--output', path.join(ROOT, 'build/mixed-stack-' + Date.now())));
    assert.ok(output.startsWith(path.join(ROOT, 'build') + path.sep), 'Record only under build/');
    fs.mkdirSync(output); // Existing recordings and secrets are never overwritten.
    const project = 'numforge-test-mixed-' + crypto.randomBytes(6).toString('hex');
    const environment = {...process.env, COMPOSE_PROJECT_NAME: project, NF_ACCOUNT_PUBLIC_ORIGIN: 'https://localhost:18884'};
    delete environment.COMPOSE_FILE;
    const command = (program, argv, options = {}) => execFileSync(program, argv, {cwd: ROOT, env: environment, windowsHide: true, ...options});
    const docker = (argv, options) => command('docker', argv, options);
    const source = JSON.parse(fs.readFileSync(path.join(ROOT, '.local/compose/pilot-runtime.json')));
    const images = {}, metadata = {};
    // Pin the running containers, not mutable tags or the current unbuilt source tree.
    for (const name of ['web', 'accounts', 'db', 'gateway', 'uptime']) {
        const live = JSON.parse(docker(['inspect', 'numforge-' + name + '-1']))[0];
        assert.equal(live.Config.Labels['com.docker.compose.project'], 'numforge');
        const tag = `${project}-${name}:fixture`;
        docker(['tag', live.Image, tag]);
        images[name] = tag;
        metadata[name] = {image: live.Image, configured_image: live.Config.Image};
    }
    images.setup = images.accounts;
    const secrets = path.join(output, 'private');
    command(python, ['-c', "import sys;from pathlib import Path;sys.path.insert(0,'scripts');import compose;compose.STATE=Path(sys.argv[1]);compose.init()", secrets]);
    const config = path.join(output, 'compose.json');
    fs.writeFileSync(config, JSON.stringify(isolate(source, project, secrets, images), null, 2));
    const compose = argv => docker(['compose', '--project-directory', ROOT, '-f', config, ...argv], {stdio: 'inherit'});
    const cases = JSON.parse(command(python, ['-c', `import decimal,json,math
with decimal.localcontext() as c:
 c.prec=1001; root=str(decimal.Decimal(2).sqrt())
 c.prec=11; fact=str(+decimal.Decimal(math.factorial(${quick ? 1000 : 100000})))
print(json.dumps({'sqrt':root,'factorial':fact}))`]));
    const rows = [], resources = [], host = [], clients = [];
    let phase = 'setup', stopped = false, stats, sampleTimer;
    const signals = () => {stopped = true;};
    process.on('SIGINT', signals); process.on('SIGTERM', signals);
    const measured = async (records, kind, operation, verify) => {
        const begin = performance.now();
        try {
            const value = await operation();
            try {verify(value);} catch (error) {
                error.code = value.status === 200 ? 'verification' : `HTTP_${value.status}_${String(value.data.code || value.data.status || value.data.error || '').replace(/\W+/g, '_')}`;
                throw error;
            }
            records.push({kind, ms: performance.now() - begin}); return value;
        }
        catch (error) {records.push({kind, ms: performance.now() - begin, error: error.code || error.name || 'error'}); return null;}
    };
    const success = response => {assert.equal(response.status, 200); assert.equal(response.data.ok, true);};
    const session = (response, id) => {assert.equal(response.status, 200); assert.equal(response.data.user?.id ?? null, id);};
    const engine = JSON.parse(docker(['info', '--format', '{{json .}}']));
    const result = {project, source_head: command('git', ['rev-parse', 'HEAD']).toString().trim(), images: metadata,
        driver_sha256: crypto.createHash('sha256').update(fs.readFileSync(__filename)).digest('hex'),
        profile_sha256: crypto.createHash('sha256').update(JSON.stringify(source)).digest('hex'),
        docker: {version: engine.ServerVersion, cpus: engine.NCPU, memory_bytes: engine.MemTotal},
        platform: {os: os.platform(), release: os.release(), cpus: os.cpus().length, memory_bytes: os.totalmem(), node: process.version},
        duration_seconds: duration, sustained_seconds: sustained, think_ms: quick ? 50 : 250, heavy_expression: `${quick ? 1000 : 100000}!`,
        scope: 'Loopback TLS; distinct IPs simulated at the trusted Cloudflare edge header; fresh data. Colocated driver/production/desktop activity remains in host metrics.', phases: rows};
    try {
        compose(['config', '--quiet']);
        compose(['up', '--no-build', '--wait', '--wait-timeout', '180']);
        const caFile = path.join(output, 'gateway-root.crt');
        compose(['cp', 'gateway:/data/caddy/pki/authorities/local/root.crt', caFile]);
        const ca = fs.readFileSync(caFile), base = 'https://localhost:18884';
        const make = ip => {const client = new Client(base, ca, ip); clients.push(client); return client;};
        const password = crypto.randomBytes(24).toString('base64url');
        const maximum = quick ? 12 : 60;
        const seed = `import json,django\ndjango.setup()\nfrom django.contrib.auth import get_user_model\nu=get_user_model()\nusers=[u.objects.create_user(username='load-'+str(i),password=${JSON.stringify(password)}) for i in range(${maximum})]\nprint(json.dumps([x.pk for x in users]))`;
        const userIDs = JSON.parse(docker(['exec', '-i', `${project}-accounts-1`, 'python', '-'], {input: seed}).toString());
        const monitor = {username: 'load-monitor', password: crypto.randomBytes(24).toString('base64url'), monitors: [
            {name: 'Load fixture C engine', type: 'keyword', url: 'http://web:18876/api/evaluate?precision=10', method: 'POST', body: '2+2',
                httpBodyEncoding: 'form', headers: '{"Content-Type":"text/plain"}', keyword: '"result":"4"'},
            {name: 'Load fixture PostgreSQL', type: 'port', hostname: 'db', port: 5432}]};
        docker(['exec', '-i', `${project}-uptime-1`, 'node', '/opt/setup_monitor.cjs'], {input: JSON.stringify(monitor)});
        const dbStats = () => JSON.parse(docker(['exec', `${project}-db-1`, 'psql', '-U', 'nf_local_admin', '-d', 'numforge_accounts', '-At', '-c',
            "SELECT json_build_object('commits',xact_commit,'rollbacks',xact_rollback,'connections',numbackends,'inserted',tup_inserted,'updated',tup_updated,'deleted',tup_deleted) FROM pg_stat_database WHERE datname='numforge_accounts'"]).toString());
        const containers = ['web', 'accounts', 'db', 'gateway', 'uptime'].map(name => `${project}-${name}-1`);
        stats = spawn('docker', ['stats', '--format', '{{json .}}', ...containers], {env: environment, windowsHide: true});
        readline.createInterface({input: stats.stdout}).on('line', line => {
            try {const r = statsLine(line); if (r) {const {name, ...values} = r; resources.push({phase, at: Date.now(), service: name.slice(project.length + 1, -2), ...values});}}
            catch {result.resource_errors = (result.resource_errors || 0) + 1;}
        });
        stats.stderr.on('data', () => {result.resource_errors = (result.resource_errors || 0) + 1;});
        let previousCPU = os.cpus();
        sampleTimer = setInterval(() => {
            const current = os.cpus(); let total = 0, idle = 0;
            current.forEach((cpu, i) => {for (const key of Object.keys(cpu.times)) total += cpu.times[key] - previousCPU[i].times[key]; idle += cpu.times.idle - previousCPU[i].times.idle;});
            previousCPU = current;
            host.push({phase, at: Date.now(), cpu_percent: total ? 100 * (1 - idle / total) : 0, free_bytes: os.freemem(), driver_rss_bytes: process.memoryUsage().rss});
            if (os.freemem() < 2 * 1024 ** 3) {stopped = true; result.memory_guard = true;}
        }, 1000);
        const login = (client, index) => client.exchange('/api/account/login', {username: 'load-' + index, password}, true);
        const prepare = async (client, index) => {const r = await client.exchange('/api/account/session'); session(r, null); return index;};
        phase = 'shared-ip-login';
        const shared = Array.from({length: quick ? 12 : 20}, () => make('198.51.100.254'));
        await Promise.all(shared.map(prepare));
        const started = performance.now(), limited = [];
        await Promise.all(shared.map((client, index) => measured(limited, 'shared_ip_login', () => login(client, index), response => {
            assert.ok([200, 429].includes(response.status));
            if (response.status === 429) assert.ok(Number(response.headers['retry-after']) > 0);
            response.status === 200 ? result.shared_ip_successes = (result.shared_ip_successes || 0) + 1 : result.shared_ip_limited = (result.shared_ip_limited || 0) + 1;
        })));
        assert.equal(result.shared_ip_successes, 10); assert.equal(result.shared_ip_limited, shared.length - 10);
        rows.push({phase, elapsed_ms: performance.now() - started, operations: summary(limited)});
        await Promise.all(shared.map(c => c.exchange('/api/account/logout', {}, true))); shared.forEach(c => c.close());
        const preflight = make('203.0.113.200');
        for (const [expr, precision, expected] of [['2+2', '10', '4'], ['sqrt(2)', '1000', cases.sqrt], [result.heavy_expression, '10', cases.factorial]]) {
            const reply = await preflight.calculate(expr, precision); success(reply); assert.equal(decimal(reply.data.result), decimal(expected));
        }
        preflight.close();
        let actorSerial = 0;
        const scenarios = (quick ? [2] : [10, 30, 60]).flatMap(count => [0, quick ? 1 : 2].map(heavyCount => ({count, heavyCount, seconds: duration})));
        if (sustained) scenarios.push({count: 30, heavyCount: 2, seconds: sustained, sustained: true});
        for (const scenario of scenarios) {
            const {count, heavyCount, seconds} = scenario;
            if (stopped) break;
            phase = `clients-${count}-heavy-${heavyCount}${scenario.sustained ? '-sustained' : ''}`;
            const phaseBegin = performance.now();
            const databaseBefore = dbStats();
            const records = [], actors = Array.from({length: count}, () => make(`192.0.2.${++actorSerial}`));
            await Promise.all(actors.map(client => measured(records, 'csrf_prepare', () => client.exchange('/api/account/session'), r => session(r, null))));
            await Promise.all(actors.map((client, index) => measured(records, 'login', () => login(client, index), r => session(r, userIDs[index]))));
            await Promise.all(actors.map(client => measured(records, 'math_session_start', () => client.calculate('', '10', 'start'), success)));
            const begin = performance.now(), end = begin + seconds * 1000;
            const normal = actors.map(async (client, index) => {
                let value, relogged = false;
                for (let turn = 0; !stopped && performance.now() < end; turn++) {
                    if (!relogged && performance.now() > begin + seconds * 500) {
                        await measured(records, 'logout', () => client.exchange('/api/account/logout', {}, true), r => session(r, null));
                        await measured(records, 'relogin', () => login(client, index), r => session(r, userIDs[index])); relogged = true;
                    }
                    if (turn % 4 === 0) {
                        value = String((index + 1) * 100000 + turn);
                        await measured(records, 'math_commit', () => client.calculate('x=' + value, '10', 'commit'), r => {success(r); assert.equal(decimal(r.data.result), decimal(value));});
                    } else if (turn % 4 === 1) {
                        await measured(records, 'math_session_read', () => client.exchange('/api/session/value?' + new URLSearchParams({client: client.id, name: 'x'})), r => {assert.equal(r.status, 200); assert.equal(r.data.value.text, value);});
                    } else if (turn % 4 === 2) {
                        await measured(records, 'sqrt_1000', () => client.calculate('sqrt(2)', '1000'), r => {success(r); assert.equal(decimal(r.data.result), decimal(cases.sqrt));});
                    } else await measured(records, 'small_calculation', () => client.calculate('2+2'), r => {success(r); assert.equal(decimal(r.data.result), decimal('4'));});
                    await measured(records, 'account_session', () => client.exchange('/api/account/session'), r => session(r, userIDs[index]));
                    await delay(result.think_ms);
                }
            });
            const heavy = Array.from({length: heavyCount}, (_, i) => {
                const client = make(`203.0.113.${i + 1}`);
                return (async () => {while (!stopped && performance.now() < end) await measured(records, 'factorial', () => client.calculate(result.heavy_expression), r => {success(r); assert.equal(decimal(r.data.result), decimal(cases.factorial));}); client.close();})();
            });
            await Promise.all([...normal, ...heavy]);
            const activeElapsed = performance.now() - begin;
            for (const client of actors) {
                const replay = make(client.ip); replay.cookies = {...client.cookies};
                await measured(records, 'final_logout', () => client.exchange('/api/account/logout', {}, true), r => session(r, null));
                await measured(records, 'cookie_replay', () => replay.exchange('/api/account/session'), r => session(r, null));
                replay.close(); client.close();
            }
            const row = {phase, clients: count, heavy_clients: heavyCount, elapsed_ms: performance.now() - phaseBegin, active_elapsed_ms: activeElapsed, operations: summary(records),
                database_before: databaseBefore, database_after: dbStats()};
            if (liveProbes) {
                const url = JSON.parse(fs.readFileSync(path.join(ROOT, '.local/compose/pilot-status.json'))).url;
                assert.match(url, /^https:\/\/[a-z0-9-]+\.trycloudflare\.com$/);
                const live = new Client(url, undefined);
                const probe = []; await measured(probe, 'public_probe', () => live.calculate('2+2'), r => {success(r); assert.equal(decimal(r.data.result), decimal('4'));});
                row.public_probe = summary(probe); live.close();
            }
            rows.push(row); console.log(JSON.stringify(row));
        }
        result.ok = !stopped && resources.length > 0 && !result.resource_errors && rows.every(row => Object.values({...row.operations, ...row.public_probe}).every(op => !Object.keys(op.errors).length));
    } finally {
        if (sampleTimer) clearInterval(sampleTimer);
        if (stats) stats.kill();
        clients.forEach(client => client.close());
        fs.writeFileSync(path.join(output, 'results.json'), JSON.stringify(result, null, 2));
        fs.writeFileSync(path.join(output, 'containers.json'), JSON.stringify(resources));
        fs.writeFileSync(path.join(output, 'host.json'), JSON.stringify(host));
        compose(['down', '--volumes', '--remove-orphans']);
    }
    console.log(JSON.stringify({output, ok: result.ok, resource_samples: resources.length, host_samples: host.length}));
    if (!result.ok) process.exitCode = 1;
}

module.exports = {isolate, decimal, bytes, statsLine, summary};
if (require.main === module) main(process.argv.slice(2)).catch(error => {console.error(error.stack); process.exitCode = 1;});
