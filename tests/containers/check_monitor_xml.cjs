const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), crypto = require('node:crypto');
const {createRequire} = require('node:module');
const {spawnSync} = require('node:child_process');
const appRequire = createRequire('/app/package.json');
if (fs.existsSync('/opt/numforge/monitor-xml.json') || process.env.NUMFORGE_FORCE_XML_CHECKS) {
    test('XML replacement pins the real consumer, lock entries and complete package bytes', () => {
        const receipt = JSON.parse(fs.readFileSync('/opt/numforge/monitor-xml.json'));
        const root = '/app/node_modules/fast-xml-parser';
        const manifest = receipt.manifest;
        const parserPackage = JSON.parse(fs.readFileSync(root + '/package.json'));
        const gamePackage = JSON.parse(fs.readFileSync('/app/node_modules/gamedig/package.json'));
        assert.equal(parserPackage.version, '5.12.0');
        assert.equal(gamePackage.version, manifest.consumer.version);
        assert.equal(gamePackage.dependencies['fast-xml-parser'], manifest.consumer.fromRange);
        assert.deepEqual(appRequire('/app/package.json').overrides.gamedig, {'fast-xml-parser': '5.12.0'});
        for (const [relative, mode, digest] of receipt.files) {
            const file = path.join(root, relative);
            assert.equal(fs.statSync(file).mode & 511, mode);
            assert.equal(crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex'), digest);
        }
        for (const filename of ['/app/package-lock.json', '/app/node_modules/.package-lock.json']) {
            const lock = JSON.parse(fs.readFileSync(filename));
            for (const pkg of manifest.packages) {
                const location = 'node_modules/fast-xml-parser' + (pkg.name === 'fast-xml-parser' ? '' : '/node_modules/' + pkg.name);
                assert.equal(lock.packages[location].version, pkg.version);
                assert.equal(lock.packages[location].integrity, pkg.integrity);
            }
        }
    });
    test('XML parser and builder preserve strings, Unicode, attributes and entities', () => {
        const {XMLParser, XMLBuilder, XMLValidator} = appRequire('fast-xml-parser');
        const parser = new XMLParser({ignoreAttributes: false, parseTagValue: false, parseAttributeValue: false});
        const xml = '<r label="výpočet 😀"><n>001</n><n>1/3</n><text>&lt; &amp; ž</text></r>';
        assert.equal(XMLValidator.validate(xml), true);
        const value = parser.parse(xml);
        assert.deepEqual(value.r.n, ['001', '1/3']);
        assert.equal(value.r['@_label'], 'výpočet 😀');
        assert.equal(value.r.text, '< & ž');
        assert.deepEqual(parser.parse(new XMLBuilder({ignoreAttributes: false}).build(value)), value);
        assert.notEqual(XMLValidator.validate('<r><x></r>'), true);
    });
    test('unterminated stop nodes fail within a bounded child process', () => {
        const program = "const {XMLParser}=require('/app/node_modules/fast-xml-parser');try{new XMLParser({stopNodes:['stop']}).parse('<stop>'+'<a '.repeat(100000));process.exit(2);}catch(e){if(!/Unexpected end|tag/i.test(e.message))throw e;}";
        const child = spawnSync(process.execPath, ['-e', program], {encoding: 'utf8', timeout: 1500});
        assert.equal(child.error, undefined, child.error?.message);
        assert.equal(child.status, 0, child.stderr);
    });
    test('actual GameDig Farming Simulator protocol retains parsed server and players', async () => {
        const {default: Protocol} = await import('file:///app/node_modules/gamedig/protocols/farmingsimulator.js');
        const protocol = new Protocol();
        protocol.options = {host: '127.0.0.1', port: 8080, token: 'fixture-only'};
        protocol.request = async options => {
            assert(options.url.endsWith('/feed/dedicated-server-stats.xml?code=fixture-only'));
            return '<Server name="Výpočet" mapName="Mapa" version="1.2"><Slots numUsed="1" capacity="8">' +
                '<Player isUsed="true" isAdmin="false" uptime="12" x="1.5" y="2" z="3">Alice</Player>' +
                '<Player isUsed="false">Free</Player></Slots><Mods><Mod name="m1" author="A" version="1" hash="h">Prvý</Mod>' +
                '<Mod name="m2" author="B" version="2" hash="i">Druhý</Mod></Mods></Server>';
        };
        const state = {players: [], raw: {}};
        await protocol.run(state);
        assert.equal(state.name, 'Výpočet'); assert.equal(state.map, 'Mapa');
        assert.equal(state.numplayers, 1); assert.equal(state.maxplayers, 8);
        assert.equal(state.players.length, 1); assert.equal(state.players[0].name, 'Alice');
        assert.equal(state.players[0].x, 1.5); assert.equal(state.raw.mods.length, 2);
    });
}
