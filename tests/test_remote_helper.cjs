'use strict';
// Windows-only lifecycle checks. The tunnel fixture never contacts Cloudflare.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const net = require('node:net');
const http = require('node:http');
const {execFile, spawn} = require('node:child_process');
const {promisify} = require('node:util');
const runFile = promisify(execFile);
const [server, fixture, powershell] = process.argv.slice(2);
const root = path.resolve(__dirname, '..'), helper = path.join(root, 'scripts/remote.ps1');
const scratch = fs.mkdtempSync(path.join(root, 'build/remote helper-'));
const candidate = path.join(scratch, 'current server.exe'), next = path.join(scratch, 'next server.exe');
const tunnel = path.join(scratch, 'test tunnel.exe');
for (const [source, target] of [[server, candidate], [server, next], [fixture, tunnel]]) fs.copyFileSync(source, target);
let port, statePath, invocation = 0;
async function call(action, extra = []) {
    // Redirect to files: detached Windows processes can inherit a pipe handle
    // after PowerShell exits, which would delay execFile's close callback.
    const stem = path.join(scratch, String(++invocation));
    const out = fs.openSync(stem + '.out', 'w'), err = fs.openSync(stem + '.err', 'w');
    const child = spawn(powershell, ['-NoProfile', '-NonInteractive', '-File', helper, '-Action', action,
        '-Port', String(port), ...extra], {cwd: root, windowsHide: true, stdio: ['ignore', out, err]});
    fs.closeSync(out); fs.closeSync(err);
    const code = await new Promise((resolve, reject) => {
        const timer = setTimeout(() => {child.kill(); reject(Error('Helper action timeout'));}, 45000);
        child.once('error', error => {clearTimeout(timer); reject(error);});
        child.once('exit', code => {clearTimeout(timer); resolve(code);});
    });
    const result = {stdout: fs.readFileSync(stem + '.out', 'utf8'), stderr: fs.readFileSync(stem + '.err', 'utf8')};
    if (code !== 0) throw Object.assign(Error(`${action}: ${result.stderr}`), result);
    return result;
}
const state = () => JSON.parse(fs.readFileSync(statePath, 'utf8').replace(/^\uFEFF/, ''));
const status = async () => JSON.parse((await call('Status', ['-Json'])).stdout.replace(/^\uFEFF/, ''));
const writeState = value => fs.writeFileSync(statePath, JSON.stringify(value));
async function reject(action, extra, pattern) {
    await assert.rejects(call(action, extra), error => {assert.match(error.stderr, pattern); return true;});
}
function request(target, body = null) {
    return new Promise((resolve, reject) => {
        const req = http.request({hostname: '127.0.0.1', port, path: target,
            method: body === null ? 'GET' : 'POST', agent: false,
            headers: body === null ? {} : {'Content-Type': 'text/plain', 'Content-Length': Buffer.byteLength(body)}}, res => {
            let data = ''; res.setEncoding('utf8'); res.on('data', text => {data += text;});
            res.on('end', () => {try {resolve({status: res.statusCode, data: JSON.parse(data)});}
                catch {reject(Error(`${target}: HTTP ${res.statusCode}: ${data}`));}});
            res.on('error', reject);
        });
        req.setTimeout(3000, () => req.destroy(Error('HTTP timeout'))); req.on('error', reject); req.end(body);
    });
}
const canonicalTime = value => value.replace(/(\.\d*?)0+Z$/, '$1Z').replace('.Z', 'Z');
function sameIdentity(actual, expected) {
    assert.deepEqual({...actual, StartTime: canonicalTime(actual.StartTime)},
        {...expected, StartTime: canonicalTime(expected.StartTime)});
}
const live = id => {try {process.kill(id, 0); return true;} catch {return false;}};
(async () => {
    const reservation = net.createServer();
    await new Promise(resolve => reservation.listen(0, '127.0.0.1', resolve));
    port = reservation.address().port;
    await new Promise(resolve => reservation.close(resolve));
    statePath = path.join(root, `build/remote-state/state-${port}.json`);
    assert.equal(fs.existsSync(statePath), false, 'Dedicated port has no saved instance');
    try {
        assert.equal((await status()).State, 'NotRecorded');
        await reject('Start', ['-Workers', '0'], /validation|ValidateRange|range/i);
        await reject('Start', ['-QueueCapacity', '65'], /validation|ValidateRange|range/i);
        await reject('Start', ['-ServerPath', path.join(scratch, 'missing.exe')], /does not exist|Cannot find path/i);
        const foreign = net.createServer(socket => socket.destroy());
        await new Promise(resolve => foreign.listen(port, '127.0.0.1', resolve));
        try {
            await reject('Start', ['-ServerPath', candidate, '-CloudflaredPath', tunnel], /address|socket|usage/i);
            assert.equal(foreign.listening, true);
        } finally {await new Promise(resolve => foreign.close(resolve));}
        await reject('Start', ['-ServerPath', tunnel, '-CloudflaredPath', tunnel], /exited during startup/i);
        assert.equal(fs.existsSync(statePath), false, 'Failed start leaves no ready record');
        await call('Start', ['-ServerPath', candidate, '-CloudflaredPath', tunnel, '-Workers', '2', '-QueueCapacity', '8']);
        let saved = state();
        const ready = await status(); assert.equal(ready.State, 'Ready', JSON.stringify({ready, saved}));
        assert.equal(saved.Workers, 2); assert.equal(saved.QueueCapacity, 8);
        const tunnelIdentity = {...saved.Tunnel}, url = saved.Url;
        await reject('Start', [], /still exist/i);
        writeState({...saved, Url: 'https://wrong-origin.trycloudflare.com'});
        try {assert.equal((await status()).LocalHealthy, false, 'Live PIDs alone do not prove correct serving');}
        finally {writeState(saved);}

        // Check both revisions and state-loss behavior through the real HTTP API.
        const client = 'b'.repeat(32), endpoint = `/api/evaluate?precision=full&angle=rad&client=${client}&revision=1&action=`;
        assert.equal((await request(endpoint + 'start', '')).data.ok, true);
        assert.equal((await request(endpoint + 'commit', 'x=7')).data.result, '7');
        await reject('Restart', ['-ServerPath', path.join(scratch, 'missing.exe')], /does not exist|Cannot find path/i);
        assert.equal(state().Server.Id, saved.Server.Id, 'Missing candidate leaves backend untouched');
        await call('Restart', ['-ServerPath', next]);
        saved = state();
        assert.equal(saved.Server.Path.toLowerCase(), next.toLowerCase());
        assert.equal(saved.Workers, 2); assert.equal(saved.QueueCapacity, 8);
        assert.equal((await request(`/api/session/value?client=${client}&name=x`)).status, 400);
        sameIdentity(saved.Tunnel, tunnelIdentity); assert.equal(saved.Url, url);
        await reject('Restart', ['-ServerPath', tunnel, '-Workers', '3', '-QueueCapacity', '16'], /previous backend restored/i);
        assert.match(fs.readFileSync(path.join(root, `build/remote-state/server-${port}.failure.log`), 'utf8'), /Candidate:/);
        saved = state();
        const restored = await status(); assert.equal(restored.State, 'Ready', JSON.stringify({restored, saved}));
        assert.equal(restored.ServerPriority, 'BelowNormal');
        assert.ok(restored.ServerResidentBytes > 0 && restored.ServerPrivateBytes > 0 && restored.ServerCpuSeconds >= 0);
        assert.equal(saved.Server.Path.toLowerCase(), next.toLowerCase());
        assert.equal(saved.Workers, 2); assert.equal(saved.QueueCapacity, 8);
        sameIdentity(saved.Tunnel, tunnelIdentity);

        // Simulate a crashed backend, then recover without recreating the tunnel.
        process.kill(saved.Server.Id);
        assert.equal((await status()).State, 'Incomplete');
        await call('Restart', ['-Workers', '3', '-QueueCapacity', '16']);
        saved = state();
        assert.equal((await status()).State, 'Ready'); assert.equal(saved.Workers, 3);
        assert.equal(saved.QueueCapacity, 16); sameIdentity(saved.Tunnel, tunnelIdentity);
        writeState({Port: port, Url: url, Processes: [saved.Server, saved.Tunnel]});
        assert.equal((await status()).State, 'Ready', 'Old state files remain readable');
        await call('Restart', ['-ServerPath', candidate]);
        saved = state(); assert.equal(saved.SchemaVersion, 2); assert.equal(saved.Workers, 4);
        assert.equal(saved.QueueCapacity, 64); sameIdentity(saved.Tunnel, tunnelIdentity);

        // Supervision is idempotent and follows a manually selected executable/settings.
        await call('Ensure', ['-ServerPath', next, '-CloudflaredPath', tunnel, '-Workers', '1']);
        assert.equal(state().Server.Id, saved.Server.Id);
        process.kill(saved.Server.Id);
        await call('Ensure', ['-ServerPath', next, '-CloudflaredPath', tunnel, '-Workers', '1']);
        saved = state(); assert.equal((await status()).State, 'Ready');
        assert.equal(saved.Server.Path.toLowerCase(), candidate.toLowerCase());
        assert.equal(saved.Workers, 4); assert.equal(saved.QueueCapacity, 64);
        sameIdentity(saved.Tunnel, tunnelIdentity);

        // Hold the same lock independently; a second action cannot change state.
        const holder = spawn(powershell, ['-NoProfile', '-NonInteractive', '-Command',
            `$l=[IO.File]::Open('${statePath.replaceAll("'", "''")}.lock','OpenOrCreate','ReadWrite','None'); [Console]::WriteLine('locked'); Start-Sleep -Seconds 20`],
            {windowsHide: true, stdio: ['ignore', 'pipe', 'pipe']});
        try {
            await new Promise((resolve, reject) => {
                const timer = setTimeout(() => reject(Error('Lock fixture timeout')), 5000);
                holder.once('error', error => {clearTimeout(timer); reject(error);});
                holder.stdout.once('data', () => {clearTimeout(timer); resolve();});
            });
            await reject('Restart', [], /Another remote action/i);
            await reject('Ensure', [], /Another remote action/i);
            await call('Ensure', ['-Scheduled']);
            assert.equal(state().Server.Id, saved.Server.Id);
        } finally {
            const exited = new Promise(resolve => holder.once('exit', resolve)); holder.kill(); await exited;
        }

        // Stop must not trust a PID whose path or creation timestamp differs.
        for (const forged of [{...saved.Server, StartTime: '2000-01-01T00:00:00Z'}, {...saved.Server, Path: tunnel}]) {
            writeState({...saved, Server: forged, Tunnel: null});
            try {
                assert.equal((await status()).ServerAlive, false);
                await call('Stop');
                assert.equal(live(saved.Server.Id), true); assert.equal(live(saved.Tunnel.Id), true);
            } finally {writeState(saved);}
        }
        process.kill(saved.Server.Id);
        assert.equal((await status()).ServerAlive, false);
        fs.renameSync(candidate, candidate + '.disabled');
        await reject('Restart', ['-ServerPath', tunnel], /rollback failed; tunnel retained/i);
        assert.equal(state().Server, null);
        sameIdentity(state().Tunnel, tunnelIdentity);
        fs.renameSync(candidate + '.disabled', candidate);
        await call('Restart', ['-ServerPath', candidate]);
        saved = state(); assert.equal((await status()).State, 'Ready');
        process.kill(saved.Tunnel.Id);
        assert.equal((await status()).TunnelAlive, false);
        await reject('Restart', [], /No live recorded tunnel/i);
        assert.equal(state().Server.Id, saved.Server.Id);
        await call('Ensure', ['-ServerPath', candidate, '-CloudflaredPath', tunnel]);
        const renewed = state(); assert.equal((await status()).State, 'Ready');
        assert.notEqual(renewed.Tunnel.Id, saved.Tunnel.Id); assert.notEqual(renewed.Url, saved.Url);
        assert.equal(live(saved.Server.Id), false, 'Old-origin backend was replaced');
        saved = renewed;
        await call('Stop');
        assert.equal(live(saved.Server.Id), false); assert.equal(live(saved.Tunnel.Id), false);
        assert.equal((await status()).State, 'NotRecorded');
        await call('Stop');
        await call('Ensure', ['-ServerPath', candidate, '-CloudflaredPath', tunnel, '-Workers', '2', '-QueueCapacity', '8']);
        assert.equal((await status()).State, 'Ready', 'A lost state file can bootstrap from task arguments');
        await call('Stop');
        const startup = await runFile(powershell, ['-NoProfile', '-NonInteractive', '-File',
            path.join(root, 'tests/test_remote_startup.ps1'), '-ServerPath', candidate, '-TunnelPath', tunnel, '-Port', String(port)],
            {windowsHide: true, timeout: 30000});
        console.log(startup.stdout.trim());
        console.log('Remote lifecycle, supervision, state, locking and ownership checks passed.');
    } finally {
        try {if (statePath && fs.existsSync(statePath)) await call('Stop');}
        finally {
            // Detect leaks, but also clean up this test's exact executable paths
            // if a broken helper left an invalid identity record behind.
            const prefix = (scratch + path.sep).replaceAll("'", "''");
            const leaked = await runFile(powershell, ['-NoProfile', '-NonInteractive', '-Command',
                `$ErrorActionPreference='Stop'; Get-Process -Name 'current server','next server','test tunnel' -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith('${prefix}',[StringComparison]::OrdinalIgnoreCase) } | ForEach-Object { $_.Id; Stop-Process -InputObject $_ -Force; [void]$_.WaitForExit(8000) }; exit 0`],
                {windowsHide: true, timeout: 15000});
            if (statePath) fs.rmSync(statePath + '.lock', {force: true});
            assert.equal(leaked.stdout.trim(), '', 'Helper must clean up all its recorded test processes');
        }
    }
})().catch(error => {console.error(error); process.exitCode = 1;}).finally(() => {
    // This test owns the fresh scratch directory; never remove another build tree.
    assert.equal(path.dirname(scratch), path.join(root, 'build'));
    fs.rmSync(scratch, {recursive: true, force: true});
});
