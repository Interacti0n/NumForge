// Dummy messages, temporary files and container-loopback servers only.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const net = require('node:net');
const tls = require('node:tls');
const http = require('node:http');
const https = require('node:https');
const crypto = require('node:crypto');
const {once} = require('node:events');
const {spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');
const appRequire = createRequire('/app/package.json');
const receipt = '/opt/numforge/monitor-patches.json';
const manifest = fs.existsSync(receipt) ? JSON.parse(fs.readFileSync(receipt, 'utf8')) : {};
if (manifest.directMigrations?.some(p => p.name === 'nodemailer') || process.env.NUMFORGE_FORCE_NODEMAILER_CHECKS) {
    const mailer = appRequire('nodemailer');
    const parse = appRequire('nodemailer/lib/addressparser');
    const SMTP = appRequire('./server/notification-providers/smtp.js');
    const message = {from: 'sender@example.test', to: 'recipient@example.test', subject: 'fixture', text: 'fixture'};

    function directory(t) {
        const root = fs.mkdtempSync(path.join(os.tmpdir(), 'nf-mail-'));
        t.after(() => fs.rmSync(root, {recursive: true}));
        return root;
    }
    function certificate(t) {
        const root = directory(t);
        const key = path.join(root, 'key.pem'), cert = path.join(root, 'cert.pem');
        const result = spawnSync('openssl', ['req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
            '-subj', '/CN=localhost', '-addext', 'subjectAltName=DNS:localhost,IP:127.0.0.1', '-keyout', key, '-out', cert],
            {timeout: 5000, encoding: 'utf8', maxBuffer: 32768});
        assert.ifError(result.error); assert.equal(result.status, 0, result.stderr);
        return {key: fs.readFileSync(key), cert: fs.readFileSync(cert)};
    }
    async function listen(t, server) {
        const sockets = new Set();
        server.on('connection', socket => {sockets.add(socket); socket.on('close', () => sockets.delete(socket));});
        server.on('tlsClientError', () => {});
        server.listen(0, '127.0.0.1'); await once(server, 'listening');
        t.after(() => {for (const socket of sockets) socket.destroy(); server.close();});
        return server.address().port;
    }
    async function httpFixture(t, options, handler) {
        const server = options ? https.createServer(options, handler) : http.createServer(handler);
        const port = await listen(t, server);
        return (options ? 'https' : 'http') + '://127.0.0.1:' + port;
    }
    async function smtpFixture(t, {mode = 'plain', credentials, rejectRecipient = false} = {}) {
        const messages = [], sessions = [];
        function session(socket, secure = mode === 'implicit', state = {commands: [], secure, recipients: []}) {
            if (!sessions.includes(state)) sessions.push(state);
            let pending = '', inData = false;
            socket.setEncoding('utf8'); socket.on('error', () => {});
            if (!state.greeted) {socket.write('220 localhost fixture\r\n'); state.greeted = true;}
            function data(chunk) {
                pending += chunk;
                while (true) {
                    if (inData) {
                        const end = pending.indexOf('\r\n.\r\n');
                        if (end < 0) return;
                        messages.push({body: pending.slice(0, end).replace(/^\.\./gm, '.'), recipients: [...state.recipients], secure: state.secure});
                        pending = pending.slice(end + 5); inData = false;
                        socket.write('250 2.0.0 accepted fixture-id\r\n');
                    }
                    const end = pending.indexOf('\r\n');
                    if (end < 0) return;
                    const line = pending.slice(0, end); pending = pending.slice(end + 2);
                    state.commands.push(line);
                    if (/^EHLO|^HELO/.test(line)) {
                        socket.write('250-localhost\r\n' + (mode === 'starttls' && !state.secure ? '250-STARTTLS\r\n' : '') + '250 AUTH PLAIN LOGIN\r\n');
                    } else if (line === 'STARTTLS') {
                        socket.write('220 Ready for TLS\r\n'); socket.removeListener('data', data);
                        const encrypted = new tls.TLSSocket(socket, {isServer: true, secureContext: tls.createSecureContext(credentials)});
                        state.secure = true; session(encrypted, true, state); return;
                    } else if (/^AUTH PLAIN /.test(line)) {
                        state.auth = Buffer.from(line.slice(11), 'base64').toString('utf8');
                        socket.write('235 2.7.0 authenticated\r\n');
                    } else if (/^MAIL FROM:/.test(line)) {state.recipients = []; socket.write('250 sender ok\r\n');}
                    else if (/^RCPT TO:/.test(line)) {
                        if (rejectRecipient) socket.write('550 5.1.1 fixture rejected\r\n');
                        else {state.recipients.push(line.slice(8)); socket.write('250 recipient ok\r\n');}
                    } else if (line === 'DATA') {inData = true; socket.write('354 End with dot\r\n');}
                    else if (line === 'QUIT') {socket.end('221 Bye\r\n'); return;}
                    else socket.write('250 ok\r\n');
                }
            }
            socket.on('data', data);
        }
        const server = mode === 'implicit' ? tls.createServer(credentials, socket => session(socket)) : net.createServer(socket => session(socket));
        const port = await listen(t, server);
        return {port, messages, sessions};
    }
    function notification(port, extra = {}) {
        return {smtpHost: '127.0.0.1', smtpPort: port, smtpSecure: false, smtpIgnoreSTARTTLS: true,
            smtpFrom: 'Sender <sender@example.test>', smtpTo: 'Recipient <recipient@example.test>', ...extra};
    }
    function transport(options = {}) {
        return mailer.createTransport({streamTransport: true, buffer: true, ...options});
    }

    test('Nodemailer pin has no runtime dependencies and supports the image Node runtime', () => {
        const pkg = appRequire('nodemailer/package.json');
        assert.equal(pkg.version, '10.3.1'); assert.deepEqual(pkg.dependencies || {}, {});
        assert(appRequire('semver').satisfies(process.versions.node, pkg.engines.node));
        assert.equal(typeof appRequire('nodemailer/lib/well-known/services.json'), 'object');
    });
    for (const field of ['path', 'href']) {
        test('raw message enforces disabled ' + (field === 'path' ? 'file' : 'URL') + ' access', async t => {
            let calls = 0;
            const root = directory(t), file = path.join(root, 'dummy.eml');
            fs.writeFileSync(file, 'Subject: dummy\r\n\r\nDUMMY_FIXTURE_CONTENT');
            const url = await httpFixture(t, null, (req, res) => {calls++; res.end('Subject: dummy\r\n\r\nDUMMY_FIXTURE_CONTENT');});
            const raw = {[field]: field === 'path' ? file : url};
            await assert.rejects(transport({disableFileAccess: true, disableUrlAccess: true}).sendMail({...message, raw}), /access.*(reject|disabl)|not allowed/i);
            assert.equal(calls, 0);
        });
    }
    test('permitted raw files and URLs retain exact message bytes', async t => {
        const raw = 'Subject: dummy\r\n\r\nDUMMY_FIXTURE_CONTENT\r\n';
        const root = directory(t), file = path.join(root, 'dummy.eml'); fs.writeFileSync(file, raw);
        const url = await httpFixture(t, null, (req, res) => res.end(raw));
        for (const source of [{path: file}, {href: url}]) {
            const info = await transport().sendMail({...message, raw: source});
            assert.equal(info.message.toString(), raw);
        }
    });
    test('JSON normalization also enforces file and URL access flags', async t => {
        const file = path.join(directory(t), 'dummy.txt'); fs.writeFileSync(file, 'DUMMY');
        let calls = 0;
        const url = await httpFixture(t, null, (req, res) => {calls++; res.end('DUMMY');});
        const json = mailer.createTransport({jsonTransport: true, disableFileAccess: true, disableUrlAccess: true});
        for (const text of [{path: file}, {href: url}]) await assert.rejects(json.sendMail({...message, text}), /access.*(reject|disabl)|not allowed/i);
        assert.equal(calls, 0);
    });
    test('internal HTTPS fetch rejects an untrusted peer before sending dummy credentials', async t => {
        const credentials = certificate(t); let requests = 0;
        const url = await httpFixture(t, credentials, async (req, res) => {
            requests++; for await (const chunk of req) {} res.end('fixture');
        });
        const fetch = appRequire('nodemailer/lib/fetch');
        const collect = async stream => {const chunks = []; for await (const chunk of stream) chunks.push(chunk); return Buffer.concat(chunks).toString();};
        await assert.rejects(collect(fetch(url, {method: 'POST', body: 'client_secret=DUMMY'})), /self.signed|certificate|cert_/i);
        assert.equal(requests, 0);
        assert.equal(await collect(fetch(url, {tls: {ca: credentials.cert}})), 'fixture');
        assert.equal(requests, 1);
    });
    for (const [label, payload] of [
        ['flat recipient list', "'a@example.test,'.repeat(100000)"],
        ['free-text fallback', "' >' + '>[x][x]'.repeat(40000)"],
    ]) {
        test('addressparser terminates within the child deadline for ' + label, () => {
            const code = `const assert=require('node:assert/strict'),parse=require('nodemailer/lib/addressparser');
                try {const result=parse(${payload});assert(Array.isArray(result));}
                catch(e){assert.match(e.message,/limit|exceed|too (large|long|many)/i);}`;
            const result = spawnSync(process.execPath, ['--max-old-space-size=96', '-e', code], {timeout: 2500, encoding: 'utf8', maxBuffer: 32768});
            assert.ifError(result.error); assert.equal(result.status, 0, result.stderr.slice(-2000));
        });
    }
    test('quoted local parts, nested comments and semicolons cannot add envelope recipients', () => {
        for (const [input, expected] of [
            ['Name <"left>right"@example.test>', '"left>right"@example.test'],
            ['safe@example.test (outer (inner) injected@other.test)', 'safe@example.test'],
            ['Team: "semi;colon" <safe@example.test>;', 'safe@example.test'],
        ]) {
            const addresses = parse(input, {flatten: true});
            assert.deepEqual(addresses.map(item => item.address), [expected], input);
        }
    });
    test('SMTP MIME preserves Unicode, attachments, Bcc privacy and canonical IDN envelope domains', async t => {
        const fixture = await smtpFixture(t);
        const smtp = mailer.createTransport({host: '127.0.0.1', port: fixture.port, secure: false, ignoreTLS: true,
            connectionTimeout: 1500, socketTimeout: 2000, greetingTimeout: 1500});
        const info = await smtp.sendMail({...message, to: 'user@bücher.test', bcc: 'private@example.test',
            subject: 'Výpočet 😀', text: 'plain fixture', html: '<b>fixture</b>',
            attachments: [{filename: 'fixture.txt', content: Buffer.from('attachment fixture')}]});
        assert.deepEqual(info.envelope.to, ['user@xn--bcher-kva.test', 'private@example.test']);
        const body = fixture.messages[0].body;
        assert(!/^Bcc:/im.test(body)); assert.match(body, /^Subject: =\?UTF-8\?/im);
        assert.match(body, /multipart\/mixed/); assert.match(body, /multipart\/alternative/);
        assert(body.includes(Buffer.from('attachment fixture').toString('base64')));
    });
    test('actual Kuma SMTP provider preserves auth, envelope recipients, headers and plain body', async t => {
        const fixture = await smtpFixture(t);
        const config = notification(fixture.port, {smtpUsername: 'fixture', smtpPassword: 'DUMMY',
            smtpCC: 'cc@example.test', smtpBCC: 'hidden@example.test', smtpAdditionalHeaders: JSON.stringify({'X-Fixture': 'owned'})});
        assert.equal(await new SMTP().send(config, 'plain fixture'), 'Sent Successfully.');
        assert.equal(fixture.messages.length, 1);
        assert.deepEqual(fixture.messages[0].recipients.sort(), ['<recipient@example.test>', '<cc@example.test>', '<hidden@example.test>'].sort());
        assert.equal(fixture.sessions[0].auth, '\0fixture\0DUMMY');
        assert.match(fixture.messages[0].body, /^X-Fixture: owned\r?$/im);
        assert.match(fixture.messages[0].body, /plain fixture/); assert(!/^Bcc:/im.test(fixture.messages[0].body));
    });
    test('actual SMTP provider preserves custom Liquid HTML and heartbeat time', async t => {
        const fixture = await smtpFixture(t);
        const config = notification(fixture.port, {customSubject: '{{ name }} {{ heartbeatJSON.status }}',
            customBody: '<b>{{ name }}</b>:{{ msg }}', htmlBody: true});
        const monitor = {name: 'fixture', type: 'push'}, heartbeat = {status: 1, timezone: 'UTC', localDateTime: '2026-11-03 10:00:00'};
        await new SMTP().send(config, 'up fixture', monitor, heartbeat);
        assert.match(fixture.messages[0].body, /^Subject: fixture 1\r?$/im);
        assert.match(fixture.messages[0].body, /Content-Type: text\/html/); assert.match(fixture.messages[0].body, /<b>fixture<\/b>:up fixture/);
        await new SMTP().send(notification(fixture.port), 'up fixture', monitor, heartbeat);
        assert.match(fixture.messages[1].body, /Time \(UTC\): 2026-11-03 10:00:00/);
    });
    test('actual SMTP provider retains DKIM signing and the independently checked body hash', async t => {
        const fixture = await smtpFixture(t);
        const {privateKey} = crypto.generateKeyPairSync('rsa', {modulusLength: 2048,
            privateKeyEncoding: {type: 'pkcs8', format: 'pem'}, publicKeyEncoding: {type: 'spki', format: 'pem'}});
        await new SMTP().send(notification(fixture.port, {smtpDkimDomain: 'example.test', smtpDkimKeySelector: 'fixture',
            smtpDkimPrivateKey: privateKey, smtpDkimHashAlgo: 'sha256'}), 'dkim fixture');
        const body = fixture.messages[0].body;
        const header = body.split('\r\n\r\n')[0].replace(/\r\n[ \t]+/g, ' ');
        assert.match(header, /^DKIM-Signature:.*d=example\.test;.*s=fixture;/im);
        const expected = crypto.createHash('sha256').update('dkim fixture\r\n').digest('base64');
        assert(header.includes('bh=' + expected));
    });
    for (const mode of ['implicit', 'starttls']) {
        test('actual SMTP provider rejects untrusted ' + mode + ' TLS and preserves explicit opt-out', async t => {
            const credentials = certificate(t), fixture = await smtpFixture(t, {mode, credentials});
            const config = notification(fixture.port, {smtpSecure: mode === 'implicit', smtpIgnoreSTARTTLS: false});
            await assert.rejects(new SMTP().send(config, 'fixture'), /self.signed|certificate|cert_/i);
            assert.equal(fixture.messages.length, 0);
            assert.equal(await new SMTP().send({...config, smtpIgnoreTLSError: true}, 'trusted-by-config fixture'), 'Sent Successfully.');
            assert.equal(fixture.messages.length, 1); assert.equal(fixture.messages[0].secure, true);
        });
    }
    test('actual SMTP provider reports recipient errors and invalid headers without a message', async t => {
        const fixture = await smtpFixture(t, {rejectRecipient: true});
        await assert.rejects(new SMTP().send(notification(fixture.port), 'fixture'), /550|recipients.*reject/i);
        await assert.rejects(new SMTP().send(notification(fixture.port, {smtpAdditionalHeaders: '{'}), 'fixture'), /Additional Headers is not a valid JSON/);
        assert.equal(fixture.messages.length, 0);
    });
} else {
    console.log('Original Nodemailer range: migration checks deferred to the qualified candidate.');
}
