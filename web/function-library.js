// Shared localized descriptions and search aliases; the C registry owns availability.
window.numforgeFunctionLibrary=(()=>{
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
    sum: ["sum","sum(x;y;...)","Súčet 1 až 256 reálnych alebo komplexných hodnôt; presné zlomky zostávajú presné.","Sum of 1 to 256 real or complex values; exact fractions stay exact."],
    product: ["product","product(x;y;...)","Súčin 1 až 256 reálnych alebo komplexných hodnôt; presné zlomky zostávajú presné.","Product of 1 to 256 real or complex values; exact fractions stay exact."],
    mean: ["mean","mean(x;y;...)","Aritmetický priemer 1 až 256 reálnych alebo komplexných hodnôt; presné zlomky zostávajú presné.","Arithmetic mean of 1 to 256 real or complex values; exact fractions stay exact."],
    floor: ["⌊x⌋","floor(x)","Zaokrúhlenie nadol na celé číslo.","Round down to an integer."],
    ceil: ["⌈x⌉","ceil(x)","Zaokrúhlenie nahor na celé číslo.","Round up to an integer."],
    trunc: ["trunc","trunc(x)","Odstránenie desatinnej časti smerom k nule.","Remove the fractional part toward zero."],
    round: ["round","round(x) / round(x;n)","Half-even na celé n miest; n môže byť záporné.","Half-even to integer n places; n may be negative."],
    gcd: ["gcd","gcd(x;y)","Najväčší spoločný deliteľ celých čísel.","Greatest common divisor of integers."],
    lcm: ["lcm","lcm(x;y)","Najmenší spoločný násobok celých čísel.","Least common multiple of integers."],
    mod: ["mod","mod(x;y)","Zvyšok celočíselného delenia; y ≠ 0.","Integer division remainder; y ≠ 0."],
    npr: ["nPr","npr(n;r)","Permutácie bez opakovania; celé 0 ≤ r ≤ n.","Permutations without repetition; integers 0 ≤ r ≤ n."],
    ncr: ["nCr","ncr(n;r)","Kombinácie bez opakovania; celé 0 ≤ r ≤ n.","Combinations without repetition; integers 0 ≤ r ≤ n."],
    factorial: ["n!","factorial(n)","Nezáporné celé n; platia časové a pamäťové limity.","Non-negative integer n; time and memory limits apply."],
    isqrt: ["isqrt","isqrt(n)","Celá časť odmocniny; celé n ≥ 0.","Integer square root; integer n ≥ 0."],
    rand: ["rand","rand() / rand(x) / rand(x;y)","Náhodná hodnota v [0,1), [0,x) alebo [x,y); x > 0 a x < y.","Random value in [0,1), [0,x) or [x,y); x > 0 and x < y."],
    pow: ["xʸ","pow(x;n)","Reálne aj komplexné exponenty; záporný základ s neceločíselným exponentom dá hlavnú komplexnú mocninu.","Real and complex exponents; a negative base with a noninteger exponent gives a principal complex power."],
    complex: ["a + bi","complex(re;im)","Reálna a imaginárna zložka. Funguje aj a + b*i; i je imaginárna jednotka.","Real and imaginary components. Also accepts a + b*i; i is the imaginary unit."],
    re: ["Re","re(z)","Reálna časť; presné zlomky zostávajú presné.","Real part; exact fractions stay exact."],
    im: ["Im","im(z)","Imaginárna časť; pre reálne číslo vráti 0.","Imaginary part; returns 0 for a real number."],
    conj: ["conj","conj(z)","Komplexne združené číslo: a + bi → a − bi.","Complex conjugate: a + bi → a − bi."],
    arg: ["arg","arg(z)","Hlavný argument v radiánoch (−π, π]; pre nulu nie je definovaný.","Principal argument in radians (−π, π]; undefined for zero."],
    sqrt: ["√x","sqrt(x)","Hlavná odmocnina; záporné x bez jednotiek dá komplexný výsledok, napr. sqrt(-1) = i.","Principal square root; negative dimensionless x gives a complex result, e.g. sqrt(-1) = i."],
    cbrt: ["∛x","cbrt(x)","Tretia odmocnina; záporné reálne x zostáva reálne, komplexné vstupy používajú hlavnú vetvu.","Cube root; negative real x stays real, complex inputs use the principal branch."]
};
Object.assign(functionHelp, {
    root: ["ⁿ√x","root(x;n)","Stupeň 1–10000; záporné x pri párnom stupni dá hlavnú komplexnú odmocninu.","Degree 1–10000; negative x with even degree gives a principal complex root."],
    exp: ["eˣ","exp(x)","Eulerovo číslo umocnené na reálny alebo komplexný exponent; polárny uhol je v radiánoch.","Euler’s number raised to a real or complex exponent; polar angle is in radians."],
    ln: ["ln","ln(x)","Prirodzený logaritmus; záporné x sa automaticky rozšíri na komplexné, nula je neplatná.","Natural logarithm; negative x automatically promotes to complex, zero is invalid."],
    log: ["log","log(x) / log(x;y)","Základ 10 alebo y; záporné vstupy a základy sú komplexné. x ≠ 0, y ≠ 0 a y ≠ 1.","Base 10 or y; negative inputs/bases promote to complex. x ≠ 0, y ≠ 0 and y ≠ 1."],
    median: ["median","median(x;y;...)","Presný medián 1 až 256 hodnôt.","Exact median of 1 to 256 values."],
    geomean: ["geomean","geomean(x;y;...)","Geometrický priemer nezáporných hodnôt.","Geometric mean of non-negative values."],
    harmean: ["harmean","harmean(x;y;...)","Harmonický priemer kladných hodnôt.","Harmonic mean of positive values."],
    variance: ["variance","variance(x;y;...)","Populačný rozptyl; 1 až 256 hodnôt.","Population variance; 1 to 256 values."],
    stdevp: ["stdevp","stdevp(x;y;...)","Populačná smerodajná odchýlka; delí n.","Population standard deviation; divides by n."],
    stdev: ["stdev","stdev(x;y;...)","Výberová smerodajná odchýlka; aspoň 2 hodnoty, delí n−1.","Sample standard deviation; at least 2 values, divides by n−1."],
    sin: ["sin","sin(x)","Sínus; reálny vstup podľa RAD/DEG, komplexný vždy v radiánoch.","Sine; real input follows RAD/DEG, complex input always uses radians."],
    cos: ["cos","cos(x)","Kosínus; reálny vstup podľa RAD/DEG, komplexný vždy v radiánoch.","Cosine; real input follows RAD/DEG, complex input always uses radians."],
    tan: ["tan","tan(x)","Tangens; reálny vstup RAD/DEG, komplexný v radiánoch. V póloch nedefinovaný; blízko pólov je citlivý na chyby.","Tangent; real input follows RAD/DEG, complex input uses radians. Undefined at poles; sensitive to errors near poles."],
    asin: ["asin","asin(x)","Inverzný sínus: reálne -1 ≤ x ≤ 1 v RAD/DEG; mimo intervalu automaticky komplexný výsledok v radiánoch.","Inverse sine: real -1 ≤ x ≤ 1 in RAD/DEG; outside the interval automatically complex in radians."],
    acos: ["acos","acos(x)","Inverzný kosínus: reálne -1 ≤ x ≤ 1 v RAD/DEG; mimo intervalu automaticky komplexný výsledok v radiánoch.","Inverse cosine: real -1 ≤ x ≤ 1 in RAD/DEG; outside the interval automatically complex in radians."],
    atan: ["atan","atan(x)","Inverzný tangens: reálne vstupy v RAD/DEG, komplexné v radiánoch; ±i nie sú definované.","Inverse tangent: real inputs in RAD/DEG, complex inputs in radians; ±i are undefined."],
    sinh: ["sinh","sinh(x)","Hyperbolický sínus; aj komplexné vstupy, imaginárny uhol v radiánoch. Nezávisí od RAD/DEG.","Hyperbolic sine; also complex inputs, imaginary angle in radians. Independent of RAD/DEG."],
    cosh: ["cosh","cosh(x)","Hyperbolický kosínus; aj komplexné vstupy, imaginárny uhol v radiánoch. Nezávisí od RAD/DEG.","Hyperbolic cosine; also complex inputs, imaginary angle in radians. Independent of RAD/DEG."],
    tanh: ["tanh","tanh(x)","Hyperbolický tangens; aj komplexné vstupy v radiánoch. Pri imaginárnych póloch je citlivý na chyby.","Hyperbolic tangent; also complex inputs in radians. Sensitive to errors near imaginary-axis poles."],
    asinh: ["asinh","asinh(x)","Inverzný hyperbolický sínus; aj komplexné vstupy, hlavná vetva.","Inverse hyperbolic sine; also complex inputs, principal branch."],
    acosh: ["acosh","acosh(x)","Inverzný hyperbolický kosínus; x < 1 sa automaticky rozšíri na komplexné.","Inverse hyperbolic cosine; x < 1 automatically promotes to complex."],
    atanh: ["atanh","atanh(x)","Inverzný hyperbolický tangens; mimo (-1,1) komplexný výsledok, ±1 sú neplatné.","Inverse hyperbolic tangent; outside (-1,1) complex results, ±1 are invalid."],
    radians: ["° → rad","radians(x)","Prevod stupňov na radiány nezávisle od režimu.","Convert degrees to radians regardless of mode."],
    degrees: ["rad → °","degrees(x)","Prevod radiánov na stupne nezávisle od režimu.","Convert radians to degrees regardless of mode."]
});
function constantAliasesFor(english){
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
    return constantAliases;
}
return {inverseFunctionAliases,functionSearchAliases,functionHelp,constantAliasesFor,
    normalize:value=>value.toLocaleLowerCase().normalize('NFD').replace(/[\u0300-\u036f]/g,'').replace(/[’']/g,'')};
})();
