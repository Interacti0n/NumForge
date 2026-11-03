// All identities/tokens are dummy fixtures; network clients reject unknown URLs.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {createRequire} = require('node:module');
const appRequire = createRequire('/app/package.json');
const guards = globalThis.monitorPatchGuards;
const receipt = '/opt/numforge/monitor-patches.json';
const manifest = fs.existsSync(receipt) ? JSON.parse(fs.readFileSync(receipt, 'utf8')) : {};
const write = (file, value) => {fs.mkdirSync(path.dirname(file), {recursive: true}); fs.writeFileSync(file, JSON.stringify(value));};

function fixture(t) {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'nf-msal-guard-')), packages = {}, snapshot = path.join(root, 'before.json');
    t.after(() => fs.rmSync(root, {recursive: true}));
    function pkg(location, name, version, integrity, dependencies) {
        write(path.join(root, location, 'package.json'), {name, version, ...(dependencies ? {dependencies} : {})});
        packages[location] = {version, integrity, resolved: `https://registry.npmjs.org/${name}/-/${name}-${version}.tgz`};
    }
    function lock() {write(path.join(root, 'package-lock.json'), {packages}); write(path.join(root, 'node_modules/.package-lock.json'), {packages});}
    write(path.join(root, 'package.json'), {version: '2.5.9', dependencies: {identity: '^4.13.1'}});
    pkg('node_modules/identity', 'identity', '4.13.1', 'owner', {msal: '^5.1.0'});
    pkg('node_modules/msal', 'msal', '5.6.0', 'old', {common: '16.13.0'});
    pkg('node_modules/msal/node_modules/common', 'common', '16.13.0', 'old-common');
    pkg('node_modules/common', 'common', '16.14.2', 'browser-common');
    fs.symlinkSync('/app/node_modules/semver', path.join(root, 'node_modules/semver')); lock();
    const migration = {consumer: 'node_modules/identity', consumerVersion: '4.13.1', consumerIntegrity: 'owner', name: 'msal',
        fromRange: '^5.1.0', version: '7.1.2', compatibility: 'consumer-major-upgrade',
        dependencyMigration: {from: {common: '16.13.0'}, to: {common: '16.15.0'}}};
    const config = {upstreamVersion: '2.5.9', keepAbsent: [], overrides: {msal: {'.': '7.1.2', common: '16.15.0'}},
        consumerMigrations: [migration], packages: [{name: 'msal', from: '5.6.0', version: '7.1.2', before: 'node_modules/msal', after: 'node_modules/msal', integrity: 'new'}],
        addedDependencies: [{name: 'common', path: 'node_modules/msal/node_modules/common', version: '16.15.0', integrity: 'new-common', addedBy: 'msal', replaces: {version: '16.13.0', integrity: 'old-common'}}]};
    function install() {pkg('node_modules/msal', 'msal', '7.1.2', 'new', {common: '16.15.0'}); pkg('node_modules/msal/node_modules/common', 'common', '16.15.0', 'new-common'); lock();}
    return {root, config, migration, snapshot, pkg, lock, install};
}
if (guards && fs.existsSync(receipt)) {
    test('consumer-only major upgrade reifies its owned helper and preserves the shared branch', t => {
        const f = fixture(t); guards.prepare(f.root, f.config, f.snapshot);
        assert(fs.existsSync(path.join(f.root, 'node_modules/msal/node_modules/common/package.json')));
        f.install(); guards.verify(f.root, f.config, f.snapshot);
        assert.equal(JSON.parse(fs.readFileSync(path.join(f.root, 'node_modules/common/package.json'))).version, '16.14.2');
        assert.equal(JSON.parse(fs.readFileSync(path.join(f.root, 'package.json'))).dependencies.msal, undefined);
    });
    test('consumer-only major policy requires explicit old and new dependency maps', t => {
        const f = fixture(t); delete f.migration.dependencyMigration;
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /requires explicit dependency maps/);
    });
    test('consumer major migration rejects an unexpected upstream dependency map before writing', t => {
        const f = fixture(t); f.migration.dependencyMigration.from.common = '16.12.0';
        const original = fs.readFileSync(path.join(f.root, 'package.json'));
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /Upstream migrated dependencies changed/);
        assert.deepEqual(fs.readFileSync(path.join(f.root, 'package.json')), original);
    });
    test('consumer major migration rejects a target dependency map outside its manifest', t => {
        const f = fixture(t); guards.prepare(f.root, f.config, f.snapshot); f.install();
        f.pkg('node_modules/msal', 'msal', '7.1.2', 'new', {common: '^16.15.0'}); f.lock();
        assert.throws(() => guards.verify(f.root, f.config, f.snapshot), /Migrated target dependencies changed/);
    });
    for (const version of ['5.7.0', '4.0.0', '7.2.0-beta.1']) {
        test('consumer-only major policy rejects unqualified version ' + version, t => {
            const f = fixture(t); f.migration.version = f.config.packages[0].version = version;
            assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /forward between stable majors/);
        });
    }
    test('consumer-only major policy cannot bypass a separate consumer range', t => {
        const f = fixture(t); f.pkg('node_modules/other', 'other', '1.0.0', 'other', {msal: '^5.1.0'}); f.lock();
        assert.throws(() => guards.prepare(f.root, f.config, f.snapshot), /Unsupported range/);
    });
    for (const phase of ['upstream', 'target']) {
        test('consumer-only major policy rejects unexpected ' + phase + ' optional dependencies', t => {
            const f = fixture(t);
            if (phase === 'target') {guards.prepare(f.root, f.config, f.snapshot); f.install();}
            const file = path.join(f.root, 'node_modules/msal/package.json'), value = JSON.parse(fs.readFileSync(file));
            value.optionalDependencies = {common: '16.15.0'}; write(file, value);
            assert.throws(() => phase === 'upstream' ? guards.prepare(f.root, f.config, f.snapshot) : guards.verify(f.root, f.config, f.snapshot), /optional dependencies/);
        });
    }
}

