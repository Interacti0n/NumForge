function initCalculator()
{
const lifecycle = new AbortController();
const form = document.querySelector('#calculator');
const expression = document.querySelector('#expression');
const result = document.querySelector('#result');
const resultApprox = document.querySelector('#result-approx');
const resultMoreMarker = document.querySelector('#result-more-marker');
const expandResult = document.querySelector('#expand-result');
const resultDialog = document.querySelector('#result-dialog');
const resultFull = document.querySelector('#result-full');
const resultApproxDialog = document.querySelector('#result-approx-dialog');
const closeResultDialog = document.querySelector('#close-result-dialog');
const copyResult = document.querySelector('#copy-result');
const copyResultDialog = document.querySelector('#copy-result-dialog');
const copyButtons = [copyResult, copyResultDialog];
const precision = document.querySelector('#precision');
const precisionMode = document.querySelector('#precision-mode');
const notationMode = document.querySelector('#notation-mode');
const complexForm = document.querySelector('#complex-form');
const angleButtons = [...document.querySelectorAll('[data-angle]')];
const english = document.documentElement.lang === 'en';
let generation = 0;
let controller = null;
let copyTimer = null;
let autoTimer = null;
let resultExpanded = false;
let resultCopy = '';
let resultApproxValue = '';
let angleUnit = 'rad';
const settingsOverview = document.querySelector('#settings-overview');
const mobileSettings = window.matchMedia('(max-width: 600px), (max-width: 960px) and (max-height: 500px) and (orientation: landscape)');
function syncSettingsPlacement()
{
    const settings = document.querySelector('.precision');
    const parent = document.querySelector(mobileSettings.matches ? '.sidebar' : '.calculator-column');
    if (settings.parentElement && settings.parentElement !== parent)
    {
        if (mobileSettings.matches) parent.append(settings);
        else parent.prepend(settings);
    }
}
function updateSettingsOverview()
{
    const label = precisionMode.value === 'custom' ? precision.value
        : precisionMode.value === 'full' ? (english ? 'Full' : 'Plný') : 'Auto';
    settingsOverview.textContent = label + ' · ' + angleUnit.toUpperCase();
}
mobileSettings.addEventListener?.('change', syncSettingsPlacement, {signal: lifecycle.signal});
syncSettingsPlacement();

function setResultApproximation(value)
{
    resultApproxValue = typeof value === 'string' ? value : '';
    const display = resultApproxValue ? '≈ ' + resultApproxValue : '';
    resultApprox.textContent = display;
    resultApprox.hidden = !display;
    resultApproxDialog.textContent = display;
    resultApproxDialog.hidden = !display;
}

function updateResultExpansion()
{
    if (resultDialog.open)
    {
        resultFull.textContent = result.textContent;
    }
    const overflowing = result.scrollHeight > result.clientHeight + 1;
    resultMoreMarker.hidden = !overflowing || resultExpanded;
    expandResult.style.display = overflowing ? 'inline-block' : 'none';
    expandResult.hidden = !overflowing;
    if (!overflowing)
    {
        if (resultDialog.open) resultDialog.close();
        resultExpanded = false;
        result.className = result.className.replace(' expanded', '');
    }
}

function toggleResultExpansion()
{
    if (expandResult.hidden)
    {
        return;
    }
    if (window.matchMedia('(min-width: 961px)').matches)
    {
        resultFull.textContent = result.textContent;
        if (!resultDialog.open) resultDialog.showModal();
        return;
    }
    resultExpanded = !resultExpanded;
    result.className =
        result.className.replace(' expanded', '') + (resultExpanded ? ' expanded' : '');
    resultMoreMarker.hidden = resultExpanded;
    expandResult.textContent = resultExpanded ? text.showLess : text.showAll;
}

function updateExpressionOverflow()
{
    const style = getComputedStyle(expression);
    const lineHeight = parseFloat(style.lineHeight);
    const frame = parseFloat(style.paddingTop) + parseFloat(style.paddingBottom) +
        parseFloat(style.borderTopWidth) + parseFloat(style.borderBottomWidth);
    const minimum = Math.ceil(lineHeight + frame);
    const maximum = Math.ceil(5 * lineHeight + frame);
    expression.style.height = '0px';
    const content = expression.value ? expression.scrollHeight +
        parseFloat(style.borderTopWidth) + parseFloat(style.borderBottomWidth) : minimum;
    expression.style.height = Math.min(maximum, Math.max(minimum, content)) + 'px';
}

function invalidate()
{
    generation++;
    controller?.abort();
    clearTimeout(copyTimer);
    clearTimeout(autoTimer);
    if (resultDialog.open) resultDialog.close();
    resultExpanded = false;
    expandResult.textContent = text.showAll;
    result.textContent = '';
    setResultApproximation('');
    resultCopy = '';
    result.className = '';
    resultMoreMarker.hidden = true;
    expandResult.hidden = true;
    expandResult.style.display = 'none';
    copyButtons.forEach(button => { button.disabled = true; button.textContent = text.copy; });
}

const text = english ? {
    calculating : 'Calculating…',
    precision : 'Enter a non-negative whole number of decimal places.',
    failure : 'Calculation failed.',
    error : 'Error: ',
    position : 'position ', near : ' near ', end : ' at the end of the expression',
    copy : '⧉ Copy',
    copied : '✓ Copied',
    showAll : 'Show all...',
    showLess : 'Show less'
}
: {
    calculating : 'Počítam…',
    precision : 'Zadaj nezáporný celý počet desatinných miest.',
    failure : 'Výpočet zlyhal.',
    error : 'Chyba: ',
    position : 'pozícia ', near : ' pri ', end : ' na konci výrazu',
    copy : '⧉ Kopírovať',
    copied : '✓ Skopírované',
    showAll : 'Zobraziť všetko...',
    showLess : 'Zobraziť menej'
};
const slovakStatus = {
    'invalid quantity operation' : 'neplatná operácia s veličinami',
    'unknown unit' : 'neznáma jednotka',
    'incompatible units' : 'nekompatibilné jednotky',
    'variable is undefined' : 'premenná nie je definovaná',
    'ans is undefined' : 'ans ešte nemá potvrdenú hodnotu',
    'stale session request' : 'zastaraná požiadavka sedenia',
    'session expired; reload the page' : 'sedenie skončilo; obnov stránku',
    'null argument' : 'chýbajúci argument',
    'out of memory' : 'nedostatok pamäte',
    'invalid argument' : 'neplatný argument',
    'invalid token' : 'neplatný token',
    'syntax error' : 'syntaktická chyba',
    'division by zero' : 'delenie nulou',
    'value too large' : 'príliš veľká hodnota',
    'scale overflow' : 'pretečenie mierky',
    'TLE: time limit exceeded' : 'TLE: prekročený časový limit',
    'not implemented' : 'funkcia nie je implementovaná',
    'wrong number of arguments' : 'nesprávny počet argumentov'
};

function selectAngleUnit(unit, recalculate)
{
    angleUnit = unit === 'deg' ? 'deg' : 'rad';
    updateSettingsOverview();
    angleButtons.forEach((button) => {
        const selected = button.dataset.angle === angleUnit;
        button.classList.toggle('active', selected);
        button.setAttribute('aria-pressed', selected ? 'true' : 'false');
    });
    try { localStorage.setItem('numforge-angle-unit', angleUnit); } catch (_) {}
    if (recalculate) scheduleCalculation(0);
}

try { angleUnit = localStorage.getItem('numforge-angle-unit') || 'rad'; } catch (_) {}
selectAngleUnit(angleUnit, false);
angleButtons.forEach((button) => button.addEventListener('click', () =>
    selectAngleUnit(button.dataset.angle, true)));

function responseError(data)
{
    if (!data.status)
    {
        return data.error || text.failure;
    }
    const status = english ? data.status : (slovakStatus[data.status] || data.status);
    const located = ['invalid quantity operation', 'unknown unit', 'incompatible units', 'invalid argument', 'invalid token', 'syntax error', 'variable is undefined',
                     'division by zero', 'wrong number of arguments'];
    const chars = [...expression.value];
    if (!located.includes(data.status) || !Number.isInteger(data.column) ||
        data.column < 1 || data.column > chars.length + 1) return status;
    const index = data.column - 1;
    const position = ' (' + text.position + data.column + ')';
    if (index === chars.length) return status + text.end + position;
    let end = index + 1;
    if (/[a-zA-Z]/.test(chars[index]))
        while (end < chars.length && /[a-zA-Z]/.test(chars[end])) end++;
    end = Math.min(end, index + 24);
    const left = Math.max(0, index - 8), right = Math.min(chars.length, end + 8);
    const excerpt = (left ? '…' : '') + chars.slice(left, index).join('') +
        '⟦' + chars.slice(index, end).join('') + '⟧' +
        chars.slice(end, right).join('') + (right < chars.length ? '…' : '');
    return status + text.near + '“' + excerpt + '”' + position +
        domainHint(data.status, chars.slice(index, end).join(''));
}

function scheduleCalculation(delay = 300)
{
    updateExpressionOverflow();
    invalidate();
    if (!expression.value.trim())
    {
        return;
    }
    const id = generation;
    autoTimer = setTimeout(() => {
        if (id === generation)
        {
            calculate(false);
        }
    }, delay);
}

expression.addEventListener('input', () => scheduleCalculation());

precision.addEventListener('input', () => { updateSettingsOverview(); scheduleCalculation(); });
const inverseFunctionAliases = {
    asin: ['arcsin', 'arcussin'], acos: ['arccos', 'arcuscos'],
    atan: ['arctan', 'arcustan'], asinh: ['arcsinh', 'arcussinh'],
    acosh: ['arccosh', 'arcuscosh'], atanh: ['arctanh', 'arcustanh']
};
// Library search terms; insertion always uses the supported canonical function.
const functionSearchAliases = {
    abs: ['absolute', 'modulus', 'magnitude', 'absolútna hodnota'],
    sign: ['sgn', 'signum', 'znamienko'],
    min: ['minimum', 'smallest', 'najmenšia hodnota'],
    max: ['maximum', 'largest', 'najväčšia hodnota'],
    sum: ['summation', 'total', 'suma', 'súčet'],
    product: ['prod', 'multiplication', 'súčin', 'násobenie'],
    mean: ['avg', 'average', 'arithmetic mean', 'priemer', 'aritmetický priemer'],
    floor: ['round down', 'dolná celá časť', 'zaokrúhliť nadol'],
    ceil: ['ceiling', 'round up', 'horná celá časť', 'zaokrúhliť nahor'],
    trunc: ['truncate', 'truncation', 'integer part', 'odrezanie', 'celá časť'],
    round: ['rounding', 'half even', 'bankers rounding', 'zaokrúhlenie', 'bankové zaokrúhľovanie'],
    gcd: ['gcf', 'hcf', 'greatest common factor', 'highest common factor', 'NSD', 'najväčší spoločný deliteľ'],
    lcm: ['least common multiple', 'lowest common multiple', 'NSN', 'najmenší spoločný násobok'],
    mod: ['modulo', 'remainder', 'zvyšok po delení'],
    npr: ['permutation', 'permutations', 'variation', 'variations', 'permutácie', 'variácie bez opakovania'],
    ncr: ['combination', 'combinations', 'binomial coefficient', 'choose', 'kombinácie', 'binomický koeficient', 'kombinačné číslo'],
    factorial: ['fact', 'factorial', 'faktoriál'],
    isqrt: ['integer square root', 'celočíselná odmocnina'],
    rand: ['random', 'rng', 'random number', 'náhodné číslo', 'náhodná hodnota'],
    pow: ['power', 'exponentiation', 'mocnina', 'umocnenie'],
    complex: ['complex number','imaginary','cartesian','komplexné číslo','imaginárne'],
    re: ['real part','real component','reálna časť','reálna zložka'],
    im: ['imaginary part','imaginary component','imaginárna časť','imaginárna zložka'],
    conj: ['conjugate','complex conjugate','komplexne združené číslo','združené'],
    arg: ['argument','phase','principal argument','fáza','argument komplexného čísla'],
    sqrt: ['square root', 'druhá odmocnina', 'kvadratická odmocnina'],
    cbrt: ['cube root', 'cubic root', 'tretia odmocnina', 'kubická odmocnina'],
    root: ['nth root', 'n-th root', 'n-tá odmocnina', 'všeobecná odmocnina'],
    exp: ['exponential', 'exponential function', 'exponenciálna funkcia'],
    ln: ['natural logarithm', 'loge', 'prirodzený logaritmus'],
    log: ['logarithm', 'log10', 'common logarithm', 'decimal logarithm', 'dekadický logaritmus', 'desiatkový logaritmus'],
    median: ['med', 'middle value', 'medián', 'prostredná hodnota'],
    geomean: ['gmean', 'geometric mean', 'geometric average', 'geometrický priemer'],
    harmean: ['hmean', 'harmonic mean', 'harmonic average', 'harmonický priemer'],
    variance: ['var', 'varp', 'population variance', 'populačný rozptyl'],
    stdevp: ['stdp', 'stddevp', 'population standard deviation', 'populačná smerodajná odchýlka'],
    stdev: ['std', 'stddev', 'sd', 'sample standard deviation', 'výberová smerodajná odchýlka', 'štandardná odchýlka'],
    sin: ['sine', 'sínus'],
    cos: ['cosine', 'kosínus'],
    tan: ['tg', 'tangent', 'tangens'],
    asin: ['arcsine', 'inverse sine', 'arc sine', 'arcus sínus', 'arkus sínus'],
    acos: ['arccosine', 'inverse cosine', 'arc cosine', 'arcus kosínus', 'arkus kosínus'],
    atan: ['arctg', 'arctangent', 'inverse tangent', 'arc tangent', 'arcus tangens', 'arkus tangens'],
    sinh: ['sh', 'hyperbolic sine', 'hyperbolický sínus'],
    cosh: ['ch', 'hyperbolic cosine', 'hyperbolický kosínus'],
    tanh: ['th', 'tgh', 'hyperbolic tangent', 'hyperbolický tangens'],
    asinh: ['arsinh', 'arsh', 'inverse hyperbolic sine', 'area hyperbolic sine', 'inverzný hyperbolický sínus', 'area sínus'],
    acosh: ['arcosh', 'arch', 'inverse hyperbolic cosine', 'area hyperbolic cosine', 'inverzný hyperbolický kosínus', 'area kosínus'],
    atanh: ['artanh', 'arth', 'artgh', 'inverse hyperbolic tangent', 'area hyperbolic tangent', 'inverzný hyperbolický tangens', 'area tangens'],
    radians: ['deg2rad', 'degrees to radians', 'stupne na radiány'],
    degrees: ['rad2deg', 'radians to degrees', 'radiány na stupne']
};
const functionHelp = {
    abs: ["|x|","abs(x)","Absolútna hodnota alebo veľkosť komplexného čísla.","Absolute value or complex magnitude."],
    sign: ["sign","sign(x)","Znamienko: -1, 0 alebo 1.","Sign: -1, 0 or 1."],
    min: ["min","min(x;y;...)","Minimum z 2 až 256 hodnôt.","Minimum of 2 to 256 values."],
    max: ["max","max(x;y;...)","Maximum z 2 až 256 hodnôt.","Maximum of 2 to 256 values."],
    sum: ["sum","sum(x;y;...)","Presný súčet 1 až 256 hodnôt.","Exact sum of 1 to 256 values."],
    product: ["product","product(x;y;...)","Presný súčin 1 až 256 hodnôt.","Exact product of 1 to 256 values."],
    mean: ["mean","mean(x;y;...)","Aritmetický priemer 1 až 256 hodnôt.","Arithmetic mean of 1 to 256 values."],
    floor: ["⌊x⌋","floor(x)","Zaokrúhlenie nadol na celé číslo.","Round down to an integer."],
    ceil: ["⌈x⌉","ceil(x)","Zaokrúhlenie nahor na celé číslo.","Round up to an integer."],
    trunc: ["trunc","trunc(x)","Odstránenie desatinnej časti smerom k nule.","Remove the fractional part toward zero."],
    round: ["round","round(x) / round(x;n)","Half-even na celé n miest; n môže byť záporné.","Half-even to integer n places; n may be negative."],
    gcd: ["gcd","gcd(x;y)","Najväčší spoločný deliteľ celých čísel.","Greatest common divisor of integers."],
    lcm: ["lcm","lcm(x;y)","Najmenší spoločný násobok celých čísel.","Least common multiple of integers."],
    mod: ["mod","mod(x;y)","Zvyšok celočíselného delenia; y ≠ 0.","Integer division remainder; y ≠ 0."],
    npr: ["nPr","npr(n;r)","Permutácie bez opakovania; celé 0 ≤ r ≤ n.","Permutations without repetition; integers 0 ≤ r ≤ n."],
    ncr: ["nCr","ncr(n;r)","Kombinácie bez opakovania; celé 0 ≤ r ≤ n.","Combinations without repetition; integers 0 ≤ r ≤ n."],
    factorial: ["n!","factorial(n)","Celé n od 0 do 10000.","Integer n from 0 to 10000."],
    isqrt: ["isqrt","isqrt(n)","Celá časť odmocniny; celé n ≥ 0.","Integer square root; integer n ≥ 0."],
    rand: ["rand","rand() / rand(x) / rand(x;y)","Náhodná hodnota v [0,1), [0,x) alebo [x,y); x > 0 a x < y.","Random value in [0,1), [0,x) or [x,y); x > 0 and x < y."],
    pow: ["xʸ","pow(x;n)","Reálny exponent musí byť celý; komplexné operandy prijímajú aj hlavné neceločíselné a komplexné mocniny. Nula na záporný alebo nereálny exponent nie je definovaná.","Real exponents must be integers; complex operands also accept principal noninteger and complex powers. Zero to a negative or nonreal exponent is undefined."],
    complex: ["a + bi","complex(re;im)","Reálna a imaginárna zložka. Funguje aj a + b*i; i je imaginárna jednotka.","Real and imaginary components. Also accepts a + b*i; i is the imaginary unit."],
    re: ["Re","re(z)","Reálna časť; presné zlomky zostávajú presné.","Real part; exact fractions stay exact."],
    im: ["Im","im(z)","Imaginárna časť; pre reálne číslo vráti 0.","Imaginary part; returns 0 for a real number."],
    conj: ["conj","conj(z)","Komplexne združené číslo: a + bi → a − bi.","Complex conjugate: a + bi → a − bi."],
    arg: ["arg","arg(z)","Hlavný argument v radiánoch (−π, π]; pre nulu nie je definovaný.","Principal argument in radians (−π, π]; undefined for zero."],
    sqrt: ["√x","sqrt(x)","Hlavná odmocnina. Reálne x ≥ 0; komplexné číslo zadaj napríklad ako -1+0i.","Principal square root. Real x ≥ 0; enter complex values explicitly, for example -1+0i."],
    cbrt: ["∛x","cbrt(x)","Tretia odmocnina aj zo záporného čísla.","Cube root, including negative numbers."]
};
Object.assign(functionHelp, {
    root: ["ⁿ√x","root(x;n)","Celé n od 1 do 10000; záporné x iba pre nepárne n.","Integer n from 1 to 10000; negative x requires odd n."],
    exp: ["eˣ","exp(x)","Eulerovo číslo umocnené na reálny alebo komplexný exponent; polárny uhol je v radiánoch.","Euler’s number raised to a real or complex exponent; polar angle is in radians."],
    ln: ["ln","ln(x)","Prirodzený logaritmus. Reálne x > 0; komplexný vstup používa hlavnú vetvu v radiánoch, nula nie je povolená.","Natural logarithm. Real x > 0; complex input uses the principal branch in radians, zero is invalid."],
    log: ["log","log(x) / log(x;y)","Základ 10 alebo y. Reálne: x > 0, y > 0 a y ≠ 1. Komplexné: ln(x)/ln(y); x ≠ 0, y ≠ 0,1.","Base 10 or y. Real: x > 0, y > 0 and y ≠ 1. Complex: ln(x)/ln(y); x ≠ 0, y ≠ 0,1."],
    median: ["median","median(x;y;...)","Presný medián 1 až 256 hodnôt.","Exact median of 1 to 256 values."],
    geomean: ["geomean","geomean(x;y;...)","Geometrický priemer nezáporných hodnôt.","Geometric mean of non-negative values."],
    harmean: ["harmean","harmean(x;y;...)","Harmonický priemer kladných hodnôt.","Harmonic mean of positive values."],
    variance: ["variance","variance(x;y;...)","Populačný rozptyl; 1 až 256 hodnôt.","Population variance; 1 to 256 values."],
    stdevp: ["stdevp","stdevp(x;y;...)","Populačná smerodajná odchýlka; delí n.","Population standard deviation; divides by n."],
    stdev: ["stdev","stdev(x;y;...)","Výberová smerodajná odchýlka; aspoň 2 hodnoty, delí n−1.","Sample standard deviation; at least 2 values, divides by n−1."],
    sin: ["sin","sin(x)","Sínus; reálny vstup podľa RAD/DEG, komplexný vždy v radiánoch.","Sine; real input follows RAD/DEG, complex input always uses radians."],
    cos: ["cos","cos(x)","Kosínus; reálny vstup podľa RAD/DEG, komplexný vždy v radiánoch.","Cosine; real input follows RAD/DEG, complex input always uses radians."],
    tan: ["tan","tan(x)","Tangens; reálny vstup RAD/DEG, komplexný v radiánoch. V póloch nedefinovaný; blízko pólov je citlivý na chyby.","Tangent; real input follows RAD/DEG, complex input uses radians. Undefined at poles; sensitive to errors near poles."],
    asin: ["asin","asin(x)","Inverzný sínus: reálne -1 ≤ x ≤ 1 v RAD/DEG; komplexné vstupy vracajú hlavnú vetvu v radiánoch.","Inverse sine: real -1 ≤ x ≤ 1 in RAD/DEG; complex inputs return the principal branch in radians."],
    acos: ["acos","acos(x)","Inverzný kosínus: reálne -1 ≤ x ≤ 1 v RAD/DEG; komplexné vstupy vracajú hlavnú vetvu v radiánoch.","Inverse cosine: real -1 ≤ x ≤ 1 in RAD/DEG; complex inputs return the principal branch in radians."],
    atan: ["atan","atan(x)","Inverzný tangens: reálne vstupy v RAD/DEG, komplexné v radiánoch; ±i nie sú definované.","Inverse tangent: real inputs in RAD/DEG, complex inputs in radians; ±i are undefined."],
    sinh: ["sinh","sinh(x)","Hyperbolický sínus; aj komplexné vstupy, imaginárny uhol v radiánoch. Nezávisí od RAD/DEG.","Hyperbolic sine; also complex inputs, imaginary angle in radians. Independent of RAD/DEG."],
    cosh: ["cosh","cosh(x)","Hyperbolický kosínus; aj komplexné vstupy, imaginárny uhol v radiánoch. Nezávisí od RAD/DEG.","Hyperbolic cosine; also complex inputs, imaginary angle in radians. Independent of RAD/DEG."],
    tanh: ["tanh","tanh(x)","Hyperbolický tangens; aj komplexné vstupy v radiánoch. Pri imaginárnych póloch je citlivý na chyby.","Hyperbolic tangent; also complex inputs in radians. Sensitive to errors near imaginary-axis poles."],
    asinh: ["asinh","asinh(x)","Inverzný hyperbolický sínus.","Inverse hyperbolic sine."],
    acosh: ["acosh","acosh(x)","Inverzný hyperbolický kosínus; x ≥ 1.","Inverse hyperbolic cosine; x ≥ 1."],
    atanh: ["atanh","atanh(x)","Inverzný hyperbolický tangens; -1 < x < 1.","Inverse hyperbolic tangent; -1 < x < 1."],
    radians: ["° → rad","radians(x)","Prevod stupňov na radiány nezávisle od režimu.","Convert degrees to radians regardless of mode."],
    degrees: ["rad → °","degrees(x)","Prevod radiánov na stupne nezávisle od režimu.","Convert radians to degrees regardless of mode."]
});
let functionRegistry = new Map();
function describeFunction(name)
{
    const canonical = functionRegistry.get(name)?.canonical || Object.keys(inverseFunctionAliases).find(key =>
        inverseFunctionAliases[key].includes(name)) || name;
    const info = functionHelp[canonical];
    if (!info) return '';
    const registeredAliases = [...functionRegistry.values()].filter(entry => entry.canonical === canonical && entry.name !== canonical).map(entry => entry.name);
    const aliases = registeredAliases.length ? registeredAliases : inverseFunctionAliases[canonical];
    return info[1] + (aliases ? ' · ' + aliases.map(alias => alias + '(x)').join(', ') : '') +
        ': ' + info[english ? 3 : 2];
}
function domainHint(status, name)
{
    if (!['invalid argument', 'wrong number of arguments'].includes(status)) return '';
    const hint = describeFunction(name);
    return hint ? ' — ' + hint : '';
}
const helpButtons = [...document.querySelectorAll('[data-function]')];
if (helpButtons.length)
{
    const help = document.createElement('p');
    help.id = 'function-help';
    help.setAttribute('aria-live', 'polite');
    help.textContent = english ? 'Choose a function to see its arguments. Separate arguments with ;.'
                               : 'Vyber funkciu pre opis argumentov. Argumenty oddeľ bodkočiarkou ;.';
    document.querySelector('.function-groups').after(help);
    helpButtons.forEach((button) => {
        const name = button.dataset.function;
        const info = functionHelp[name];
        if (!info) return;
        button.textContent = info[0];
        button.title = describeFunction(name);
        button.setAttribute('aria-label', describeFunction(name));
        button.setAttribute('aria-describedby', 'function-help');
        ['mouseenter', 'focus', 'click'].forEach((event) =>
            button.addEventListener(event, () => { help.textContent = describeFunction(name); }));
    });
}
let cacheClient = '';
if (typeof crypto !== 'undefined' && crypto.getRandomValues)
{
    const bytes = crypto.getRandomValues(new Uint8Array(16));
    cacheClient = [...bytes].map(value => value.toString(16).padStart(2, '0')).join('');
    try {
        const saved=sessionStorage.getItem('numforge-client-id');
        if (/^[0-9a-f]{32}$/.test(saved)) cacheClient=saved;
        sessionStorage.setItem('numforge-client-id',cacheClient);
    } catch (_) {}
}
let sessionReady = null;
let sessionStarted = false;
let pendingCommit = null;
let commitInFlight = false;
const historyEntries = [];
const storedVariables = new Map();
let historySequence = 0;
let variableDeletionInFlight = false;
let pendingVariableDeletion = null;
let activeSessionTab = 'history';
const variableStatus = document.createElement('p');
variableStatus.id = 'variable-status';
variableStatus.setAttribute('role', 'status');
document.querySelector('.session-panel').append(variableStatus);

function selectSessionTab(name, focus = false)
{
    activeSessionTab = name;
    for (const section of ['history', 'variables']) {
        const selected = section === name;
        const tab = document.querySelector('#session-' + section + '-tab');
        tab.setAttribute('aria-selected', String(selected));
        tab.tabIndex = selected ? 0 : -1;
        document.querySelector('#session-' + section + '-panel').hidden = !selected;
        if (selected && focus) tab.focus();
    }
    updateHistoryOverflow();
}
document.querySelectorAll('.session-tabs [role="tab"]').forEach(tab => {
    tab.addEventListener('click', () => selectSessionTab(tab.id.includes('variables') ? 'variables' : 'history'));
    tab.addEventListener('keydown', event => {
        if (['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) {
            event.preventDefault();
            selectSessionTab(event.key === 'Home' ? 'history' : event.key === 'End' ? 'variables'
                : activeSessionTab === 'history' ? 'variables' : 'history', true);
        }
    });
});

async function deleteVariable(name)
{
    if (commitInFlight || pendingCommit || variableDeletionInFlight) return;
    if (pendingVariableDeletion && pendingVariableDeletion.name !== name) return;
    variableDeletionInFlight = true;
    let finishDeletion;
    const deletionFinished = new Promise(resolve => { finishDeletion = resolve; });
    window.numforgePendingVariableDeletion = deletionFinished;
    clearTimeout(autoTimer);
    controller?.abort();
    const deletion = pendingVariableDeletion || {name, id: ++generation};
    pendingVariableDeletion = deletion;
    try {
        if (!sessionStarted) await ensureSession();
        const {response, data} = await requestEvaluation('/api/evaluate?precision=10&angle=rad', {
            method: 'POST', headers: {'Content-Type': 'text/plain; charset=utf-8'}, body: name,
            signal: AbortSignal.timeout(10000)
        }, 'delete-variable', deletion.id);
        if (!response.ok || !data.ok) {
            pendingVariableDeletion = null;
            throw new Error(responseError(data));
        }
        await refreshSession();
        pendingVariableDeletion = null;
        renderVariables();
        document.querySelector('#variable-status').textContent = english ? 'Variable deleted.' : 'Premenná odstránená.';
        document.querySelector('#session-variables-tab').focus();
    } catch (error) {
        document.querySelector('#variable-status').textContent = error.message + (pendingVariableDeletion
            ? (english ? ' Click delete again to retry, or start a new session.' : ' Opakuj kliknutie na odstránenie alebo začni nové sedenie.') : '');
    } finally {
        variableDeletionInFlight = false;
        if (window.numforgePendingVariableDeletion === deletionFinished)
            window.numforgePendingVariableDeletion = null;
        finishDeletion();
        if (!pendingVariableDeletion && expression.value.trim()) scheduleCalculation(0);
    }
}

function renderVariables()
{
    const list = document.querySelector('#variable-list');
    list.replaceChildren();
    document.querySelector('#variable-count').textContent = storedVariables.size + ' / 32';
    document.querySelector('#variables-empty').hidden = storedVariables.size > 0;
    [...storedVariables].sort(([a], [b]) => a.localeCompare(b)).forEach(([name, value]) => {
        const item = document.createElement('li');
        const button = document.createElement('button');
        button.type = 'button';
        const label = document.createElement('strong');
        label.className = 'variable-name';
        label.textContent = name;
        const display = document.createElement('span');
        display.className = 'variable-value';
        display.textContent = '= ' + value;
        button.title = name + ' = ' + value;
        button.setAttribute('aria-label', (english ? 'Insert variable ' : 'Vložiť premennú ') + name);
        let caret = null;
        button.addEventListener('pointerdown', () => {
            caret = document.activeElement === expression
                ? {start: expression.selectionStart, end: expression.selectionEnd} : null;
        });
        button.addEventListener('click', () => {
            const position = caret || {start: expression.value.length, end: expression.value.length};
            expression.setSelectionRange(position.start, position.end);
            insertText(name);
            caret = null;
        });
        button.append(label, display);
        const remove = document.createElement('button');
        remove.type = 'button';
        remove.className = 'variable-delete';
        remove.textContent = '×';
        remove.title = (english ? 'Delete variable ' : 'Odstrániť premennú ') + name;
        remove.setAttribute('aria-label', remove.title);
        remove.addEventListener('click', () => deleteVariable(name));
        item.append(button, remove);
        list.append(item);
    });
}

function updateHistoryOverflow()
{
    document.querySelectorAll('#history-list .history-line').forEach(line => {
        line.classList.remove('is-truncated');
        line.classList.toggle('is-truncated', line.scrollWidth > line.clientWidth + 1);
    });
}

function renderHistory()
{
    const list = document.querySelector('#history-list');
    list.replaceChildren();
    historyEntries.forEach((entry, index) => {
        const item = document.createElement('li');
        const number = historySequence - historyEntries.length + index + 1;
        const marker = document.createElement('span');
        marker.className = 'history-index';
        marker.textContent = '#' + number;
        const button = document.createElement('button');
        button.type = 'button';
        button.className = 'history-entry';
        const line = document.createElement('span');
        line.className = 'history-line';
        const input = document.createElement('span');
        input.className = 'history-value';
        input.textContent = entry.input;
        const separator = document.createElement('span');
        separator.className = 'history-separator';
        separator.textContent = ' → ';
        const result = document.createElement('span');
        result.className = 'history-value';
        result.textContent = entry.display;
        line.append(input, separator, result);
        button.append(line);
        button.title = '#' + number + ' · ' + entry.input + ' → ' + entry.display;
        button.disabled = entry.copy === null;
        let caret = null;
        button.addEventListener('pointerdown', () => {
            caret = document.activeElement === expression
                ? {start: expression.selectionStart, end: expression.selectionEnd} : null;
        });
        button.addEventListener('click', () => {
            const position = caret || {start: expression.value.length, end: expression.value.length};
            expression.setSelectionRange(position.start, position.end);
            insertText(entry.copy || entry.display);
            caret = null;
        });
        const copy = document.createElement('button');
        copy.type = 'button';
        copy.className = 'history-copy';
        copy.disabled = entry.copy === null;
        copy.textContent = '⧉';
        copy.title = english ? 'Copy result' : 'Skopírovať výsledok';
        copy.setAttribute('aria-label', copy.title);
        copy.addEventListener('click', async () => {
            try
            {
                await copyText(entry.copy || entry.display);
                copy.textContent = '✓';
                setTimeout(() => { copy.textContent = '⧉'; }, 1400);
            }
            catch (_) { copy.textContent = '⧉'; }
        });
        item.append(marker, button, copy);
        list.append(item);
    });
    updateHistoryOverflow();
    list.scrollTop = list.scrollHeight;
}

let pendingReset = null, lifecycleInFlight = false;
async function mutateSession(action)
{
    if (commitInFlight || variableDeletionInFlight || lifecycleInFlight) return;
    if (pendingReset && pendingReset.action !== action) return;
    clearTimeout(autoTimer); controller?.abort();
    lifecycleInFlight = true;
    let settleLifecycle;
    const lifecycleFinished = new Promise(resolve => { settleLifecycle = resolve; });
    window.numforgePendingLifecycle = lifecycleFinished;
    try {
        if (action !== 'reset') await ensureSession();
        pendingReset ||= {action, id: ++generation};
        const response = await fetch('/api/session?client=' + cacheClient + '&revision=' + pendingReset.id + '&action=' + pendingReset.action, {
            method: 'POST', body: '', signal: AbortSignal.timeout(10000)
        });
        const data = await response.json();
        if (!response.ok || !data.ok) {
            pendingReset = null;
            if (action === 'reset' && data.status === 'session expired; reload the page') {
                sessionStorage.removeItem('numforge-client-id');
                sessionStorage.removeItem('numforge-navigation-state');
                window.location.reload();return;
            }
            throw new Error(responseError(data));
        }
        pendingReset = null; pendingCommit = null; pendingVariableDeletion = null;
        if (action === 'reset') {
            try {
                for (const key of ['numforge-navigation-state','numforge-units-state','numforge-units-pending']) sessionStorage.removeItem(key);
            } catch (_) {}
            window.location.reload();
        } else {
            await refreshSession();
            variableStatus.textContent = english ? 'History cleared; ans and variables preserved.' : 'História vymazaná; ans a premenné zostali zachované.';
        }
    } catch (error) {
        variableStatus.textContent = error.message + (pendingReset ? (english ? ' Click the same action again to retry.' : ' Opakuj tú istú akciu kliknutím.') : '');
    } finally {
        lifecycleInFlight = false; settleLifecycle();
        if (window.numforgePendingLifecycle === lifecycleFinished) window.numforgePendingLifecycle = null;
    }
}
document.querySelector('#reset-session').addEventListener('click', () => mutateSession('reset'));
document.querySelector('#clear-history').addEventListener('click', () => mutateSession('clear-history'));

let refreshGeneration = 0;
async function refreshSession()
{
    const refresh = ++refreshGeneration;
    for (let attempt = 0; attempt < 3; attempt++) {
        const read = async path => {
            const response = await fetch(path, {signal: lifecycle.signal});
            const data = await response.json();
            if (response.status === 409) throw new Error('snapshot_changed');
            if (!response.ok || !data.ok) throw new Error(responseError(data));
            return data;
        };
        try {
            const state = await read('/api/session?client=' + cacheClient);
            const revision = Number(state.revision);
            if (!Number.isSafeInteger(revision)) throw new Error('Invalid session revision');
            const lists = {};
            for (const kind of ['variables', 'history']) {
                const entries = []; let offset = 0;
                do {
                    const page = await read('/api/session/' + kind + '?client=' + cacheClient + '&limit=1&offset=' + offset + '&at=' + state.revision);
                    if (!Array.isArray(page.items)) throw new Error('Invalid session snapshot');
                    entries.push(...page.items); offset = page.next_offset;
                    if (refresh !== refreshGeneration || lifecycle.signal.aborted) return;
                } while (offset !== null);
                lists[kind] = entries;
            }
            if (refresh !== refreshGeneration || lifecycle.signal.aborted) return;
            generation = Math.max(generation, revision);
            document.querySelector('#clear-history').disabled = state.history_count === 0;
            storedVariables.clear();
            lists.variables.forEach(entry => storedVariables.set(entry.name, entry.display));
            historyEntries.splice(0, historyEntries.length, ...lists.history.map(entry => ({
                input: entry.expression, display: entry.display,
                copy: Object.hasOwn(entry, 'copy') ? entry.copy : entry.display.replace(/ × 10\^([+-]?\d+)/g, (_, exponent) =>
                    'E' + (/^[+-]/.test(exponent) ? exponent : '+' + exponent)),
                precision: String(entry.value.context.places === -1 ? 'full' : entry.value.context.places),
                angle: entry.value.context.angle, notation: entry.value.context.notation,
                complexForm: entry.value.complex_form || 'cartesian'
            })));
            historySequence = Number(state.history_sequence);
            renderVariables(); renderHistory();
            return;
        } catch (error) {
            if (error.message === 'snapshot_changed' && attempt < 2) continue;
            throw error;
        }
    }
}

async function loadFunctionRegistry()
{
    try {
        const response = await fetch('/api/functions', {signal: lifecycle.signal});
        const data = await response.json();
        if (!response.ok || !data.ok || !Array.isArray(data.functions)) throw new Error('Invalid function catalogue');
        const registry = new Map(data.functions.map(entry => [entry.name, entry]));
        functionRegistry = registry;
        for (const button of helpButtons) {
            const entry = registry.get(button.dataset.function);
            button.disabled = !entry?.implemented;
            if (entry) {
                button.dataset.minimumArguments = entry.minimum_arguments;
                button.dataset.maximumArguments = entry.maximum_arguments;
                button.dataset.canonical = entry.canonical;
            }
        }
    } catch (error) {
        if (!lifecycle.signal.aborted) variableStatus.textContent = english ? 'Function catalogue unavailable.' : 'Katalóg funkcií nie je dostupný.';
    }
}

async function ensureSession()
{
    if (!cacheClient) throw new Error(english ? 'Session unavailable. Reload the page.' : 'Sedenie nie je dostupné. Obnov stránku.');
    if (!sessionReady)
    {
        sessionReady = requestEvaluation('/api/evaluate?precision=10&angle=rad', {
            method: 'POST', headers: {'Content-Type': 'text/plain; charset=utf-8'}, body: ''
        }, 'start', 1).then(({response, data}) => {
            if (!response.ok || !data.ok) throw new Error(responseError(data));
            sessionStarted = true;
        }).catch(error => { sessionReady = null; throw error; });
    }
    return sessionReady;
}

async function requestEvaluation(url, options, action, revision)
{
    if (cacheClient) url += '&client=' + cacheClient + '&revision=' + revision + '&action=' + action;
    let response;
    try { response = await fetch(url, options); }
    catch (error)
    {
        if (error.name === 'AbortError') throw error;
        throw new Error(english
            ? 'Cannot contact the server. Check the connection and that the server is running, then press Enter to retry.'
            : 'Nepodarilo sa spojiť so serverom. Skontroluj spojenie a či server beží, potom opakuj stlačením Enter.');
    }
    const unexpected = english ? 'Unexpected server response. Press Enter to retry.'
                               : 'Neočakávaná odpoveď servera. Opakuj stlačením Enter.';
    if (!response.headers.get('content-type')?.includes('application/json'))
        throw new Error(unexpected);
    let data;
    try { data = await response.json(); }
    catch (error)
    {
        if (error.name === 'AbortError') throw error;
        throw new Error(unexpected);
    }
    if (!data || typeof data !== 'object' || typeof data.ok !== 'boolean' ||
        (data.ok && typeof data.result !== 'string'))
        throw new Error(unexpected);
    return {response, data};
}
const groups = [...document.querySelectorAll('details.function-group')];
if (groups.length)
{
    const search = document.querySelector('#function-search');
    const searchStatus = document.querySelector('#search-status');
    const functionGroups = document.querySelector('.function-groups');
    const normalizeSearch = value => value.toLocaleLowerCase().normalize('NFD')
        .replace(/[\u0300-\u036f]/g, '').replace(/[’']/g, '');
    const constantAliases = {
        'π': ['pi', 'constant', 'constants', 'Archimedes constant', "Archimedes' constant",
            "Ludolph's number", 'Ludolph number', 'Ludolf number', 'circular constant'],
        'e': ['e', 'constant', 'constants', 'Euler number', "Euler's number",
            'Napier constant', "Napier's constant",
            'Napier number', "Napier's number", 'natural logarithm base', 'base of natural logarithms'],
        'φ': ['phi', 'φ', 'ϕ', 'constant', 'constants', 'golden ratio', 'golden number',
            'golden mean', 'golden section', 'divine proportion']
    };
    if (!english)
    {
        constantAliases['π'].push('pí', 'konštanta', 'konštanty', 'Archimedova konštanta',
            'Archimedovo číslo', 'Ludolfovo číslo', 'Ludolphovo číslo', 'kruhová konštanta');
        constantAliases.e.push('konštanta', 'konštanty', 'Eulerovo číslo',
            'Napierovo číslo', 'Napierova konštanta', 'základ prirodzeného logaritmu',
            'základ prirodzených logaritmov');
        constantAliases['φ'].push('fí', 'fi', 'konštanta', 'konštanty', 'zlatý rez',
            'zlaté číslo', 'zlatý pomer', 'zlatá konštanta', 'božský pomer');
    }
    const tabs = document.createElement('div');
    tabs.className = 'function-tabs';
    tabs.setAttribute('role', 'tablist');
    tabs.setAttribute('aria-label', english ? 'Functions' : 'Funkcie');
    groups[0].before(tabs);
    let activeGroup = 0;
    function selectGroup(index)
    {
        activeGroup = index;
        search.value = '';
        tabs.hidden = false;
        functionGroups.classList.remove('searching');
        searchStatus.hidden = true;
        groups.forEach((group, i) => {
            group.hidden = i !== index;
            group.classList.remove('search-results');
            group.querySelectorAll('[data-insert]').forEach(button => { button.hidden = false; });
            tabs.children[i].setAttribute('aria-selected', String(i === index));
            tabs.children[i].tabIndex = i === index ? 0 : -1;
        });
    }
    function filterFunctions()
    {
        const query = normalizeSearch(search.value.trim());
        if (!query)
        {
            selectGroup(activeGroup);
            return;
        }
        tabs.hidden = true;
        functionGroups.classList.add('searching');
        let matches = 0;
        groups.forEach(group => {
            let groupMatches = 0;
            group.querySelectorAll('[data-insert]').forEach(button => {
                const info = functionHelp[button.dataset.function];
                const aliases = [...(inverseFunctionAliases[button.dataset.function] || []),
                    ...(functionSearchAliases[button.dataset.function] || []),
                    ...[...functionRegistry.values()].filter(entry => entry.canonical === button.dataset.function)
                        .map(entry => entry.name)];
                const constants = button.closest('.constants-library')
                    ? constantAliases[button.dataset.insert] || [] : [];
                const searchable = normalizeSearch([button.dataset.function, button.dataset.insert,
                    button.title, button.textContent, ...aliases, ...constants, ...(info || [])].join(' '));
                button.hidden = !searchable.includes(query);
                if (!button.hidden) groupMatches++;
            });
            group.hidden = groupMatches === 0;
            group.classList.toggle('search-results', groupMatches > 0);
            matches += groupMatches;
        });
        searchStatus.textContent = matches
            ? (english ? `${matches} matching functions` : `${matches} zodpovedajúcich funkcií`)
            : (english ? 'No matching function. Try another name.' : 'Žiadna funkcia sa nenašla. Skús iný názov.');
        searchStatus.hidden = false;
    }
    groups.forEach((group, i) => {
        const summary = group.querySelector('summary');
        const button = document.createElement('button');
        button.type = 'button';
        button.textContent = summary.textContent;
        button.id = 'function-tab-' + i;
        group.id = 'function-panel-' + i;
        button.setAttribute('role', 'tab');
        button.setAttribute('aria-controls', group.id);
        group.dataset.label = summary.textContent;
        group.setAttribute('role', 'tabpanel');
        group.setAttribute('aria-labelledby', button.id);
        group.open = true;
        summary.hidden = true;
        button.addEventListener('click', () => selectGroup(i));
        button.addEventListener('keydown', (event) => {
            let next = i;
            if (event.key === 'ArrowRight') next = (i + 1) % groups.length;
            else if (event.key === 'ArrowLeft') next = (i + groups.length - 1) % groups.length;
            else if (event.key === 'Home') next = 0;
            else if (event.key === 'End') next = groups.length - 1;
            else return;
            event.preventDefault();
            selectGroup(next);
            tabs.children[next].focus();
        });
        tabs.append(button);
    });
    search.addEventListener('input', filterFunctions);
    search.addEventListener('keydown', event => {
        if (event.key === 'Escape')
        {
            search.value = '';
            filterFunctions();
        }
    });
    selectGroup(0);
}
expression.addEventListener('keydown', (event) => {
    if (event.key === 'Enter' && !event.isComposing && !event.shiftKey)
    {
        event.preventDefault();
        expression.setSelectionRange(expression.value.length, expression.value.length);
        form.requestSubmit();
    }
});
updateExpressionOverflow();
window.addEventListener?.('resize', () => {
    updateExpressionOverflow();
    updateResultExpansion();
    updateHistoryOverflow();
    updateRecentLayout();
}, {signal: lifecycle.signal});
function insertText(text, isFunction = false)
{
    invalidate();
    const start = expression.selectionStart ?? expression.value.length;
    const end = expression.selectionEnd ?? start;
    const insertion = isFunction ? text + ')' : text;
    expression.setRangeText(insertion, start, end, 'end');
    if (isFunction) expression.setSelectionRange(start + text.length, start + text.length);
    expression.focus();
    scheduleCalculation();
}

function eraseText()
{
    invalidate();
    const start = expression.selectionStart ?? expression.value.length;
    const end = expression.selectionEnd ?? start;
    if (start !== end)
    {
        expression.setRangeText('', start, end, 'end');
    }
    else if (start > 0)
    {
        expression.setRangeText('', start - 1, start, 'end');
    }
    expression.focus();
    scheduleCalculation();
}

async function copyText(value)
{
    if (navigator.clipboard && window.isSecureContext)
    {
        await navigator.clipboard.writeText(value);
        return;
    }
    const temporary = document.createElement('textarea');
    temporary.value = value;
    temporary.style.position = 'fixed';
    temporary.style.opacity = '0';
    document.body.append(temporary);
    temporary.select();
    const copied = document.execCommand('copy');
    temporary.remove();
    if (!copied)
    {
        throw new Error('copy failed');
    }
}

const recentButtons = document.querySelector('#recent-buttons');
const recentLimit = 12;
let recentItems = [];
function insertFromButton(button)
{
    const isFunction = Boolean(button.dataset.function) && button.dataset.insert !== 'rand()';
    insertText(button.dataset.insert, isFunction);
    if (!button.closest?.('.constants, .operations, .functions')) return;
    const item = {key: button.dataset.insert, label: button.dataset.function || button.textContent.trim(),
        insert: button.dataset.insert, isFunction, title: button.title};
    rememberRecent(item);
}
function rememberRecent(item)
{
    recentItems = [item, ...recentItems.filter(previous => previous.key !== item.key)].slice(0, recentLimit);
    renderRecent();
}
function updateRecentLayout()
{
    const width = recentButtons.clientWidth;
    if (!width) return;
    const buttons = [...recentButtons.children];
    const gap = parseFloat(getComputedStyle(recentButtons).columnGap) || 0;
    buttons.forEach(button => {
        button.hidden = false;
        button.style.flex = '0 0 auto';
    });
    let used = 0;
    let full = false;
    buttons.forEach((button, index) => {
        const needed = button.offsetWidth + (index ? gap : 0);
        if (full || (index && used + needed > width - 1))
        {
            button.hidden = true;
            full = true;
        }
        else used += needed;
        button.style.flex = '';
    });
}
function renderRecent()
{
    recentButtons.replaceChildren(...recentItems.map(recent => {
        const button = document.createElement('button');
        button.type = 'button';
        button.textContent = recent.label;
        button.title = recent.title || recent.label;
        button.addEventListener('click', () => {
            insertText(recent.insert, recent.isFunction);
            rememberRecent(recent);
        });
        return button;
    }));
    updateRecentLayout();
}
if (typeof ResizeObserver !== 'undefined')
    new ResizeObserver(updateRecentLayout).observe(recentButtons);
document.querySelectorAll('[data-insert]')
    .forEach(button => button.addEventListener('click', () => insertFromButton(button)));


document.querySelectorAll('[data-action]')
    .forEach((button) => button.addEventListener('click', () => {
        if (button.dataset.action === 'clear')
        {
            invalidate();
            expression.value = '';
            updateExpressionOverflow();
            expression.focus();
        }
        else if (button.dataset.action === 'backspace')
        {
            eraseText();
        }
        else
        {
            form.requestSubmit();
        }
    }));

async function copyCurrentResult()
{
    if (copyResult.disabled || !result.textContent)
    {
        return;
    }
    const id = generation, value = resultCopy;
    try
    {
        await copyText(value);
        if (id !== generation)
        {
            return;
        }
        copyButtons.forEach(button => { button.textContent = text.copied; button.disabled = true; });
        copyTimer = setTimeout(() => {
            if (id === generation)
            {
                copyButtons.forEach(button => { button.textContent = text.copy; button.disabled = !resultCopy; });
            }
        }, 1400);
    }
    catch (_)
    {
        if (id === generation)
        {
            copyButtons.forEach(button => { button.textContent = text.copy; });
        }
    }
}
copyButtons.forEach(button => button.addEventListener('click', copyCurrentResult));

expandResult.addEventListener('click', toggleResultExpansion);

result.addEventListener('click', toggleResultExpansion);
closeResultDialog.addEventListener('click', () => resultDialog.close());
resultDialog.addEventListener('click', event => {
    const bounds = resultDialog.getBoundingClientRect();
    if (event.clientX < bounds.left || event.clientX > bounds.right ||
        event.clientY < bounds.top || event.clientY > bounds.bottom) resultDialog.close();
});

function updatePrecisionMode()
{
    const custom = precisionMode.value === 'custom';
    precision.disabled = !custom;
    document.querySelector('#custom-precision').hidden = !custom;
    updateSettingsOverview();
}
updatePrecisionMode();
precisionMode.addEventListener('change', () => {
    updatePrecisionMode();
    scheduleCalculation(0);
});
notationMode.addEventListener('change', () => scheduleCalculation(0));
complexForm.addEventListener('change', () => scheduleCalculation(0));


async function calculate(commit)
{
    if (lifecycleInFlight || pendingReset || variableDeletionInFlight || pendingVariableDeletion || commitInFlight || (!commit && pendingCommit)) return;
    invalidate();
    if (!expression.value.trim() && !pendingCommit) return;
    const id = generation;
    controller = new AbortController();
    if (commit && pendingCommit)
    {
        expression.value = pendingCommit.input;
        updateExpressionOverflow();
        precisionMode.value = pendingCommit.mode;
        notationMode.value = pendingCommit.notation;
        complexForm.value = pendingCommit.complexForm || 'cartesian';
        if (pendingCommit.precision !== 'full') precision.value = pendingCommit.precision;
        updatePrecisionMode();
        selectAngleUnit(pendingCommit.angle, false);
    }
    result.className = '';
    result.textContent = text.calculating;
    copyButtons.forEach(button => { button.disabled = true; });
    let sent = false;
    try
    {
        const requestedPrecision = precisionMode.value === 'full' ? 'full' :
            (precisionMode.value === 'custom' ? precision.value : '10');
        const request = pendingCommit || {input: expression.value, precision: requestedPrecision,
            mode: precisionMode.value, notation: notationMode.value, complexForm: complexForm.value, angle: angleUnit, id};
        if (request.precision !== 'full' &&
            (!/^[0-9]+$/.test(request.precision) || Number(request.precision) > 10000))
            throw new Error(english ? 'Precision must be between 0 and 10000.' : 'Presnosť musí byť od 0 do 10000.');
        if (new TextEncoder().encode(request.input).length > 4096)
            throw new Error(english ? 'Expression exceeds 4096 UTF-8 bytes.' : 'Výraz presahuje 4096 UTF-8 bajtov.');
        if (commit) commitInFlight = true;
        if (!sessionStarted) await ensureSession();
        if (!commit && id !== generation) return;
        if (commit) pendingCommit = request;
        sent = true;
        const {response, data} = await requestEvaluation(
            '/api/evaluate?precision=' + encodeURIComponent(request.precision) + '&angle=' + request.angle + '&notation=' + request.notation + '&form=' + (request.complexForm || 'cartesian'), {
                method: 'POST', headers: {'Content-Type': 'text/plain; charset=utf-8'},
                body: request.input, ...(commit ? {} : {signal: controller.signal})
            }, commit ? 'commit' : 'preview', request.id);
        if (commit) pendingCommit = null;
        if (!response.ok || !data.ok) throw new Error(responseError(data));
        if (commit) {
            refreshSession().catch(error => {
                if (!lifecycle.signal.aborted) variableStatus.textContent = error.message;
            });
        }
        if (id !== generation) return;
        result.textContent = data.result;
        setResultApproximation(data.approx);
        resultCopy = Object.hasOwn(data, 'copy') ? (typeof data.copy === 'string' ? data.copy : '') :
            (request.notation === 'math' ? '' : data.result);
        copyButtons.forEach(button => { button.disabled = !resultCopy; });
        updateResultExpansion();
    }
    catch (error)
    {
        if ((id !== generation && !pendingCommit) || error.name === 'AbortError') return;
        result.className = 'error';
        setResultApproximation('');
        const uncertain = commit && sent && pendingCommit;
        result.textContent = text.error + (uncertain
            ? (english ? 'Confirmation uncertain. Press Enter to retry the same calculation, or start a new session. '
                       : 'Potvrdenie je neisté. Enter zopakuje tú istú požiadavku, alebo začni nové sedenie. ') + error.message
            : ((error instanceof TypeError || error instanceof SyntaxError) ? text.failure : error.message));
        copyButtons.forEach(button => { button.disabled = true; });
        updateResultExpansion();
    }
    finally
    {
        if (commit)
        {
            commitInFlight = false;
            if (!pendingCommit && id !== generation) scheduleCalculation(0);
        }
        if (commit && id === generation && !lifecycle.signal.aborted && mobileSettings.matches)
            document.querySelector('.result-panel').scrollIntoView({block: 'start',
                behavior: window.matchMedia('(prefers-reduced-motion: reduce)').matches ? 'auto' : 'smooth'});
    }
}

form.addEventListener('submit', event => {
    event.preventDefault();
    if (mobileSettings.matches) expression.blur();
    return calculate(true);
});

function saveNavigationState()
{
    try
    {
        sessionStorage.setItem('numforge-navigation-state', JSON.stringify({
            client: cacheClient, revision: generation, started: sessionStarted,
            pending: pendingCommit, expression: expression.value,
            precision: precision.value, mode: precisionMode.value,
            notation: notationMode.value, complexForm: complexForm.value, angle: angleUnit,
            result: result.textContent, resultCopy, resultApprox: resultApproxValue,
            resultError: result.classList.contains('error'),
            sessionTab: activeSessionTab, pendingVariableDeletion, pendingLifecycle: pendingReset, recent: recentItems,
            category: document.querySelector('.function-tabs button[aria-selected="true"]')?.id,
            search: document.querySelector('#function-search').value
        }));
    }
    catch (_) {}
}

window.addEventListener?.('numforge:navigate', saveNavigationState, {signal: lifecycle.signal});
window.addEventListener?.('pageshow', event => {
    if (event.persisted)
    {
        try { sessionStorage.removeItem('numforge-navigation-state'); } catch (_) {}
    }
}, {signal: lifecycle.signal});
try
{
    const saved = sessionStorage.getItem('numforge-navigation-state');
    sessionStorage.removeItem('numforge-navigation-state');
    if (saved)
    {
        const state = JSON.parse(saved);
        if (/^[0-9a-f]{32}$/.test(state.client) &&
            Number.isSafeInteger(state.revision) && state.revision >= 0 &&
            typeof state.expression === 'string' && typeof state.precision === 'string' &&
            ['auto', 'full', 'custom'].includes(state.mode) &&
            ['auto', 'plain', 'scientific', 'math', 'fraction'].includes(state.notation) &&
            ['rad', 'deg'].includes(state.angle))
        {
            cacheClient = state.client;
            generation = state.revision;
            sessionStarted = state.started === true;
            if (sessionStarted) sessionReady = Promise.resolve();
            expression.value = state.expression;
            precision.value = state.precision;
            precisionMode.value = state.mode;
            notationMode.value = state.notation;
            complexForm.value = ['cartesian','trig','exp'].includes(state.complexForm) ? state.complexForm : 'cartesian';
            selectAngleUnit(state.angle, false);
            updatePrecisionMode();
            updateExpressionOverflow();
            selectSessionTab(state.sessionTab === 'variables' ? 'variables' : 'history');
            if (state.pendingLifecycle && ['reset','clear-history'].includes(state.pendingLifecycle.action) &&
                Number.isSafeInteger(state.pendingLifecycle.id)) pendingReset=state.pendingLifecycle;
            if (state.pendingVariableDeletion && typeof state.pendingVariableDeletion.name === 'string' &&
                Number.isSafeInteger(state.pendingVariableDeletion.id))
                pendingVariableDeletion = state.pendingVariableDeletion;
            if (Array.isArray(state.recent))
            {
                recentItems = state.recent.filter(item => item &&
                    typeof item.key === 'string' && typeof item.label === 'string' &&
                    typeof item.insert === 'string').slice(0, recentLimit);
                renderRecent();
            }
            if (typeof state.category === 'string')
                document.getElementById(state.category)?.click();
            if (typeof state.search === 'string' && state.search)
            {
                const search = document.querySelector('#function-search');
                search.value = state.search;
                search.dispatchEvent(new Event('input'));
            }
            result.textContent = typeof state.result === 'string' ? state.result : '';
            setResultApproximation(state.resultApprox);
            result.className = state.resultError === true ? 'error' : '';
            resultCopy = typeof state.resultCopy === 'string' ? state.resultCopy : '';
            copyButtons.forEach(button => { button.disabled = !resultCopy; });
            updateResultExpansion();
            if (state.pending && typeof state.pending.input === 'string' &&
                Number.isSafeInteger(state.pending.id))
            {
                pendingCommit = state.pending;
                calculate(true);
            }
            else if (expression.value.trim()) scheduleCalculation(0);
        }
    }
}
catch (_) {}
loadFunctionRegistry();
ensureSession().then(() => refreshSession()).catch(error => {
    if (!lifecycle.signal.aborted) variableStatus.textContent = error.message;
});
window.addEventListener?.('focus', () => {
    if (sessionStarted && !commitInFlight && !variableDeletionInFlight)
        refreshSession().catch(error => { variableStatus.textContent = error.message; });
}, {signal: lifecycle.signal});
return () => {
    lifecycle.abort();
    controller?.abort();
    clearTimeout(autoTimer);
    clearTimeout(copyTimer);
};
}

window.numforgeInitCalculator = initCalculator;
if (document.querySelector('#calculator'))
    window.numforgeDisposeCalculator = initCalculator();
