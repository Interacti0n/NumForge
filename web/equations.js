(() => {
function initEquations() {
    const form = document.querySelector('#equation-form');
    if (!form) return null;
    const lifecycle = new AbortController(), signal = lifecycle.signal;
    const inputs = ['a','b','c'].map(name => document.querySelector('#equation-' + name));
    const mode = document.querySelector('#equation-mode'), expression = document.querySelector('#equation-expression');
    const fields = [...inputs, expression];
    const precision = document.querySelector('#equation-precision');
    const submit = document.querySelector('#equation-submit');
    const status = document.querySelector('#equation-status'), error = document.querySelector('#equation-error');
    const roots = document.querySelector('#equation-roots');
    const sk = document.documentElement.lang === 'sk';
    const words = sk ? {
        idle:'Zadaj rovnicu a zvoľ Vyriešiť.', busy:'Počítam…', finite:'Presné riešenia',
        none:'Rovnica nemá riešenie.', all:'Rovnicu spĺňa každé komplexné číslo.',
        double:'Dvojnásobný koreň', copy:'Kopírovať', copied:'Skopírované', copyError:'Kopírovanie sa nepodarilo.',
        exact:'Použi presný reálny koeficient, napr. 1/3 alebo sqrt(4).',
        variable:'Premenné ani ans tu nie sú dostupné.', invalid:'Skontroluj výraz koeficientu.',
        limit:'Výpočet prekročil dostupný čas alebo pamäť. Skús jednoduchšie koeficienty.',
        network:'Spojenie so serverom sa nepodarilo. Skús to znova.', size:'Celé zadanie môže mať najviac 4096 bajtov.',
        coefficient:'Koeficient', column:'pozícia',
        polynomial:'Podporujeme polynómy v x do stupňa 2 s presnými reálnymi konštantami. Bez x v menovateli či vo funkciách.', onlyX:'Neznáma môže byť iba x; ans ani uložené premenné tu nie sú dostupné.', equationInvalid:'Skontroluj zápis rovnice vrátane zátvoriek a znaku =.'
    } : {
        idle:'Enter an equation and choose Solve.', busy:'Calculating…', finite:'Exact solutions',
        none:'The equation has no solutions.', all:'Every complex number satisfies the equation.',
        double:'Double root', copy:'Copy', copied:'Copied', copyError:'Could not copy.',
        exact:'Use an exact real coefficient, e.g. 1/3 or sqrt(4).',
        variable:'Variables and ans are unavailable here.', invalid:'Check the coefficient expression.',
        limit:'The calculation exceeded the available time or memory. Try simpler coefficients.',
        network:'Could not connect to the server. Try again.', size:'The complete input must fit in 4096 bytes.',
        coefficient:'Coefficient', column:'position',
        polynomial:'Use a polynomial in x of degree at most 2 with exact real constants. No x in denominators or function calls.', onlyX:'Only x is supported; ans and saved variables are unavailable here.', equationInvalid:'Check the equation, including parentheses and the = sign.'
    };
    let controller = null, generation = 0, result = null;
    const equationHelp = document.querySelector('#equation-help').textContent;
    function save() {
        const state = {inputs:inputs.map(input => input.value), expression:expression.value, mode:mode.value, precision:precision.value, result};
        try {
            if (JSON.stringify(state).length > 24000) state.result = null;
            sessionStorage.setItem('numforge-equations-v1', JSON.stringify(state));
        } catch (_) { /* Storage is optional. */ }
    }
    function render(value) {
        roots.replaceChildren();
        status.textContent = value ? words[value.kind === 'finite' ? 'finite' : value.kind] : words.idle;
        if (!value || value.kind !== 'finite') return;
        value.roots.forEach((root, index) => {
            const row = document.createElement('li'); row.className = 'equation-root';
            const exact = document.createElement('code'); exact.textContent = 'x' + (value.count === 2 ? (index ? '₂' : '₁') : '') + ' = ' + root.exact;
            row.append(exact);
            if (root.multiplicity === 2) { const note = document.createElement('small'); note.textContent = words.double; row.append(note); }
            if (root.approximation !== root.exact) {
                const approximation = document.createElement('code'); approximation.className = 'equation-approx';
                approximation.textContent = '≈ ' + root.approximation; row.append(approximation);
            }
            const copy = document.createElement('button'); copy.type = 'button'; copy.textContent = words.copy;
            copy.addEventListener('click', async () => {
                try { await navigator.clipboard.writeText(root.exact); copy.textContent = words.copied; }
                catch (_) { copy.textContent = words.copyError; }
            }, {signal});
            row.append(copy); roots.append(row);
        });
    }
    function cancel() { ++generation; controller?.abort(); controller = null; submit.disabled = false; form.removeAttribute('aria-busy'); }
    function edit() {
        cancel(); result = null; error.hidden = true;
        fields.forEach(input => input.removeAttribute('aria-invalid'));
        render(null); save();
    }
    function showError(data) {
        const field = /^coefficient_([abc])_error$/.exec(data.code || '');
        let message = mode.value === 'expression' ? words.equationInvalid : words.invalid;
        if (/not implemented|invalid quantity|invalid argument/.test(data.status)) message = words.exact;
        if (mode.value === 'expression' && /not implemented/.test(data.status)) message = words.polynomial;
        if (/undefined/.test(data.status)) message = words.variable;
        if (mode.value === 'expression' && /undefined/.test(data.status)) message = words.onlyX;
        if (/TLE|value too large|out of memory|scale overflow/.test(data.status)) message = words.limit;
        error.textContent = (field ? words.coefficient + ' ' + field[1] + ': ' : '') + message +
            ((field || mode.value === 'expression') && Number.isInteger(data.column) ? ' · ' + words.column + ' ' + data.column : '');
        error.hidden = false; status.textContent = '';
        if (field) { const input = inputs['abc'.indexOf(field[1])]; input.setAttribute('aria-invalid','true'); input.focus(); }
        else if (mode.value === 'expression') {
            expression.setAttribute('aria-invalid','true'); expression.focus();
            const caret = Array.from(expression.value).slice(0, Math.max(0,(data.column || 1)-1)).join('').length;
            expression.setSelectionRange(caret, caret);
        }
    }
    function syncMode() {
        const whole = mode.value === 'expression';
        document.querySelector('.equation-coefficients').hidden = whole;
        document.querySelector('#equation-expression-field').hidden = !whole;
        expression.disabled = !whole;
        inputs.forEach(input => input.disabled = whole);
        document.querySelector('#equation-help').textContent = whole ? equationHelp : (sk
            ? 'Čísla alebo presné výrazy, napr. 1/3 či sqrt(4). Pre lineárnu rovnicu nastav a = 0.'
            : 'Numbers or exact expressions, e.g. 1/3 or sqrt(4). Use a = 0 for a linear equation.');
    }
    mode.addEventListener('change', () => { syncMode(); edit(); }, {signal});
    form.addEventListener('input', edit, {signal});
    document.querySelector('#equation-clear').addEventListener('click', () => { const active = mode.value === 'expression' ? [expression] : inputs; active.forEach(input => input.value = ''); edit(); active[0].focus(); }, {signal});
    form.addEventListener('submit', async event => {
        event.preventDefault(); cancel(); result = null; render(null); error.hidden = true;
        fields.forEach(input => input.removeAttribute('aria-invalid'));
        const active = mode.value === 'expression' ? [expression] : inputs;
        const body = active.map(input => input.value).join('\n');
        if (new TextEncoder().encode(body).length > 4096 || active.some(input => /[\r\n]/.test(input.value))) {
            error.textContent = words.size; error.hidden = false; save(); return;
        }
        const current = generation; controller = new AbortController();
        const request = controller;
        const timeout = setTimeout(() => request.abort(), 15000);
        submit.disabled = true; form.setAttribute('aria-busy','true'); status.textContent = words.busy; save();
        try {
            const response = await fetch('/api/equations/solve?mode=' + mode.value + '&precision=' + encodeURIComponent(precision.value), {
                method:'POST', headers:{'Content-Type':'text/plain; charset=utf-8'}, body, signal:request.signal
            });
            const data = await response.json();
            if (current !== generation) return;
            if (!response.ok || !data.ok) { showError(data); return; }
            if (!['finite','none','all'].includes(data.kind) || !Array.isArray(data.roots) || data.roots.length > 2 ||
                data.roots.some(root => typeof root.exact !== 'string' || typeof root.approximation !== 'string')) throw Error('Invalid response');
            result = data; render(data); save();
            if (matchMedia('(max-width:760px)').matches) document.querySelector('.equation-result').scrollIntoView({block:'start',behavior:'smooth'});
        } catch (_) {
            if (current === generation) { error.textContent = words.network; error.hidden = false; status.textContent = ''; }
        } finally {
            clearTimeout(timeout);
            if (current === generation) { controller = null; submit.disabled = false; form.removeAttribute('aria-busy'); }
        }
    }, {signal});
    try {
        const saved = JSON.parse(sessionStorage.getItem('numforge-equations-v1'));
        if (saved && Array.isArray(saved.inputs) && saved.inputs.length === 3 && saved.inputs.every(value => typeof value === 'string' && value.length <= 4096)) {
            inputs.forEach((input,index) => input.value = saved.inputs[index]);
            mode.value = saved.mode === 'expression' ? 'expression' : 'coefficients';
            if (typeof saved.expression === 'string' && saved.expression.length <= 4096) expression.value = saved.expression;
            if (/^\d+$/.test(saved.precision) && Number(saved.precision) >= 1 && Number(saved.precision) <= 1000) precision.value = saved.precision;
            if (saved.result && ['finite','none','all'].includes(saved.result.kind) && Array.isArray(saved.result.roots) && saved.result.roots.length <= 2 &&
                saved.result.roots.every(root => typeof root.exact === 'string' && typeof root.approximation === 'string')) result = saved.result;
        }
    } catch (_) { /* Ignore unavailable or obsolete state. */ }
    syncMode(); render(result);
    return () => { save(); cancel(); lifecycle.abort(); };
}
window.numforgeInitEquations = initEquations;
window.numforgeDisposeEquations = initEquations();
})();
