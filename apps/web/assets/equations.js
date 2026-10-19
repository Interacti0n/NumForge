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
    const editor = document.querySelector('#equation-editor'), editorInput = document.querySelector('#equation-editor-input');
    let editorTarget = expression, submittingEditor = false;
    const isNumeric = kind => kind === 'numeric' || kind === 'numeric_scan';
    const words = sk ? {
        idle:'Zadaj rovnicu a zvoľ Vyriešiť.', busy:'Počítam…', finite:'Presné riešenia',
        none:'Rovnica nemá riešenie.', all:'Rovnicu spĺňa každé komplexné číslo.',
        double:'Dvojnásobný koreň', copy:'Kopírovať', copied:'Skopírované', copyError:'Kopírovanie sa nepodarilo.',
        exact:'Použi presný reálny koeficient, napr. 1/3 alebo sqrt(4).',
        variable:'Premenné ani ans tu nie sú dostupné.', invalid:'Skontroluj výraz koeficientu.',
        limit:'Výpočet prekročil dostupný čas alebo pamäť. Zjednoduš zadanie alebo zníž presnosť.',
        network:'Spojenie so serverom sa nepodarilo. Skús to znova.', size:'Celé zadanie môže mať najviac 4096 bajtov.',
        coefficient:'Koeficient', column:'pozícia',
        polynomial:'Podporujeme polynómy v x do stupňa 2 s presnými reálnymi konštantami. Bez x v menovateli či vo funkciách. Pre všeobecné funkcie zvoľ numerický režim.', onlyX:'Neznáma môže byť iba x; ans ani uložené premenné tu nie sú dostupné.', equationInvalid:'Skontroluj zápis rovnice vrátane zátvoriek a znaku =.'
    } : {
        idle:'Enter an equation and choose Solve.', busy:'Calculating…', finite:'Exact solutions',
        none:'The equation has no solutions.', all:'Every complex number satisfies the equation.',
        double:'Double root', copy:'Copy', copied:'Copied', copyError:'Could not copy.',
        exact:'Use an exact real coefficient, e.g. 1/3 or sqrt(4).',
        variable:'Variables and ans are unavailable here.', invalid:'Check the coefficient expression.',
        limit:'The calculation exceeded the available time or memory. Simplify the input or reduce precision.',
        network:'Could not connect to the server. Try again.', size:'The complete input must fit in 4096 bytes.',
        coefficient:'Coefficient', column:'position',
        polynomial:'Use a polynomial in x of degree at most 2 with exact real constants. No x in denominators or function calls. Choose a numerical mode for general functions.', onlyX:'Only x is supported; ans and saved variables are unavailable here.', equationInvalid:'Check the equation, including parentheses and the = sign.'
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
    const scanWords = sk ? {
        help:'Hľadá viac reálnych koreňov vrátane jednoduchých dotykov. Zoznam môže byť neúplný; pri chýbajúcom koreni skús užší interval.',
        scanned:'Prehľadávanie skončilo. Ďalšie korene môžu existovať.',
        root_limit:'Dosiahnutý limit 32 výsledkov. Pre ďalšie korene zúž interval.',
        evaluation_limit:'Dosiahnutý limit práce. Pre ďalšie korene zúž interval.',
        found:'Nájdené korene: ', empty:'V tomto prehľadaní sa nepodarilo potvrdiť koreň. ',
        gaps:' Niektoré úseky boli nedefinované alebo sa koreň nepodarilo overiť.'
    } : {
        help:'Searches for multiple real roots, including simple contacts. The list may be incomplete; try a narrower interval if a root is missing.',
        scanned:'Scan finished. Other roots may exist.',
        root_limit:'Reached the 32-result limit. Narrow the interval to look for more roots.',
        evaluation_limit:'Reached the work limit. Narrow the interval to look for more roots.',
        found:'Roots found: ', empty:'This scan could not confirm a root. ',
        gaps:' Some sections were undefined or a root could not be verified.'
    };
    let controller = null, generation = 0, result = null;
    const equationHelp = document.querySelector('#equation-help').textContent;
    function save() {
        const state = {numericDomainVersion:2, scanCandidateVersion:1, inputs:inputs.map(input => input.value), expression:expression.value, bounds:bounds.map(input=>input.value), mode:mode.value, precision:precision.value, result};
        try {
            if (JSON.stringify(state).length > 24000) state.result = null;
            sessionStorage.setItem('numforge-equations-v1', JSON.stringify(state));
        } catch (_) { /* Storage is optional. */ }
    }
    function render(value) {
        roots.replaceChildren();
        status.textContent = value?.kind === 'numeric_scan' ?
            (value.count ? scanWords.found + value.count + '. ' : scanWords.empty) + scanWords[value.outcome] +
            (value.domain_gaps || value.rejected_brackets || value.candidate_gaps ? scanWords.gaps : '') :
            value?.kind === 'numeric' ? numericWords[value.outcome] : value ? words[value.kind === 'finite' ? 'finite' : value.kind] : words.idle;
        if (!value || !['finite','numeric','numeric_scan'].includes(value.kind)) return;
        value.roots.forEach((root, index) => {
            const row = document.createElement('li'); row.className = 'equation-root';
            const numerical = isNumeric(value.kind);
            let verification = null;
            const exact = document.createElement('code'); exact.textContent = numerical ? 'x ≈ ' + root.approximation : 'x' + (value.count === 2 ? (index ? '₂' : '₁') : '') + ' = ' + root.exact;
            row.append(exact);
            if (root.multiplicity === 2) { const note = document.createElement('small'); note.textContent = words.double; row.append(note); }
            if (numerical) {
                verification=document.createElement('details');verification.className='equation-verification';
                const summary=document.createElement('summary');summary.textContent=sk?'Overený bod':'Checked point';
                const coordinate=document.createElement('code');coordinate.textContent='x = '+root.x;
                const note = document.createElement('small'); note.textContent = numericWords.residual + root.residual;
                note.style.overflowWrap = 'anywhere';verification.append(summary,coordinate,note);row.append(verification);
            } else if (root.approximation !== root.exact) {
                const approximation = document.createElement('code'); approximation.className = 'equation-approx';
                approximation.textContent = '≈ ' + root.approximation; row.append(approximation);
            }
            const copy = document.createElement('button'); copy.type = 'button'; copy.textContent = numerical ? numericWords.copy : words.copy;
            copy.addEventListener('click', async () => {
                try { await navigator.clipboard.writeText(numerical ? root.x : root.exact); copy.textContent = words.copied; }
                catch (_) { copy.textContent = words.copyError;if(verification)verification.open=true; }
            }, {signal});
            const actions = document.createElement('div'); actions.className = 'equation-root-actions';
            actions.append(copy);
            // Decimal previews position the camera only. Graph intercepts still
            // have to pass the graph API's independent C verification.
            const coordinate = numerical && root.coordinate_kind !== 'rational' ? root.x : root.approximation;
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
        const whole = mode.value !== 'coefficients', numerical = isNumeric(mode.value);
        document.querySelector('.equation-formula').textContent = numerical ? 'f(x) = g(x)' : 'ax² + bx + c = 0';
        document.querySelector('.equation-coefficients').hidden = whole;
        document.querySelector('#equation-expression-field').hidden = !whole;
        expression.disabled = !whole;
        inputs.forEach(input => input.disabled = whole);
        document.querySelector('#equation-range').hidden = !numerical;
        bounds.forEach(input=>input.disabled = !numerical);
        precision.min = numerical ? '16' : '1'; precision.max = numerical ? '100' : '1000';
        if (numerical && (Number(precision.value)<16 || Number(precision.value)>100)) precision.value = '34';
        document.querySelector('.equation-settings summary').textContent = numerical ? (sk ? 'Výpočtová presnosť' : 'Calculation precision') : (sk ? 'Presnosť náhľadu' : 'Preview precision');
        document.querySelector('.equation-settings p').textContent = numerical ? (sk ? 'Presnosť výpočtu aj náhľadu. Tolerancia zostáva 1E-12; viac číslic nezaručuje nájdenie koreňa.' : 'Calculation and preview precision. Tolerances stay at 1E-12; more digits do not guarantee a root.') : (sk ? 'Presné korene zostávajú zachované. Desatinné náhľady sú približné.' : 'Exact roots are preserved. Decimal previews are approximate.');
        document.querySelector('#equation-help').textContent = mode.value === 'numeric_scan' ? scanWords.help : numerical ? numericWords.help : whole ? equationHelp : (sk
            ? 'Čísla alebo presné výrazy, napr. 1/3 či sqrt(4). Pre lineárnu rovnicu nastav a = 0.'
            : 'Numbers or exact expressions, e.g. 1/3 or sqrt(4). Use a = 0 for a linear equation.');
        renderExamples();
    }
    const examples = {
        expression:[['3x + 1 = 7','3x+1=7'],['x² = 2','x^2=2'],['x² + 1 = 0','x^2+1=0']],
        coefficients:[['1/3 · x − 2 = 0',['0','1/3','-2']],['x² − 2 = 0',['1','0','-2']],['(2x + 3)² = 0',['4','12','9']]],
        numeric:[['cos(x) = x','cos(x)=x','0','1'],['ln(x) = 2','ln(x)=2','1','10'],['√x = 2','sqrt(x)=2','0','8']],
        numeric_scan:[['sin(x) = 0','sin(x)=0','-10','10'],['(x − 1/3)² = 0','(x-1/3)^2=0','0','1'],['(x − 0.01)(x − 0.02) = 0','(x-0.01)*(x-0.02)=0','0','1']]
    };
    function renderExamples() {
        const container = document.querySelector('#equation-examples'); container.replaceChildren();
        for (const [index,[label]] of examples[mode.value].entries()) {
            const button = document.createElement('button'); button.type='button'; button.textContent=label;button.dataset.example=String(index);
            container.append(button);
        }
    }
    const $ = id => document.querySelector('#'+id);
    $('equation-examples').addEventListener('click',event=>{
        const button=event.target.closest('button[data-example]');if(!button)return;
        const [,value,low,high]=examples[mode.value][Number(button.dataset.example)];
        if (Array.isArray(value)) inputs.forEach((input,index)=>input.value=value[index]);
        else { expression.value=value;if(low!==undefined){bounds[0].value=low;bounds[1].value=high;} }
        edit();(mode.value==='coefficients'?inputs[0]:expression).focus({preventScroll:true});
    },{signal});
    const catalogueWords = sk ? {
        all:'Všetky kategórie', algebra:'Zápis', basic:'Základné', powers:'Mocniny a logaritmy', trig:'Goniometria', hyperbolic:'Hyperbolické', integers:'Celé čísla', stats:'Štatistika', constants:'Konštanty',
        noMatch:'Žiadna zhoda. Skús iný názov.', unavailable:'Knižnica nie je dostupná. Skús ju znovu otvoriť; zadanie môžeš písať ručne.',
        exact:'V presnom režime sú funkcie iba pre konštanty; výsledok musí byť presný a reálny. x patrí iba do polynómu do stupňa 2.',
        numeric:'Použi x a radiány. Argumenty oddeľuj bodkočiarkou; niektoré definičné obory nemusí riešič overiť.',
        coefficient:'Koeficient ', equation:'Rovnica v x', tooLong:'Zadanie môže mať najviac 4096 znakov.'
    } : {
        all:'All categories', algebra:'Notation', basic:'Basic', powers:'Powers & logs', trig:'Trigonometry', hyperbolic:'Hyperbolic', integers:'Integers', stats:'Statistics', constants:'Constants',
        noMatch:'No match. Try another name.', unavailable:'The library is unavailable. Try reopening it; you can still type the input.',
        exact:'In exact mode, functions apply to constants only and must yield an exact real value. x belongs only in a polynomial of degree at most 2.',
        numeric:'Use x and radians. Separate arguments with semicolons; some domains may remain unverified.',
        coefficient:'Coefficient ', equation:'Equation in x', tooLong:'The input must fit in 4096 characters.'
    };
    let cataloguePromise = null, registry = null, catalogueEntries = [], cataloguePage = 0, catalogueFailed = false;
    const category = $('equation-editor-category'), search = $('equation-editor-search'), help = $('equation-editor-help');
    const targets = $('equation-editor-target'), libraryStatus = $('equation-editor-status');
    const editorTitle=$('equation-editor-title').textContent;
    function syncEditor() {
        if (!editorTarget || !editor.open) return;
        editorTarget.value=editorInput.value; editorTarget.setSelectionRange(editorInput.selectionStart,editorInput.selectionEnd);
        editorTarget.dispatchEvent(new Event('input',{bubbles:true}));
    }
    function filterCatalogue(reset=true) {
        if (reset) cataloguePage=0;
        const library=window.numforgeFunctionLibrary;
        const height=window.visualViewport?.height||innerHeight, compact=height<=460;
        editor.toggleAttribute('data-compact',compact);
        editor.style.setProperty('--editor-height',Math.max(120,height-24)+'px');
        const perPage=compact?3:height<=700?6:9, query=library?.normalize(search.value.trim())||'';
        const matches=catalogueEntries.filter(entry=>(!category.value||entry.category===category.value)&&entry.searchable.includes(query));
        const pages=Math.max(1,Math.ceil(matches.length/perPage)); cataloguePage=Math.min(cataloguePage,pages-1);
        const shown=new Set(matches.slice(cataloguePage*perPage,(cataloguePage+1)*perPage));
        catalogueEntries.forEach(entry=>entry.button.hidden=!shown.has(entry));
        for (const tab of $('equation-editor-tabs').children) tab.setAttribute('aria-pressed',String(tab.dataset.category===category.value));
        $('equation-editor-page').textContent=`${cataloguePage+1} / ${pages}`;
        $('equation-editor-previous').disabled=cataloguePage===0; $('equation-editor-next').disabled=cataloguePage===pages-1;
        libraryStatus.hidden=registry?!!matches.length:!catalogueFailed; if (registry) libraryStatus.textContent=catalogueWords.noMatch;
    }
    function insertEditor(value,name='') {
        const start=editorInput.selectionStart??editorInput.value.length,end=editorInput.selectionEnd??start;
        const selected=editorInput.value.slice(start,end),arity=name?(registry.get(name)?.minimum_arguments||1):0;
        const insertion=name?name+'('+selected+';'.repeat(Math.max(0,arity-1))+')':value;
        if (editorInput.value.length-(end-start)+insertion.length>4096) { libraryStatus.hidden=false; libraryStatus.textContent=catalogueWords.tooLong; return; }
        editorInput.setRangeText(insertion,start,end,'end'); editorInput.focus({preventScroll:true});
        if (name) { const caret=selected?(arity>1?start+name.length+selected.length+2:start+insertion.length):start+name.length+1; editorInput.setSelectionRange(caret,caret); }
        syncEditor();
    }
    function renderCatalogue() {
        if (!registry) return;
        const library=window.numforgeFunctionLibrary, numeric=isNumeric(mode.value), coefficients=mode.value==='coefficients', query=search.value;
        const groups = numeric ? [
            ['basic',['abs','sign','min','max','floor','ceil','trunc','round']],['powers',['pow','sqrt','cbrt','root','exp','ln','log']],
            ['trig',['sin','cos','tan','asin','acos','atan','radians','degrees']],['hyperbolic',['sinh','cosh','tanh','asinh','acosh','atanh']],
            ['integers',['gcd','lcm','mod','npr','ncr','factorial','isqrt']],['stats',['sum','product','mean','median','geomean','harmean','variance','stdevp','stdev']]
        ] : [['basic',['abs','sign','min','max','sum','product']],['powers',['pow','sqrt','cbrt','root']],['integers',['gcd','lcm','mod','npr','ncr','factorial','isqrt']]];
        const buttons=$('equation-editor-functions'),tabs=$('equation-editor-tabs'); buttons.replaceChildren();tabs.replaceChildren();category.replaceChildren();catalogueEntries=[];
        category.add(new Option(catalogueWords.all,''));
        function add(label,value,terms,group,name='',description='') {
            const button=document.createElement('button');button.type='button';button.textContent=label;button.dataset.insert=value;button.dataset.function=name;button.title=description;
            buttons.append(button);catalogueEntries.push({button,category:group,searchable:library.normalize([label,value,description,...terms].join(' '))});
        }
        const groupIds=['algebra',...groups.map(([id])=>id),...(numeric?['constants']:[])];
        for (const id of groupIds) {
            category.add(new Option(catalogueWords[id],id)); const button=document.createElement('button');button.type='button';button.textContent=catalogueWords[id];button.dataset.category=id;
            tabs.append(button);
        }
        for (const [label,value] of [...(!coefficients?[['x','x'],['x²','x^2'],['=','=']]:[]),['+','+'],['−','-'],['×','*'],['÷','/'],['(', '('],[')',')'],[';',';'],['1/3','1/3']]) add(label,value,[],'algebra');
        for (const [id,names] of groups) for (const name of names) {
            const entry=registry.get(name),info=library.functionHelp[name];if(!entry?.implemented||!info)continue;
            const aliases=[...registry.values()].filter(e=>e.canonical===name).map(e=>e.name);
            const description=info[1]+' · '+info[sk?2:3].replaceAll('RAD/DEG',sk?'radiány':'radians');
            add(name,name+'(',[...aliases,...(library.functionSearchAliases[name]||[]),...(library.inverseFunctionAliases[name]||[])],id,name,description);
        }
        if (numeric) for (const [symbol,en,sl] of [['π','Pi','Pí'],['e','Euler’s number','Eulerovo číslo'],['φ','Golden ratio','Zlatý rez']]) add(symbol,symbol,library.constantAliasesFor(!sk)[symbol],'constants','',sk?sl:en);
        help.textContent=catalogueWords[numeric?'numeric':'exact'];category.value=query?'':'algebra';search.value=query;filterCatalogue();
    }
    async function loadCatalogue() {
        if (registry) { renderCatalogue(); return; }
        if (!cataloguePromise) { catalogueFailed=false; cataloguePromise=fetch('/api/functions',{signal}).then(async response=>{
            const data=await response.json();if(!response.ok||!data.ok||!Array.isArray(data.functions)||!window.numforgeFunctionLibrary)throw Error();
            registry=new Map(data.functions.map(entry=>[entry.name,entry]));
        }).catch(error=>{cataloguePromise=null;throw error;}); }
        try { await cataloguePromise;if(!signal.aborted)renderCatalogue(); }
        catch (error) { if(!signal.aborted&&error.name!=='AbortError') { catalogueFailed=true;libraryStatus.hidden=false;libraryStatus.textContent=catalogueWords.unavailable; } }
    }
    function openEditor() {
        if (editorTarget.disabled || ![expression,...inputs].includes(editorTarget)) editorTarget=mode.value==='coefficients'?inputs[0]:expression;
        targets.hidden=mode.value!=='coefficients'; targets.value=inputs.includes(editorTarget)?editorTarget.id.slice(-1):'expression';
        $('equation-editor-title').textContent=mode.value==='coefficients'?catalogueWords.coefficient+targets.value:editorTitle;
        editorInput.value=editorTarget.value;editorInput.setAttribute('aria-label',mode.value==='coefficients'?catalogueWords.coefficient+targets.value:catalogueWords.equation);
        editorInput.setSelectionRange(editorTarget.selectionStart,editorTarget.selectionEnd);submittingEditor=false;search.value='';
        editor.showModal();document.body.classList.add('equation-editor-open');filterCatalogue();editorInput.focus({preventScroll:true});loadCatalogue();
    }
    for (const input of [expression,...inputs]) input.addEventListener('focus',()=>editorTarget=input,{signal});
    $('equation-edit').addEventListener('click',openEditor,{signal});
    $('equation-editor-close').addEventListener('click',()=>editor.close(),{signal});
    editor.addEventListener('close',()=>{document.body.classList.remove('equation-editor-open');if(!signal.aborted&&!submittingEditor)editorTarget.focus({preventScroll:true});},{signal});
    editor.addEventListener('keydown',event=>{if(event.key==='Escape'){event.preventDefault();editor.close();}},{signal});
    editorInput.addEventListener('input',syncEditor,{signal});
    targets.addEventListener('change',()=>{editorTarget=inputs['abc'.indexOf(targets.value)];editorInput.value=editorTarget.value;$('equation-editor-title').textContent=catalogueWords.coefficient+targets.value;editorInput.setAttribute('aria-label',catalogueWords.coefficient+targets.value);editorInput.focus({preventScroll:true});},{signal});
    function submitEditor() { syncEditor();submittingEditor=true;editor.close();form.requestSubmit(); }
    $('equation-editor-solve').addEventListener('click',submitEditor,{signal});
    $('equation-editor-clear').addEventListener('click',()=>{editorInput.value='';syncEditor();editorInput.focus({preventScroll:true});},{signal});
    editorInput.addEventListener('keydown',event=>{if(event.key==='Enter'){event.preventDefault();submitEditor();}},{signal});
    search.addEventListener('input',()=>{if(search.value.trim())category.value='';filterCatalogue();},{signal});category.addEventListener('change',()=>filterCatalogue(),{signal});
    $('equation-editor-tabs').addEventListener('click',event=>{const button=event.target.closest('button[data-category]');if(button){category.value=button.dataset.category;search.value='';filterCatalogue();}},{signal});
    $('equation-editor-functions').addEventListener('click',event=>{const button=event.target.closest('button[data-insert]');if(button){help.textContent=button.title||catalogueWords[isNumeric(mode.value)?'numeric':'exact'];insertEditor(button.dataset.insert,button.dataset.function);}},{signal});
    $('equation-editor-functions').addEventListener('focusin',event=>{const button=event.target.closest('button[data-insert]');if(button)help.textContent=button.title||catalogueWords[isNumeric(mode.value)?'numeric':'exact'];},{signal});
    $('equation-editor-previous').addEventListener('click',()=>{--cataloguePage;filterCatalogue(false);},{signal});
    $('equation-editor-next').addEventListener('click',()=>{++cataloguePage;filterCatalogue(false);},{signal});
    window.addEventListener('resize',()=>{if(editor.open)filterCatalogue(false);},{signal});
    window.visualViewport?.addEventListener('resize',()=>{if(editor.open)filterCatalogue(false);},{signal});
    signal.addEventListener('abort',()=>{editor.close();document.body.classList.remove('equation-editor-open');},{once:true});
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
            if (isNumeric(mode.value)) target += '&min=' + encodeURIComponent(bounds[0].value) + '&max=' + encodeURIComponent(bounds[1].value);
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
            mode.value = ['expression','numeric','numeric_scan'].includes(saved.mode) ? saved.mode : 'coefficients';
            if (Array.isArray(saved.bounds) && saved.bounds.length === 2 && saved.bounds.every(value=>typeof value==='string'&&value.length<=47)) bounds.forEach((input,index)=>input.value=saved.bounds[index]);
            if (typeof saved.expression === 'string' && saved.expression.length <= 4096) expression.value = saved.expression;
            if (/^\d+$/.test(saved.precision) && Number(saved.precision) >= 1 && Number(saved.precision) <= 1000) precision.value = saved.precision;
            if (validResult(saved.result) && (!isNumeric(saved.result.kind) || saved.numericDomainVersion === 2) &&
                (saved.result.kind !== 'numeric_scan' || saved.scanCandidateVersion === 1)) result = saved.result;
        }
    } catch (_) { /* Ignore unavailable or obsolete state. */ }
    function validResult(value) {
        return value && ['finite','none','all','numeric','numeric_scan'].includes(value.kind) && Array.isArray(value.roots) && value.roots.length <= (value.kind === 'numeric_scan' ? 32 : 2) &&
            (value.kind !== 'numeric_scan' || ['scanned','root_limit','evaluation_limit'].includes(value.outcome) && value.complete === false && value.count === value.roots.length) &&
            (value.kind !== 'numeric' || Object.hasOwn(numericWords, value.outcome) && ['found','not_bracketed','domain_gap','domain_unverified','iteration_limit','precision_limit'].includes(value.outcome)) &&
            value.roots.every(root=>typeof root.approximation==='string' && (isNumeric(value.kind) ? typeof root.x==='string' && typeof root.residual==='string' : typeof root.exact==='string'));
    }
    syncMode(); render(result);
    return () => { save(); cancel(); lifecycle.abort(); };
}
window.numforgeInitEquations = initEquations;
window.numforgeDisposeEquations = initEquations();
})();
