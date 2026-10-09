// Independent exact references from Node's native BigInt. No NumForge calls.
const fs = require('node:fs');
function makeCases(quick, factorialOnly = false) {
    const rows = [];
    const factorialReferences = new Map();
    let seed = 0x31415926;
    function random(limbs, pattern = 'random') {
        let value = 0n;
        for (let i = 0; i < limbs * 2; i++) {
            seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
            const word = pattern === 'max' ? 0xffffffff : pattern === 'sparse' ? 0 :
                pattern === 'alternating' ? (i % 2 ? 0x55555555 : 0xaaaaaaaa) : seed;
            value |= BigInt(word) << BigInt(i * 32);
        }
        return value | (1n << BigInt(limbs * 64 - 1));
    }
    const abs = x => x < 0n ? -x : x;
    function gcd(a, b) { a = abs(a); b = abs(b); while (b) [a, b] = [b, a % b]; return a; }
    function add(name, op, mode, a, b = 0n) {
        let expected, remainder = 0n;
        if (op === 'mul') expected = a * b;
        else if (op === 'square') expected = a * a;
        else if (op === 'copy') expected = a;
        else if (op === 'pow' || op === 'pow_square') expected = a ** b;
        else if (op === 'factorial' || op === 'tree') {
            expected = factorialReferences.get(a);
            if (expected === undefined) {
                expected = 1n; for (let i = 2n; i <= a; i++) expected *= i;
                factorialReferences.set(a, expected);
            }
        } else if (op === 'divmod') { expected = a / b; remainder = a % b; }
        else if (op === 'div') expected = a / b;
        else if (op === 'mod') expected = a % b;
        else if (op === 'gcd') expected = gcd(a, b);
        else throw Error(op);
        rows.push([name, op, mode, a, b, expected, remainder].map(String).join('\t'));
    }
    function factorials() {
        for (const n of quick ? [0, 1, 20, 100] : [0, 1, 20, 100, 500, 1000, 5000, 10000, 20000, 50000, 100000])
            for (const op of ['factorial', 'tree']) add(`${op}-${n}`, op, 'reuse', BigInt(n));
    }
    if (factorialOnly) { factorials(); return rows; }
    const sizes = quick ? [1, 8, 32] : [1, 2, 4, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 256, 384, 512, 768, 1024];
    for (const n of sizes) {
        const a = random(n), b = random(n);
        for (const mode of ['reuse', 'cold', 'alias_a', 'alias_b']) add(`mul-balanced-${n}-${mode}`, 'mul', mode, a, b);
        for (const pattern of ['max', 'sparse', 'alternating']) {
            const patterned = random(n, pattern);
            add(`mul-${pattern}-${n}`, 'mul', 'reuse', patterned, random(n, pattern));
            add(`square-special-${pattern}-${n}`, 'square', 'reuse', patterned);
        }
        add(`mul-negative-${n}`, 'mul', 'reuse', -a, b);
        for (const small of [...new Set([1, Math.max(1, Math.floor(n / 8)), Math.max(1, Math.floor(n / 2))])]) {
            const s = random(small);
            add(`mul-ratio-${n}-${small}`, 'mul', 'reuse', a, s);
            if (n !== small) add(`mul-reverse-${small}-${n}`, 'mul', 'reuse', s, a);
        }
        add(`copy-${n}`, 'copy', 'reuse', a);
        add(`square-mul-${n}`, 'mul', 'reuse', a, a);
        add(`square-alias-${n}`, 'mul', 'alias_all', a, a);
        add(`square-special-${n}`, 'square', 'reuse', a);
        add(`square-special-alias-${n}`, 'square', 'alias_a', a);
        if (n <= (quick ? 8 : 64)) {
            const divisor = random(Math.max(1, Math.floor(n / 2)));
            const pairs = [['single', a, 97n], ['similar', a, b], ['unequal', a, divisor],
                ['exact', a * divisor, divisor], ['remainder', a * divisor + 1n, divisor],
                ['negative', -a, divisor], ['negative-divisor', a, -divisor], ['both-negative', -a, -divisor],
                ['smaller', divisor, a]];
            for (const [shape, x, y] of pairs) for (const op of ['divmod', 'div', 'mod', 'gcd'])
                add(`${op}-${shape}-${n}`, op, 'reuse', x, y);
            add(`divmod-alias-${n}`, 'divmod', 'alias_pair', a * divisor + 1n, divisor);
            // Consecutive Fibonacci numbers exercise long Euclidean chains.
            let f = 1n, g = 1n; for (let i = 0; i < n * 32; i++) [f, g] = [g, f + g];
            add(`gcd-fibonacci-${n}`, 'gcd', 'reuse', f, g);
            add(`gcd-common-${n}`, 'gcd', 'reuse', a * 210n, divisor * 210n);
        }
    }
    for (const a of [0n, 1n, -1n, 2n, -2n]) for (const b of [0n, 1n, -1n, (1n << 129n) - 1n])
        for (const mode of ['reuse', 'alias_a', 'alias_b']) add(`edge-mul-${a}-${b}-${mode}`, 'mul', mode, a, b);
    for (const a of [0n, 1n, -1n, -((1n << 129n) - 1n)]) {
        for (const mode of ['reuse', 'alias_a']) add(`edge-square-${a}-${mode}`, 'square', mode, a);
        for (const b of [0n, 1n, -1n]) add(`edge-gcd-${a}-${b}`, 'gcd', 'reuse', a, b);
    }
    factorials();
    for (const n of quick ? [1, 8] : [1, 4, 16, 64]) for (const exponent of quick ? [0, 2, 17] : [0, 1, 2, 3, 16, 17, 64]) {
        const base = random(n);
        for (const op of ['pow', 'pow_square']) add(`${op}-${n}-${exponent}`, op, 'reuse', base, BigInt(exponent));
    }
    for (const a of [0n, 1n, -1n, -7n]) for (const exponent of [0n, 1n, 2n, 17n])
        for (const op of ['pow', 'pow_square']) add(`edge-${op}-${a}-${exponent}`, op, 'alias_a', a, exponent);
    return rows;
}
module.exports = { makeCases };
if (require.main === module) {
    if (!process.argv[2]) throw Error('Usage: node benchmarks/bigint_cases.js output.tsv [--quick] [--factorial-only]');
    fs.writeFileSync(process.argv[2], makeCases(process.argv.includes('--quick'), process.argv.includes('--factorial-only')).join('\n') + '\n');
}