if (manifest.consumerMigrations?.some(p => p.name === '@azure/msal-node') || process.env.NUMFORGE_FORCE_MSAL_CHECKS) {
    const msal = appRequire('@azure/msal-node');
    const tenant = '11111111-1111-4111-8111-111111111111', client = '22222222-2222-4222-8222-222222222222';
    const authority = 'https://login.microsoftonline.com/' + tenant;
    const scope = 'https://database.windows.net/.default';
    const endpoint = authority + '/oauth2/v2.0/token';
    const metadata = {authorization_endpoint: authority + '/oauth2/v2.0/authorize', token_endpoint: endpoint,
        issuer: authority + '/v2.0', jwks_uri: authority + '/discovery/v2.0/keys', end_session_endpoint: authority + '/oauth2/v2.0/logout'};
    function response() {
        const encode = value => Buffer.from(JSON.stringify(value)).toString('base64url');
        const now = Math.floor(Date.now() / 1000);
        const claims = {aud: client, iss: authority + '/v2.0', tid: tenant, oid: '33333333-3333-4333-8333-333333333333',
            sub: 'fixture', preferred_username: 'fixture@example.test', nonce: 'EXPECTED_NONCE', iat: now, exp: now + 3600};
        return {access_token: 'DUMMY_ACCESS_TOKEN', token_type: 'Bearer', expires_in: 3600, scope,
            id_token: encode({alg: 'RS256', typ: 'JWT'}) + '.' + encode(claims) + '.DUMMY_SIGNATURE',
            client_info: encode({uid: claims.oid, utid: tenant})};
    }
    function network() {
        const calls = [];
        function send(method, url, options = {}) {
            assert.equal(new URL(url).hostname, 'login.microsoftonline.com', 'Untrusted endpoint: ' + url);
            calls.push({method, url, options}); assert(calls.length < 12, 'Fixture request bound');
            if (/openid-configuration/.test(url)) return {headers: {}, status: 200, body: metadata};
            if (/\/discovery\/instance/.test(url)) return {headers: {}, status: 200, body: {tenant_discovery_endpoint: authority + '/v2.0/.well-known/openid-configuration',
                metadata: [{preferred_network: 'login.microsoftonline.com', preferred_cache: 'login.microsoftonline.com', aliases: ['login.microsoftonline.com']}]}};
            if (/userrealm/.test(url)) return {headers: {}, status: 200, body: {NameSpaceType: 'Managed', cloud_instance_name: 'microsoftonline.com'}};
            const target = new URL(url);
            assert.equal(target.origin + target.pathname, endpoint);
            const body = response();
            if (new URLSearchParams(options.body).get('grant_type') === 'client_credentials') {
                delete body.id_token; delete body.client_info;
            }
            return {headers: {}, status: 200, body};
        }
        return {calls, send, sendGetRequestAsync: async (url, options) => send('GET', url, options),
            sendPostRequestAsync: async (url, options) => send('POST', url, options)};
    }
    function configuration(net = network(), auth = {}) {
        return {auth: {clientId: client, authority, clientSecret: 'DUMMY_SECRET', knownAuthorities: ['login.microsoftonline.com'],
            authorityMetadata: JSON.stringify(metadata), ...auth}, system: {networkClient: net}};
    }
    const request = () => ({code: 'ORIGINAL_CODE', redirectUri: 'https://app.example.test/callback', scopes: [scope],
        nonce: 'EXPECTED_NONCE', state: 'EXPECTED_STATE', codeVerifier: 'FIXTURE_PKCE_VERIFIER'});

    test('MSAL Node and its owned common pin preserve Identity and browser common versions', () => {
        const nodeRequire = createRequire(appRequire.resolve('@azure/msal-node/package.json'));
        const browserRequire = createRequire(appRequire.resolve('@azure/msal-browser/package.json'));
        const identityRequire = createRequire(appRequire.resolve('@azure/identity/package.json'));
        assert.equal(appRequire('@azure/msal-node/package.json').version, '7.1.2');
        assert.equal(nodeRequire('@azure/msal-common/package.json').version, '16.15.0');
        assert.equal(browserRequire('@azure/msal-common/package.json').version, '16.14.2');
        assert.equal(appRequire('@azure/identity/package.json').version, '4.13.1');
        assert.equal(identityRequire.resolve('@azure/msal-node'), appRequire.resolve('@azure/msal-node'));
    });
    test('confidential client rejects unsupported instance-aware configuration', () => {
        assert.throws(() => new msal.ConfidentialClientApplication(configuration(network(), {instanceAware: true})),
            error => error.errorCode === 'unsupported_cca_instance_aware');
    });
    for (const value of ['true', 'false']) {
        test('confidential auth URL rejects response-routing option instance_aware=' + value, async () => {
            const net = network(), app = new msal.ConfidentialClientApplication(configuration(net));
            await assert.rejects(app.getAuthCodeUrl({...request(), extraQueryParameters: {instance_aware: value}}),
                error => error.errorCode === 'unsupported_cca_instance_aware');
            assert.equal(net.calls.length, 0);
        });
    }
    test('authorization response cannot forward untrusted routing or metadata into token acquisition', async t => {
        const app = new msal.ConfidentialClientApplication(configuration());
        let forwarded;
        const base = Object.getPrototypeOf(msal.ConfidentialClientApplication.prototype);
        t.mock.method(base, 'acquireTokenByCode', async (input, payload) => {forwarded = payload; return {accessToken: 'DUMMY'};});
        const payload = {code: 'INJECTED', nonce: 'INJECTED', state: 'OBSERVED_STATE', cloud_instance_host_name: 'untrusted.example.test',
            cloud_instance_name: 'untrusted', client_info: 'INJECTED', error: 'INJECTED'};
        const original = structuredClone(payload);
        await app.acquireTokenByCode(request(), payload);
        assert.deepEqual(forwarded, {code: 'ORIGINAL_CODE', nonce: 'EXPECTED_NONCE', state: 'OBSERVED_STATE'});
        assert.deepEqual(payload, original);
    });
    test('ordinary authorization URL preserves state, nonce and PKCE at the trusted authority', async () => {
        const app = new msal.ConfidentialClientApplication(configuration());
        const url = new URL(await app.getAuthCodeUrl({...request(), codeChallenge: 'FIXTURE_CHALLENGE', codeChallengeMethod: 'S256'}));
        assert.equal(url.hostname, 'login.microsoftonline.com'); assert.equal(url.searchParams.get('state'), 'EXPECTED_STATE');
        assert.equal(url.searchParams.get('nonce'), 'EXPECTED_NONCE'); assert.equal(url.searchParams.get('code_challenge'), 'FIXTURE_CHALLENGE');
    });
    test('auth code redemption keeps trusted code and nonce despite response-controlled routing fields', async () => {
        const net = network(), app = new msal.ConfidentialClientApplication(configuration(net));
        const result = await app.acquireTokenByCode(request(), {state: 'EXPECTED_STATE', code: 'INJECTED', nonce: 'INJECTED',
            cloud_instance_host_name: 'untrusted.example.test', cloud_instance_name: 'untrusted'});
        assert.equal(result.accessToken, 'DUMMY_ACCESS_TOKEN');
        const posts = net.calls.filter(call => call.method === 'POST'); assert.equal(posts.length, 1);
        const form = new URLSearchParams(posts[0].options.body);
        assert.equal(form.get('code'), 'ORIGINAL_CODE'); assert.equal(form.get('code_verifier'), 'FIXTURE_PKCE_VERIFIER');
        const target = new URL(posts[0].url); assert.equal(target.origin + target.pathname, endpoint);
    });
    test('mismatched response state fails before any token request', async () => {
        const net = network(), app = new msal.ConfidentialClientApplication(configuration(net));
        await assert.rejects(app.acquireTokenByCode(request(), {code: 'ORIGINAL_CODE', state: 'WRONG_STATE'}), /state/i);
        assert.equal(net.calls.length, 0);
    });
    test('client credential acquisition and repeated-request cache remain usable', async () => {
        const net = network(), app = new msal.ConfidentialClientApplication(configuration(net));
        const first = await app.acquireTokenByClientCredential({scopes: [scope]});
        const second = await app.acquireTokenByClientCredential({scopes: [scope]});
        assert.equal(first.accessToken, 'DUMMY_ACCESS_TOKEN'); assert.equal(second.accessToken, first.accessToken);
        assert.equal(second.fromCache, true); assert.equal(net.calls.filter(call => call.method === 'POST').length, 1);
    });
    async function identityToken(kind) {
        const identity = appRequire('@azure/identity'), net = network();
        const {createHttpHeaders} = appRequire('@azure/core-rest-pipeline');
        const httpClient = {sendRequest: async request => {
            const result = net.send(request.method, request.url, {body: request.body});
            return {request, status: result.status, headers: createHttpHeaders({'content-type': 'application/json'}), bodyAsText: JSON.stringify(result.body)};
        }};
        const options = {httpClient, disableInstanceDiscovery: true, retryOptions: {maxRetries: 0}};
        const credential = kind === 'secret' ? new identity.ClientSecretCredential(tenant, client, 'DUMMY_SECRET', options) :
            new identity.UsernamePasswordCredential(tenant, client, 'fixture@example.test', 'DUMMY_PASSWORD', options);
        const token = await credential.getToken(scope);
        assert.equal(token.token, 'DUMMY_ACCESS_TOKEN'); assert(token.expiresOnTimestamp > Date.now());
        const posts = net.calls.filter(call => call.method === 'POST'); assert.equal(posts.length, 1);
        const form = new URLSearchParams(posts[0].options.body);
        assert.equal(form.get('grant_type'), kind === 'secret' ? 'client_credentials' : 'password');
        assert.equal(form.get(kind === 'secret' ? 'client_secret' : 'password'), kind === 'secret' ? 'DUMMY_SECRET' : 'DUMMY_PASSWORD');
    }
    test('actual Azure Identity ClientSecretCredential preserves SQL token acquisition', () => identityToken('secret'));
    test('actual Azure Identity UsernamePasswordCredential preserves SQL password flow', () => identityToken('password'));
} else {
    console.log('Original MSAL consumer: migration checks deferred to the qualified candidate.');
}
