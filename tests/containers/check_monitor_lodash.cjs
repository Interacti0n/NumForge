// Exercise the migrated nested library, not the already patched root copy.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const {createRequire} = require('node:module');
const appRequire = createRequire('/app/package.json');
const ownerRequire = createRequire(appRequire.resolve('redbean-node'));
const receipt = '/opt/numforge/monitor-patches.json';
const manifest = fs.existsSync(receipt) ? JSON.parse(fs.readFileSync(receipt, 'utf8')) : {};
if (manifest.consumerMigrations?.some(p => p.name === 'lodash') || process.env.NUMFORGE_FORCE_LODASH_CHECKS) {
    const lodash = ownerRequire('lodash');
    test('nested Lodash is exact while root Lodash and both Knex versions stay unchanged', () => {
        assert.equal(lodash.VERSION, '4.18.1');
        assert.equal(appRequire('lodash').VERSION, '4.18.1');
        assert.equal(ownerRequire('redbean-node/package.json').version, '0.3.3');
        assert.equal(ownerRequire('knex/package.json').version, '2.4.2');
        assert.equal(appRequire('knex/package.json').version, '3.1.0');
    });
    test('template import key cannot execute a default-parameter expression', () => {
        delete globalThis.numforgeLodashMarker;
        let failure;
        try {
            try {
                const key = 'a = (globalThis.numforgeLodashMarker = 23)';
                lodash.template('safe', {imports: {[key]: undefined}})();
            } catch (error) {failure = error;}
            assert.equal(globalThis.numforgeLodashMarker, undefined);
            assert(failure instanceof Error, 'Unsafe import key must be rejected');
        } finally {delete globalThis.numforgeLodashMarker;}
    });
    test('templates enumerate own imports without inheriting prototype helpers', () => {
        const imports = Object.create({inheritedHelper: () => 'inherited'});
        imports.ownHelper = () => 'owned';
        assert.equal(lodash.template('<%= ownHelper() %>|<%= typeof inheritedHelper %>', {imports})(), 'owned|undefined');
    });
    test('array-wrapped path segments cannot delete constructor prototype properties', () => {
        for (const method of ['unset', 'omit']) {
            function Fixture() {}
            Fixture.prototype.sentinel = 'preserved';
            const value = {constructor: Fixture};
            const route = [['constructor'], ['prototype'], 'sentinel'];
            if (method === 'unset') lodash.unset(value, route);
            else lodash.omit(value, [route]);
            assert.equal(Fixture.prototype.sentinel, 'preserved', method);
        }
    });
    test('published modular template/fromPairs helpers and ordinary data utilities work', () => {
        const template = ownerRequire('lodash/template');
        const fromPairs = ownerRequire('lodash/fromPairs');
        assert.equal(template('<%- name %>:<%= twice(value) %>', {imports: {twice: x => x * 2}})({name: '<pilot>', value: 7}), '&lt;pilot&gt;:14');
        const result = fromPairs([['name', 'pilot'], ['__proto__', {polluted: true}]]);
        assert.equal(result.name, 'pilot');
        assert.equal(Object.getPrototypeOf(result), Object.prototype);
        assert.equal({}.polluted, undefined);
        const source = {nested: {values: [1, 2]}};
        const clone = lodash.cloneDeep(source); clone.nested.values.push(3);
        assert.deepEqual(source, {nested: {values: [1, 2]}});
        assert.deepEqual(lodash.chain([{x: 2}, {x: 1}]).sortBy('x').map('x').value(), [1, 2]);
    });

    async function database(t, scopedRequire) {
        const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'nf-lodash-db-'));
        const Dialect = scopedRequire('knex/lib/dialects/sqlite3/index.js');
        t.mock.method(Dialect.prototype, '_driver', () => appRequire('@louislam/sqlite3'));
        const knex = scopedRequire('knex')({client: 'sqlite3', connection: {filename: path.join(temporary, 'fixture.sqlite')},
            useNullAsDefault: true, pool: {min: 0, max: 1}});
        t.after(async () => {await knex.destroy(); fs.rmSync(temporary, {recursive: true});});
        await knex.schema.createTable('fixture', table => {
            table.increments('id'); table.string('label').notNullable().unique(); table.integer('counter'); table.text('payload');
        });
        const {RedBeanNode} = appRequire('redbean-node');
        const r = new RedBeanNode(); r.setup(knex); r.freeze(true);
        const bean = r.dispense('fixture'); bean.label = 'Výpočet'; bean.counter = 7; bean.payload = JSON.stringify({values: [1, '😀']});
        const id = await r.store(bean);
        const loaded = await r.load('fixture', id);
        assert.equal(loaded.label, 'Výpočet'); assert.equal(loaded.counter, 7);
        assert.deepEqual(JSON.parse(loaded.payload), {values: [1, '😀']});
        loaded.label = "O'Reilly"; loaded.counter = 11; await r.store(loaded);
        const rollback = await r.begin();
        try {
            const changed = await rollback.load('fixture', id); changed.counter = 99; await rollback.store(changed);
        } finally {await rollback.rollback();}
        assert.equal((await r.load('fixture', id)).counter, 11);
        const commit = await r.begin();
        try {
            const added = commit.dispense('fixture'); added.label = 'committed'; added.counter = -2; added.payload = '{}';
            await commit.store(added); await commit.commit();
        } catch (error) {await commit.rollback(); throw error;}
        assert.equal((await r.findOne('fixture', 'label = ?', ['committed'])).counter, -2);
        const duplicate = r.dispense('fixture'); duplicate.label = "O'Reilly"; duplicate.counter = 0;
        await assert.rejects(r.store(duplicate), /UNIQUE|constraint/i);
        assert.equal((await r.load('fixture', id)).label, "O'Reilly");
        await r.trash(loaded); assert.equal(await r.load('fixture', id), null);
        assert.equal((await knex('fixture').count({total: '*'}).first()).total, 1);
    }
    test('actual RedBean CRUD, commit/rollback and error recovery work with Kuma root Knex', {timeout: 7000}, t => database(t, appRequire));
    test('RedBean nested Knex also preserves SQLite schema, bindings and transactions', {timeout: 7000}, t => database(t, ownerRequire));
} else {
    console.log('Original Lodash consumer range: migration checks deferred to the qualified candidate.');
}
