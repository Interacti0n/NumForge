const fs = require('node:fs');
const path = require('node:path');
const pow = s => 10n ** BigInt(s);
const abs = n => n < 0n ? -n : n;
function parts(text) {
    const [, sign, integer, fraction = '', exponent = '0'] = /^(-?)(\d+)(?:\.(\d+))?(?:[eE]([+-]?\d+))?$/.exec(text);
    return [BigInt(sign + integer + fraction), fraction.length - Number(exponent)];
}
function decimal(c, scale) {
    if (!c) return '0';
    const sign = c < 0n ? '-' : '';
    let digits = abs(c).toString();
    while (scale > 0 && digits.endsWith('0')) { digits = digits.slice(0, -1); scale--; }
    if (scale <= 0) return sign + digits + '0'.repeat(-scale);
    if (digits.length <= scale) return sign + '0.' + '0'.repeat(scale - digits.length) + digits;
    return sign + digits.slice(0, -scale) + '.' + digits.slice(-scale);
}
function rounded(n, d, mode) {
    const negative = (n < 0n) !== (d < 0n); n = abs(n); d = abs(d);
    let q = n / d, r = n % d;
    const up = r !== 0n && (mode === 1 || (mode === 2 && negative) || (mode === 3 && !negative) ||
        (mode === 4 && 2n * r >= d) || (mode === 5 && (2n * r > d || (2n * r === d && q % 2n))));
    if (up) q++;
    return negative ? -q : q;
}
function reference(op, a, b, parameter, mode) {
    const [ac, as] = parts(a), [bc, bs] = parts(b);
    if (op === 'normalize') return decimal(BigInt(a), parameter);
    if (op === 'add' || op === 'sub') {
        const scale = Math.max(as, bs);
        return decimal(ac * pow(scale - as) + (op === 'sub' ? -bc : bc) * pow(scale - bs), scale);
    }
    if (op === 'mul') return decimal(ac * bc, as + bs);
    if (op === 'rescale') return decimal(parameter >= as ? ac * pow(parameter - as) : rounded(ac, pow(as - parameter), mode), parameter);
    if (!bc) return '0'; // caller sets expected error status
    let n = ac, d = bc;
    if (bs >= as) n *= pow(bs - as); else d *= pow(as - bs);
    if (op === 'divexact') {
        let x = abs(n), y = abs(d);
        while (y) [x, y] = [y, x % y];
        let reduced = abs(d) / x, twos = 0, fives = 0;
        while (reduced % 2n === 0n) { reduced /= 2n; twos++; }
        while (reduced % 5n === 0n) { reduced /= 5n; fives++; }
        if (reduced === 1n) { const scale = Math.max(twos, fives); return decimal(n * pow(scale) / d, scale); }
    }
    let scale = parameter;
    if (op !== 'div') {
        if (!n) return '0';
        let nn = abs(n), dd = abs(d), exponent = 0;
        while (nn >= dd * 10n) { dd *= 10n; exponent++; }
        while (nn < dd) { nn *= 10n; exponent--; }
        scale = parameter - 1 - exponent;
    }
    if (scale >= 0) n *= pow(scale); else d *= pow(-scale);
    return decimal(rounded(n, d, mode), scale);
}
function makeCases(quick = false) {
    const rows = [];
    const add = (name, op, mode, a, b = '0', parameter = 0, rounding = 5, status = 0) =>
        rows.push([name, op, mode, a, b, parameter, rounding, status, '0', reference(op, a, b, parameter, rounding)].join('\t'));
    for (const digits of quick ? [16, 100] : [16, 100, 500, 1000, 2000]) {
        const a = '7' + '3141592653589793238462643383279'.repeat(Math.ceil(digits / 29)).slice(0, digits - 1);
        const b = '3' + '2718281828459045235360287471352'.repeat(Math.ceil(digits / 29)).slice(0, digits - 1);
        for (const gap of quick ? [0, 19] : [0, 1, 100, 1000]) for (const op of ['add', 'sub'])
            for (const mode of ['reuse', 'fresh', 'alias_a', 'alias_b'])
                add(`${op}_${digits}_gap${gap}_${mode}`, op, mode, `${a}e-7`, `${b}e-${7 + gap}`);
        for (const mode of ['reuse', 'fresh', 'alias_a', 'alias_b']) {
            add(`mul_${digits}_${mode}`, 'mul', mode, `${a}e-7`, `-${b}e-3`);
            add(`cancel_${digits}_${mode}`, 'sub', mode, a, a);
        }
        for (const zeros of quick ? [0, 20] : [0, 1, 100, 1000])
            add(`normalize_${digits}_zeros${zeros}`, 'normalize', 'alias_a', a + '0'.repeat(zeros), '0', zeros + 3);
        for (const [kind, denominator] of [['finite', '8'], ['recurring', '7'], ['dense', `${b}e-${digits - 1}`]])
            for (const op of ['div', 'divsig', 'divexact']) for (const mode of ['reuse', 'alias_b'])
                add(`large_division_${digits}_${kind}_${op}_${mode}`, op, mode,
                    `${a}e-${digits - 1}`, denominator, digits, 5);
    }
    for (let rounding = 0; rounding < 6; rounding++) for (const a of ['1', '-1', '1234567890123456789e-9', '-2.345', '2.355', '2.34501']) {
        for (const scale of [-2, 2, 30])
            add(`rescale_${a.replace(/[.-]/g, '_')}_s${scale}_${rounding}`, 'rescale', 'alias_a', a, '0', scale, rounding);
        for (const b of ['8', '7', '-0.000125']) for (const op of ['div', 'divsig', 'divexact'])
            add(`${op}_${a.replace(/[.-]/g, '_')}_${b.replace(/[.-]/g, '_')}_${rounding}`, op, 'reuse', a, b, 10, rounding);
    }
    for (let rounding = 0; rounding < 6; rounding++)
        add(`division_negative_scale_${rounding}`, 'div', 'alias_b', '-12345', '7', -2, rounding);
    for (const mode of ['reuse', 'alias_a', 'alias_b']) add(`division_zero_${mode}`, 'div', mode, '1', '0', 10, 5, 4);
    const math = fs.readFileSync(path.join(__dirname, 'references', 'decimal_math_references.tsv'), 'utf8').split(/\r?\n/).filter(line => line && !line.startsWith('#'));
    // The extreme-pole family is a strict diagnostic in the full matrix: current
    // fixed guards lose digits there. It is never counted as verified or given
    // a tolerance. CI exercises the nearer, well-resolved pole case instead.
    rows.push(...math.filter(line => !quick || Number(line.split('\t')[5]) <= 100 && !line.startsWith('tan_near_pole_')));
    for (const name of ['exp_ordinary_p100', 'sqrt_ordinary_p100', 'log_ordinary_p100']) {
        const fields = math.find(line => line.startsWith(name + '\t')).split('\t');
        fields[0] += '_alias'; fields[2] = fields[1] === 'log' ? 'alias_b' : 'alias_a';
        rows.push(fields.join('\t'));
    }
    // Import the existing directed-rounding allowance explicitly; never apply
    // it to new cases or nearest modes. Count accepted differences separately.
    for (const line of fs.readFileSync(path.join(__dirname, '..', 'tests', 'transcendental_references.tsv'), 'utf8').split(/\r?\n/)) {
        if (!line || line.startsWith('#')) continue;
        const [op, a, b, digits, mode, expected] = line.split('\t');
        if (+mode >= 4 || expected.startsWith('ERROR')) continue;
        const [coefficient, scale] = parts(expected);
        const ulp = coefficient ? `1e${abs(coefficient).toString().length - scale - +digits}` : '0';
        rows.push([`legacy_${rows.length}_${op}`, op, 'reuse', a, b, digits, mode, 0, ulp, expected].join('\t'));
    }
    const names = rows.map(row => row.split('\t')[0]);
    if (new Set(names).size !== names.length) throw Error('Duplicate decimal benchmark names');
    return rows;
}
module.exports = {makeCases, reference};
