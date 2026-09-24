'use strict';

// Exercise the real process, including terminal framing and recovery after errors.
const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const executable = process.argv[2];
assert.ok(executable, 'Pass the calculator executable');

function run(input) {
    const child = spawnSync(executable, [], {
        input, encoding: 'utf8', timeout: 15000, windowsHide: true,
        maxBuffer: 1024 * 1024,
    });
    if (child.error) throw child.error;
    assert.equal(child.status, 0, child.stderr);
    return {
        ...child,
        results: [...child.stdout.matchAll(/= ([^\r\n]*)/g)].map(match => match[1]),
    };
}

let result = run('0,1+0.2\n2(2+2)\nπ-π\n1e3-3e\n1E3\n5!\n2^10\n');
assert.deepEqual(result.results, ['0.3', '8', '0', '0', '1000', '120', '1024']);
assert.equal(result.stderr, '');

result = run('precision\nprecision 2\n1/8\nprecision full\n1/8\nprecision\nprecision -1\nprecision 10001\nprecision\n');
assert.deepEqual(result.results, ['0.12', '0.125']);
assert.match(result.stdout, /output precision: 10 decimal places/);
assert.equal((result.stdout.match(/output precision: full/g) || []).length, 2);
assert.match(result.stderr, /precision must be/);
assert.match(result.stderr, /failed to set precision/);

result = run('angle\nangle deg\nsin(90)\natan(1)\nangle\nangle rad\nsin(π\/2)\nangle nope\n');
assert.deepEqual(result.results, ['1', '45', '1']);
assert.match(result.stdout, /angle unit: RAD/);
assert.match(result.stdout, /angle unit: DEG/);
assert.match(result.stderr, /angle unit must be/);

result = run('1.2.3\nπ/0\n2+2\n');
assert.deepEqual(result.results, ['4']);
assert.match(result.stderr, /error at column 4:/);
assert.match(result.stderr, /error at column 2: division by zero/);

result = run(' '.repeat(4093) + '2+2\n' + '1'.repeat(5000) + '\n3+3\n');
assert.deepEqual(result.results, ['4', '6']);
assert.match(result.stderr, /input is too long/);
assert.deepEqual(run('\n2+2').results, ['4']); // EOF without newline
assert.deepEqual(run('').results, []);
for (const command of ['exit', 'quit']) {
    assert.deepEqual(run(`2+2\r\n${command}\r\n9+9\r\n`).results, ['4']);
}
console.log('CLI process regressions passed');

result = run('ans\nprecision 2\n1/8\nprecision full\nans*8\n1/0\nans+1\nhistory\nreset\nans\n');
assert.deepEqual(result.results, ['0.12', '1', '2']);
assert.match(result.stdout, /1: 1\/8 -> 0.12/);
assert.match(result.stdout, /3: ans\+1 -> 2/);
assert.equal((result.stderr.match(/ans is undefined/g) || []).length, 2);
assert.match(result.stderr, /division by zero/);
