// Run against the RedBean consumer; unrelated glob major versions stay intact.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {createRequire} = require('node:module');
const {spawnSync} = require('node:child_process');
const appRequire = createRequire('/app/package.json');
const ownerRequire = createRequire(appRequire.resolve('redbean-node'));
const receipt = '/opt/numforge/monitor-patches.json';
const manifest = fs.existsSync(receipt) ? JSON.parse(fs.readFileSync(receipt, 'utf8')) : {};
if (manifest.consumerMigrations?.some(p => p.name === 'glob') || process.env.NUMFORGE_FORCE_GLOB_CHECKS) {
    const glob = ownerRequire('glob');
    const globFile = ownerRequire.resolve('glob/package.json');
    const globRequire = createRequire(globFile);
    const modern = globRequire('./package.json').version === '14.0.2';
    const cli = path.join(path.dirname(globFile), 'dist/esm/bin.mjs');

    function directory(t) {
        const root = fs.mkdtempSync(path.join(os.tmpdir(), 'nf-glob-'));
        t.after(() => fs.rmSync(root, {recursive: true}));
        return root;
    }
    function command(root, args, shell = '/bin/sh') {
        const result = spawnSync(process.execPath, [cli, ...args], {cwd: root,
            env: {PATH: process.env.PATH, SHELL: shell}, encoding: 'utf8', timeout: 3000, maxBuffer: 32768});
        assert.ifError(result.error);
        assert.equal(result.status, 0, result.stderr);
        return result.stdout.trim().split('\n').filter(Boolean);
    }

    test('RedBean glob is pinned while legacy root glob and owner stay unchanged', () => {
        assert.equal(globRequire('./package.json').version, modern ? '14.0.2' : '10.5.0');
        assert.equal(appRequire('glob/package.json').version, '7.2.3');
        assert.equal(ownerRequire('redbean-node/package.json').version, '0.3.3');
        assert.equal(globRequire('minimatch/package.json').version, modern ? '11.0.1' : '9.0.9');
        if (!modern) assert.equal(globRequire('package-json-from-dist/package.json').version, '1.0.1');
    });
    test('sync, async, stream and iterator glob APIs preserve recursive matching', async t => {
        const root = directory(t);
        for (const name of ['model/a.js', 'model/nested/b.js', 'model/nested/c.ts', 'model/.hidden.js', 'other.js']) {
            fs.mkdirSync(path.dirname(path.join(root, name)), {recursive: true});
            fs.writeFileSync(path.join(root, name), '');
        }
        const options = {cwd: root, nodir: true, ignore: ['**/nested/c.ts']};
        const expected = ['model/a.js', 'model/nested/b.js'];
        assert.deepEqual(glob.globSync('model/**/*.{js,ts}', options).sort(), expected);
        assert.deepEqual((await glob.glob('model/**/*.{js,ts}', options)).sort(), expected);
        assert.deepEqual([...glob.globIterateSync('model/**/*.{js,ts}', options)].sort(), expected);
        const streamed = [];
        for await (const item of glob.globStream('model/**/*.{js,ts}', options)) streamed.push(item);
        assert.deepEqual(streamed.sort(), expected);
        assert.deepEqual(glob.globSync('absent/**/*.js', options), []);
        assert(glob.globSync('model/**/*.js', {...options, dot: true}).includes('model/.hidden.js'));
    });
    if (!modern) {
    for (const shell of ['/bin/sh', '/bin/unknown-shell']) {
        test('CLI default command treats shell syntax as literal filenames: ' + shell, t => {
            const root = directory(t);
            for (const [name, marker] of [['$(touch injected-substitution)', 'injected-substitution'],
                ['`touch injected-backtick`', 'injected-backtick'], ['file; touch injected-semicolon; #', 'injected-semicolon']]) {
                const cwd = path.join(root, marker);
                fs.mkdirSync(cwd);
                fs.writeFileSync(path.join(cwd, name), '');
                // Only the old CLI executes these benign marker commands.
                // No host directory or runtime configuration is mounted.
                const output = command(cwd, ['-c', 'echo', '*'], shell);
                assert(!fs.existsSync(path.join(cwd, marker)), 'CLI executed a filename: ' + marker);
                assert.deepEqual(output, [name]);
            }
        });
    }
    test('CLI cmd-arg preserves argument boundaries including whitespace and metacharacters', t => {
        const root = directory(t);
        const names = ['$(touch marker)', 'file with spaces', 'Výpočet-😀', 'quote"and\'apostrophe'];
        for (const name of names) fs.writeFileSync(path.join(root, name), '');
        fs.writeFileSync(path.join(root, 'args.cjs'), 'console.log(JSON.stringify(process.argv.slice(2)))');
        const output = command(root, ['-c', process.execPath, '-g', path.join(root, 'args.cjs'), '-g', 'prefix value', '--ignore', 'args.cjs', '*']);
        const args = JSON.parse(output.join('\n'));
        assert.equal(args.shift(), 'prefix value');
        assert.deepEqual(args.sort(), names.sort());
        assert(!fs.existsSync(path.join(root, 'marker')));
    });
    test('CLI known-shell command text preserves filenames as positional arguments', t => {
        const root = directory(t);
        fs.writeFileSync(path.join(root, '$(touch marker)'), '');
        const output = command(root, ['-c', 'printf "%s\\n"', '*']);
        assert.deepEqual(output, ['$(touch marker)']);
        assert(!fs.existsSync(path.join(root, 'marker')));
    });
    test('CLI version and no-match listing remain usable', t => {
        const root = directory(t);
        assert.deepEqual(command(root, ['--version']), ['10.5.0']);
        assert.deepEqual(command(root, ['absent-*']), []);
    });
    } else {
        test('glob 14 has no removed CLI entrypoint or command launcher', () => {
            assert.equal(globRequire('./package.json').bin, undefined);
            assert(!fs.existsSync(cli));
        });
    }
} else {
    console.log('Original glob consumer: migration checks deferred to the qualified candidate.');
}
