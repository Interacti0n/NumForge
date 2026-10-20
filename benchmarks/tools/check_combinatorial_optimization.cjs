'use strict';

// Independent JavaScript BigInt oracle; intentionally uses factorial quotients
// rather than the C implementation's falling-product recurrence.
const { spawnSync } = require('node:child_process');
const executable = process.argv[2];
if (!executable) throw new Error('Usage: node check_combinatorial_optimization.cjs calculator_limit_benchmark');
function factorial(n) {
    let value = 1n;
    for (let i = 2n; i <= n; ++i) value *= i;
    return value;
}
const cases = [[0n, 0n], [1000n, 500n], [10000n, 5000n], [10000n, 10000n], [100000n, 50000n]];
for (const [n, r] of cases) {
    const numerator = factorial(n);
    const permutation = numerator / factorial(n - r);
    for (const [name, expected] of [['npr', permutation], ['ncr', permutation / factorial(r)]]) {
        // The large permutation exceeds the unchanged 65536-byte output cap.
        if (name === 'npr' && n > 10000n) continue;
        const expression = `${name}(${n};${r})`;
        const run = spawnSync(executable, [expression, '-1', '1', '60000'], {
            encoding: 'utf8', timeout: 70000,
            env: { ...process.env, NUMFORGE_BENCH_EXPECTED: expected.toString() }
        });
        if (run.error || run.status !== 0 || !run.stdout.includes('success,0,'))
            throw new Error(`${expression}: ${run.error || run.stderr || run.stdout}`);
        console.log(`${expression}: exact JavaScript BigInt reference verified`);
    }
}
