'use strict';
const { spawnSync } = require('node:child_process');
if (!process.argv[2]) throw new Error('Usage: node test_division_oracle.js <numeric_oracle_driver>');
const B = 1n << 64n;
const cases = [];
let seed = 0xd171de, sequence = 0;
function word() { seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0; return BigInt(seed); }
function random(limbs) {
    let value = 0n;
    for (let i = 0; i < limbs * 2; i++) value = (value << 32n) | word();
    return value | (1n << BigInt(limbs * 64 - 1));
}
function pair(a, b, allAliases = false) {
    if (b === 0n) throw new Error('Invalid oracle divisor');
    for (const [x, y] of [[a, b], [-a, b], [a, -b], [-a, -b]]) {
        const expected = `${x / y};${x % y}`;
        const aliases = allAliases ? [0, 1, 2, 3, 4, 5, 6] : [sequence++ % 7];
        for (const alias of aliases) cases.push({ input: `idivpair ${x} ${y} ${alias} 0`, expected });
    }
}
// Every normalization shift, maximal quotient digits, exact/near-exact results.
for (let shift = 0; shift < 64; shift++) for (const limbs of [2, 3]) {
    const v = (1n << BigInt(64 * limbs - 1 - shift)) | (B - 1n);
    for (const q of [1n, 2n, B - 1n, B, B + 1n])
        for (const r of [0n, 1n, v - 1n]) pair(v * q + r, v);
}
// Quotient-estimate corrections, add-back, high-limb equality and zero digits.
for (const limbs of [2, 3, 4]) {
    const high = (B / 2n) * B ** BigInt(limbs - 1);
    for (const v of [high + 1n, high + B ** BigInt(limbs - 1) - 1n, B ** BigInt(limbs) - 1n])
        for (const q of [2n, B - 1n, B + 1n, B ** 3n + 1n]) pair(v * q - 1n, v, true);
}
for (const divisor of [1n, 2n, B / 2n, B - 1n, B, B + 1n])
    for (const a of [0n, 1n, divisor - 1n, divisor, divisor + 1n, B ** 4n - 1n]) pair(a, divisor, true);
for (const limbs of [1, 2, 3, 4, 8, 16, 32, 64]) for (let i = 0; i < 128; i++) {
    const v = random(1 + i % limbs), q = random(1 + i % 4);
    pair(v * q + random(limbs) % v, v);
}
// Near the line protocol's 19999-digit operand cap, with small/large divisors.
pair((1n << 65500n) - 1n, (1n << 32700n) + 1n, true);
pair((1n << 65500n) - 1n, B - 1n, true);
const result = spawnSync(process.argv[2], {
    input: cases.map(c => c.input).join('\n') + '\n', encoding: 'utf8',
    timeout: 110000, windowsHide: true, maxBuffer: 32 * 1024 * 1024
});
if (result.error) throw result.error;
if (result.status !== 0) throw new Error(`Driver exit ${result.status}: ${result.stderr}`);
const actual = result.stdout.trim().split(/\r?\n/);
if (actual.length !== cases.length) throw new Error(`Expected ${cases.length} outputs, got ${actual.length}`);
let failures = 0;
cases.forEach((c, i) => {
    if (actual[i] !== c.expected) {
        if (failures++ < 5) console.error({ input: c.input, expected: c.expected, actual: actual[i] });
    }
});
console.log(JSON.stringify({ seed: '0xd171de', cases: cases.length, failures }));
process.exitCode = failures ? 1 : 0;
