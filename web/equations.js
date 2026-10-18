(() => {
function initEquations() {
    const form = document.querySelector('#equation-form');
    if (!form) return null;
    const lifecycle = new AbortController(), signal = lifecycle.signal;
    const inputs = ['a','b','c'].map(name => document.querySelector('#equation-' + name));
    const mode = document.querySelector('#equation-mode'), expression = document.querySelector('#equation-expression');
    const bounds = [document.querySelector('#equation-min'), document.querySelector('#equation-max')];
    const fields = [...inputs, expression, ...bounds];
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
    const numericWords = sk ? {
        found:'Jeden približný koreň. Ďalšie môžu existovať.', not_bracketed:'Interval nepotvrdil zmenu znamienka. Koreň môže existovať; uprav interval.',
        domain_gap:'V skúmaných bodoch je nedefinovaná alebo nereálna hodnota. Zvoľ interval v reálnej doméne.',
        domain_unverified:'V okolí kandidáta sa nepodarilo overiť spojitosť a definičný obor. Koreň nepotvrdzujeme; skús iný interval alebo jednoduchší zápis.',
        iteration_limit:'Koreň sa v rámci počtu krokov nepodarilo potvrdiť. Skús užší interval.',
        precision_limit:'Výpočet nerozlíšil hodnoty dostatočne. Zvýš presnosť alebo zúž interval.',
        help:'Jeden reálny koreň, napr. cos(x) = x. Zvoľ spojitý interval so zmenou znamienka rozdielu strán. Nehľadá všetky korene.',
        residual:'Zvyšok v overenom bode: ', copy:'Kopírovať bod', options:'Skontroluj interval od < do a presnosť 16–100.'
    } : {
        found:'One approximate root. Other roots may exist.', not_bracketed:'The interval did not confirm a sign change. A root may still exist; adjust the interval.',
        domain_gap:'A sampled value is undefined or non-real. Choose an interval within the real domain.',
        domain_unverified:'Continuity and the real domain could not be verified around the candidate. No root is confirmed; try another interval or a simpler expression.',
        iteration_limit:'Could not confirm a root within the step limit. Try a narrower interval.',
        precision_limit:'Values could not be distinguished sufficiently. Increase precision or narrow the interval.',
        help:'One real root, e.g. cos(x) = x. Choose a continuous interval where the difference between sides changes sign. Does not enumerate roots.',
        residual:'Residual at the checked point: ', copy:'Copy point', options:'Check that from < to and precision is 16–100.'
    };
    let controller = null, generation = 0, result = null;
    const equationHelp = document.querySelector('#equation-help').textContent;
    function save() {
        const state = {numericDomainVersion:2, inputs:inputs.map(input => input.value), expression:expression.value, bounds:bounds.map(input=>input.value), mode:mode.value, precision:precision.value, result};
        try {
            if (JSON.stringify(state).length > 24000) state.result = null;
            sessionStorage.setItem('numforge-equations-v1', JSON.stringify(state));
        } catch (_) { /* Storage is optional. */ }
    }
    function render(value) {
        roots.replaceChildren();
        status.textContent = value?.kind === 'numeric' ? numericWords[value.outcome] : value ? words[value.kind === 'finite' ? 'finite' : value.kind] : words.idle;
        if (!value || !['finite','numeric'].includes(value.kind)) return;
        value.roots.forEach((root, index) => {
            const row = document.createElement('li'); row.className = 'equation-root';
            const numerical = value.kind === 'numeric';
            const exact = document.createElement('code'); exact.textContent = numerical ? 'x ≈ ' + root.approximation : 'x' + (value.count === 2 ? (index ? '₂' : '₁') : '') + ' = ' + root.exact;
            row.append(exact);
            if (root.multiplicity === 2) { const note = document.createElement('small'); note.textContent = words.double; row.append(note); }
            if (numerical) {
                const note = document.createElement('small'); note.textContent = numericWords.residual + root.residual;
                note.style.overflowWrap = 'anywhere'; row.append(note);
            } else if (root.approximation !== root.exact) {
                const approximation = document.createElement('code'); approximation.className = 'equation-approx';
                approximation.textContent = '≈ ' + root.approximation; row.append(approximation);
            }
            const copy = document.createElement('button'); copy.type = 'button'; copy.textContent = numerical ? numericWords.copy : words.copy;
            copy.addEventListener('click', async () => {
                try { await navigator.clipboard.writeText(numerical ? root.x : root.exact); copy.textContent = words.copied; }
                catch (_) { copy.textContent = words.copyError; }
            }, {signal});
            const actions = document.createElement('div'); actions.className = 'equation-root-actions';
            actions.append(copy);
            // Decimal previews position the camera only. Graph intercepts still
            // have to pass the graph API's independent C verification.
            const coordinate = numerical ? root.x : root.approximation;
            const x = typeof coordinate === 'string' && coordinate.length <= 65536 && /^[-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?$/.test(coordinate) ? Number(coordinate) : NaN;
            const radius = Math.max(10, Math.abs(x) * .6);
            if ((numerical || value.imaginary === false) && Number.isFinite(x) &&
                Number.isFinite(x-radius) && Number.isFinite(x+radius) &&
                !(x === 0 && /[1-9]/.test(coordinate.split(/[eE]/)[0]))) {
                const graph = document.createElement('a'); graph.className = 'equation-graph-link';
                graph.href = '/graph?lang=' + (sk ? 'sk' : 'en');
                graph.textContent = sk ? 'Zobraziť v grafe' : 'Show in graph';
                graph.title = sk ? 'Rozdiel strán rovnice; riešenie leží na osi x.' : 'Difference between equation sides; a solution lies on the x axis.';
                graph.addEventListener('click', event => {
                    // Use normal same-tab navigation, including the SPA router.
                    if (event.button || event.ctrlKey || event.metaKey || event.shiftKey || event.altKey) { event.preventDefault(); return; }
                    let source;
                    if (mode.value === 'coefficients') source = '('+inputs[0].value+')*x^2+('+inputs[1].value+')*x+('+inputs[2].value+')';
                    else {
                        const sides = expression.value.split('=');
                        if (sides.length === 2) source = '('+sides[0]+')-('+sides[1]+')';
                    }
                    try {
                        if (!source || new TextEncoder().encode(source).length > 4096) throw Error();
                        const saved = JSON.parse(sessionStorage.getItem('numforge-graph-state') || 'null');
                        const entries = Array.isArray(saved?.curves) ? saved.curves.filter(c=>c && typeof c.expression === 'string' && c.expression.trim()) : [];
                        if (entries.length >= 4 && !entries.some(c=>c.expression === source)) {
                            event.preventDefault(); error.textContent = sk ? 'V grafe sú už štyri funkcie. Najprv jednu odstráň.' : 'The graph already has four functions. Remove one first.'; error.hidden = false; return;
                        }
                        sessionStorage.setItem('numforge-equation-graph', JSON.stringify({expression:source, x:[x-radius,x+radius]}));
                    } catch (_) {
                        event.preventDefault(); error.textContent = sk ? 'Prenos do grafu sa nepodaril. Skontroluj dostupnosť úložiska alebo skráť zadanie.' : 'Could not open the graph. Check browser storage or shorten the input.'; error.hidden = false;
                    }
                }, {signal});
                actions.append(graph);
            }
            row.append(actions); roots.append(row);
        });
    }
    function cancel() { ++generation; controller?.abort(); controller = null; submit.disabled = false; form.removeAttribute('aria-busy'); }
    function edit() {
        cancel(); result = null; error.hidden = true;
        fields.forEach(input => input.removeAttribute('aria-invalid'));
        render(null); save();
    }
    function showError(data) {
        if (data.code === 'numeric_options_error') {
            error.textContent = numericWords.options; error.hidden = false; status.textContent = '';
            bounds.forEach(input=>input.setAttribute('aria-invalid','true')); bounds[0].focus(); return;
        }
        const field = /^coefficient_([abc])_error$/.exec(data.code || '');
        let message = mode.value !== 'coefficients' ? words.equationInvalid : words.invalid;
        if (/not implemented|invalid quantity|invalid argument/.test(data.status)) message = words.exact;
        if (mode.value === 'expression' && /not implemented/.test(data.status)) message = words.polynomial;
        if (/undefined/.test(data.status)) message = words.variable;
        if (mode.value !== 'coefficients' && /undefined/.test(data.status)) message = words.onlyX;
        if (/TLE|value too large|out of memory|scale overflow/.test(data.status)) message = words.limit;
        error.textContent = (field ? words.coefficient + ' ' + field[1] + ': ' : '') + message +
            ((field || mode.value !== 'coefficients') && Number.isInteger(data.column) ? ' · ' + words.column + ' ' + data.column : '');
        error.hidden = false; status.textContent = '';
        if (field) { const input = inputs['abc'.indexOf(field[1])]; input.setAttribute('aria-invalid','true'); input.focus(); }
        else if (mode.value !== 'coefficients') {
            expression.setAttribute('aria-invalid','true'); expression.focus();
            const caret = Array.from(expression.value).slice(0, Math.max(0,(data.column || 1)-1)).join('').length;
            expression.setSelectionRange(caret, caret);
        }
    }
    function syncMode() {
        const whole = mode.value !== 'coefficients', numerical = mode.value === 'numeric';
        document.querySelector('.equation-formula').textContent = numerical ? 'f(x) = g(x)' : 'ax² + bx + c = 0';
        document.querySelector('.equation-coefficients').hidden = whole;
        document.querySelector('#equation-expression-field').hidden = !whole;
        expression.disabled = !whole;
        inputs.forEach(input => input.disabled = whole);
        document.querySelector('#equation-range').hidden = !numerical;
        bounds.forEach(input=>input.disabled = !numerical);
        precision.min = numerical ? '16' : '1'; precision.max = numerical ? '100' : '1000';
        if (numerical && (Number(precision.value)<16 || Number(precision.value)>100)) precision.value = '34';
        document.querySelector('#equation-help').textContent = numerical ? numericWords.help : whole ? equationHelp : (sk
            ? 'Čísla alebo presné výrazy, napr. 1/3 či sqrt(4). Pre lineárnu rovnicu nastav a = 0.'
            : 'Numbers or exact expressions, e.g. 1/3 or sqrt(4). Use a = 0 for a linear equation.');
    }
    mode.addEventListener('change', () => { syncMode(); edit(); }, {signal});
    form.addEventListener('input', edit, {signal});
    document.querySelector('#equation-clear').addEventListener('click', () => { const active = mode.value !== 'coefficients' ? [expression] : inputs; active.forEach(input => input.value = ''); edit(); active[0].focus(); }, {signal});
    form.addEventListener('submit', async event => {
        event.preventDefault(); cancel(); result = null; render(null); error.hidden = true;
        fields.forEach(input => input.removeAttribute('aria-invalid'));
        const active = mode.value !== 'coefficients' ? [expression] : inputs;
        const body = active.map(input => input.value).join('\n');
        if (new TextEncoder().encode(body).length > 4096 || active.some(input => /[\r\n]/.test(input.value))) {
            error.textContent = words.size; error.hidden = false; save(); return;
        }
        const current = generation; controller = new AbortController();
        const request = controller;
        const timeout = setTimeout(() => request.abort(), 15000);
        submit.disabled = true; form.setAttribute('aria-busy','true'); status.textContent = words.busy; save();
        try {
            let target = '/api/equations/solve?mode=' + mode.value + '&precision=' + encodeURIComponent(precision.value);
            if (mode.value === 'numeric') target += '&min=' + encodeURIComponent(bounds[0].value) + '&max=' + encodeURIComponent(bounds[1].value);
            const response = await fetch(target, {
                method:'POST', headers:{'Content-Type':'text/plain; charset=utf-8'}, body, signal:request.signal
            });
            const data = await response.json();
            if (current !== generation) return;
            if (!response.ok || !data.ok) { showError(data); return; }
            if (!validResult(data)) throw Error('Invalid response');
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
            mode.value = ['expression','numeric'].includes(saved.mode) ? saved.mode : 'coefficients';
            if (Array.isArray(saved.bounds) && saved.bounds.length === 2 && saved.bounds.every(value=>typeof value==='string'&&value.length<=47)) bounds.forEach((input,index)=>input.value=saved.bounds[index]);
            if (typeof saved.expression === 'string' && saved.expression.length <= 4096) expression.value = saved.expression;
            if (/^\d+$/.test(saved.precision) && Number(saved.precision) >= 1 && Number(saved.precision) <= 1000) precision.value = saved.precision;
            if (validResult(saved.result) && (saved.result.kind !== 'numeric' || saved.numericDomainVersion === 2)) result = saved.result;
        }
    } catch (_) { /* Ignore unavailable or obsolete state. */ }
    function validResult(value) {
        return value && ['finite','none','all','numeric'].includes(value.kind) && Array.isArray(value.roots) && value.roots.length <= 2 &&
            (value.kind !== 'numeric' || Object.hasOwn(numericWords, value.outcome) && ['found','not_bracketed','domain_gap','domain_unverified','iteration_limit','precision_limit'].includes(value.outcome)) &&
            value.roots.every(root=>typeof root.approximation==='string' && (value.kind==='numeric' ? typeof root.x==='string' && typeof root.residual==='string' : typeof root.exact==='string'));
    }
    syncMode(); render(result);
    return () => { save(); cancel(); lifecycle.abort(); };
}
window.numforgeInitEquations = initEquations;
window.numforgeDisposeEquations = initEquations();
})();
