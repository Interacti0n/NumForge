// Run through check_monitor.py; no notification is sent to an external service.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const {spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');
const appRequire = createRequire('/app/package.json');
const manifestFile = '/opt/numforge/monitor-patches.json';
const manifest = fs.existsSync(manifestFile) ? JSON.parse(fs.readFileSync(manifestFile, 'utf8')) : {};
if (manifest.directMigrations?.some(p => p.name === 'liquidjs') || process.env.NUMFORGE_FORCE_LIQUID_CHECKS) {
    const {Liquid} = appRequire('liquidjs');
    const NotificationProvider = appRequire('./server/notification-providers/notification-provider');
    const provider = new NotificationProvider();

    test('Liquid uses the exact qualified security version', () => {
        assert.equal(appRequire('liquidjs/package.json').version, '10.27.2');
    });

    test('real notification templates preserve context, Unicode and legacy aliases', async () => {
        const template = '{{ status }}|{{ name }}|{{ hostnameOrURL }}|{{ msg }}|{{ STATUS }}|{{ NAME }}|{{ HOSTNAME_OR_URL }}|{{ monitorJSON.id }}|{{ heartbeatJSON.status }}';
        const monitor = {id: 7, type: 'keyword', name: 'Výpočet', url: 'https://example.invalid/health'};
        assert.equal(await provider.renderTemplate(template, 'Všetko funguje', monitor, {status: 1}),
            '✅ Up|Výpočet|https://example.invalid/health|Všetko funguje|✅ Up|Výpočet|https://example.invalid/health|7|1');
        assert.equal(await provider.renderTemplate('{{ status }}:{{ hostnameOrURL }}', 'x', {...monitor, type: 'port', hostname: '127.0.0.1', port: 443}, {status: 0}), '🔴 Down:127.0.0.1:443');
        assert.equal(await provider.renderTemplate('{{ name }}|{{ hostnameOrURL }}|{{ status }}', 'x', null, null),
            'Monitor Name not available|testing.hostname|⚠️ Test');
    });

    test('notification strip_html terminates for unfinished tags and raw-text blocks', () => {
        // A separate bounded process contains the old infinite loop. The heap cap
        // also bounds that old implementation's repeated string accumulation.
        const program = `const assert=require('node:assert/strict');const Provider=require('/app/server/notification-providers/notification-provider');
        (async()=>{for(const [input,expected] of [['a<','a<'],['a<script','a<script'],['a<!--','a<!--'],['a<style','a<style'],['<b>ok</b>','ok'],['safe<script>bad</script>end','safeend']])
            assert.equal(await new Provider().renderTemplate('{{ msg | strip_html }}',input,null,null),expected);})().catch(e=>{console.error(e);process.exitCode=1;});`;
        const result = spawnSync(process.execPath, ['--max-old-space-size=32', '-e', program], {timeout: 2000, encoding: 'utf8', maxBuffer: 65536});
        assert.equal(result.error, undefined, 'Unfinished HTML must terminate within the test deadline');
        assert.equal(result.status, 0, 'HTML filter child must exit successfully: ' + result.stderr.slice(-1000));
    });

    test('pop charges array copies to configured memoryLimit', async () => {
        const engine = new Liquid({memoryLimit: 10});
        await assert.rejects(engine.parseAndRender('{% assign shortened = values | pop %}ok', {values: Array(20).fill('a')}), /memory alloc limit exceeded/);
        assert.equal(await engine.parseAndRender('{{ values | pop | join: "," }}', {values: ['a', 'b', 'c']}), 'a,b');
    });

    test('join charges produced text including separators before materializing output', async () => {
        const engine = new Liquid({memoryLimit: 100});
        const template = '{% assign joined = values | join: separator %}ok';
        await assert.rejects(engine.parseAndRender(template, {values: ['a'.repeat(60), 'b'.repeat(60)], separator: ''}), /memory alloc limit exceeded/);
        await assert.rejects(engine.parseAndRender(template, {values: ['a', 'b', 'c'], separator: '-'.repeat(60)}), /memory alloc limit exceeded/);
        assert.equal(await engine.parseAndRender('{{ values | join: ":" }}', {values: ['a', 'b']}), 'a:b');
    });

    test('ownPropertyOnly protects direct access; inherited array-filter limitation stays explicit', async () => {
        const values = Array(1);
        const prototype = Object.create(Array.prototype);
        prototype[0] = 'PRIVATE_INHERITED_VALUE';
        Object.setPrototypeOf(values, prototype);
        const engine = new Liquid({ownPropertyOnly: true});
        for (const expression of ['values[0]', 'values[-1]', 'values.first', 'values.last', 'values | first', 'values | last']) {
            assert.equal(await engine.parseAndRender('{{ ' + expression + ' }}', {values}), '', expression);
        }
        // The published Node build still reads inherited elements via these
        // filters/loops (also reproduced in 10.30.1). It is not an isolation
        // boundary for prototype-polluted JavaScript arrays. Keep that limit
        // visible rather than claiming the entire advisory class is closed.
        for (const expression of ['values | join: ","', 'values | reverse | first',
            'values | slice: 0, 1 | join: ","', 'values | compact | join: ","']) {
            assert.equal(await engine.parseAndRender('{{ ' + expression + ' }}', {values}), 'PRIVATE_INHERITED_VALUE', expression);
        }
        assert.equal(await engine.parseAndRender('{% for value in values %}[{{ value }}]{% endfor %}', {values}), '[PRIVATE_INHERITED_VALUE]');
        assert.equal(await engine.parseAndRender('{{ values | join: "," }}', {values: ['own', 'data']}), 'own,data');
    });

    test('notification templates retain filter behavior and reject bad syntax or file includes', async () => {
        assert.equal(await provider.renderTemplate('{% if heartbeatJSON.status == 1 %}{{ name | upcase }}:{{ msg | escape }}{% endif %}', '<ok>', {name: 'pilot', type: 'push'}, {status: 1}), 'PILOT:&lt;ok&gt;');
        await assert.rejects(provider.renderTemplate('{% if true %}', 'x', null, null), /not closed/);
        await assert.rejects(provider.renderTemplate('{% include "package.json" %}', 'x', null, null), /ENOENT|failed to lookup/);
    });
} else {
    console.log('Original Liquid range: migration regressions deferred to the qualified candidate.');
}
