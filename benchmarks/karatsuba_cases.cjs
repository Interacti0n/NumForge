// Independent whole-product references: Node BigInt, never NumForge arithmetic.
const fs = require('node:fs');
function makeCases(quick = false, timing = false) {
    const rows = [];
    let seed = 0x31415926;
    const random = (n, pattern = 'random') => {
        let value = 0n;
        for (let i = 0; i < n * 2; ++i) {
            seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
            const word = pattern === 'max' ? 0xffffffff : pattern === 'sparse' ? 0 :
                pattern === 'alternating' ? (i % 2 ? 0x55555555 : 0xaaaaaaaa) : seed;
            value |= BigInt(word) << BigInt(i * 32);
        }
        return value | (1n << BigInt(n * 64 - 1));
    };
    const operations = ['mul', 'karatsuba16', 'karatsuba32', 'karatsuba64', 'karatsuba128', 'karatsuba256'];
    function add(name, a, b, modes = ['reuse']) {
        for (const mode of modes) for (const op of operations) {
            const product = mode === 'alias_all' ? a * a : a * b;
            const fixture = x => timing ? 'hex:' + (x < 0n ? '-' : '') + (x < 0n ? -x : x).toString(16) : String(x);
            rows.push([name + '-' + mode + '-' + op, op, mode, fixture(a), fixture(b), fixture(product), 0].join('\t'));
        }
    }
    const sizes = timing ? [16,32,48,64,96,128,192,256,384,512,768,1024,2048,4096,8192] :
        quick ? [15,16,17,31,32,33,63,64,65,127,128,129] :
        [7,8,9,15,16,17,31,32,33,63,64,65,127,128,129,255,256,257,511,512,513,1023,1024,1025,2049];
    for (const n of sizes) {
        const a = random(n), b = random(n);
        add('balanced-' + n, a, b, timing ? ['reuse'] : ['reuse', 'alias_a', 'alias_b', 'alias_all']);
        add('square-' + n, a, a);
        if (!timing) {
            for (const pattern of ['max', 'sparse', 'alternating'])
                add(pattern + '-' + n, random(n, pattern), random(n, pattern));
            add('negative-' + n, -a, b); add('both-negative-' + n, -a, -b);
        }
        const ratios = [1, Math.max(1, Math.floor(n / 8)), Math.max(1, Math.floor(n / 2)), n - 1];
        if (!timing) ratios.push(Math.floor(n / 2) + 1, Math.floor(3 * n / 4));
        for (const small of [...new Set(ratios)]) {
            const s = random(small);
            add(`ratio-${n}-${small}`, a, s); add(`reverse-${small}-${n}`, s, a);
        }
    }
    if (!timing) {
        for (const a of [0n, 1n, -1n, (1n << 193n) - 1n])
            for (const b of [0n, -1n, (1n << 129n) - 1n])
                add(`edge-${a}-${b}`, a, b, ['reuse', 'alias_a', 'alias_b', 'alias_all']);
        for (const n of [0, 1, 20, 100, 2000]) {
            let expected = 1n;
            for (let i = 2n; i <= BigInt(n); ++i) expected *= i;
            for (const op of ['factorial', 'tree_grouped', 'tree_karatsuba16', 'tree_karatsuba32', 'tree_karatsuba64', 'tree_karatsuba128'])
                for (const mode of ['reuse', 'alias_a'])
                    rows.push([`${op}-${n}-${mode}`, op, mode, n, 0, expected, 0].join('\t'));
        }
    }
    return rows;
}
if (require.main === module) {
    if (!process.argv[2]) throw Error('Usage: node karatsuba_cases.cjs output.tsv [--quick|--timing]');
    fs.writeFileSync(process.argv[2], makeCases(process.argv.includes('--quick'), process.argv.includes('--timing')).join('\n') + '\n');
}
module.exports = {makeCases};
