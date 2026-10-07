(() => {
'use strict';
function initUnits() {
    const root = document.querySelector('.units-workspace');
    if (!root) return () => {};
    const lifecycle = new AbortController();
    const $ = id => root.querySelector('#' + id);
    const english = document.documentElement.lang === 'en';
    const text = english ? {
        preview:'Preview · Enter to confirm', confirmed:'Conversion confirmed', empty:'Enter a value or expression.',
        loading:'Loading units…', calculating:'Converting…', network:'Could not connect. Try again.',
        catalogue:'Could not load units. Try again.', settings:'Use whole numbers: working precision 1–10000, decimal places 0–10000.',
        copied:'Number copied to clipboard.', copyFailed:'Could not copy. Select the result and copy it manually.',
        common:'Common units', more:'Other units and prefixes', approximate:'Approximate calculation', exact:'Exact conversion factors',
        pi:'Angle conversion uses an approximation of π.', inputApprox:'The expression contains an approximate calculation.',
        display:'The displayed number follows your display and rounding settings.', source:'Definition sources:',
        readOnly:'Conversion does not change calculator variables, ans or history.',
        temperature:'Temperature points and temperature differences are separate categories. Values below absolute zero are converted mathematically.',
        information:'B = 8 bits. Decimal prefixes (MB) and binary prefixes (MiB) are distinct.',
        volume:'US and UK liquid measures have different definitions; their IDs distinguish them.',
        defaultNote:'Names and symbols are display labels; conversion uses the catalogue definitions.',
        errors:{unknown_unit:'Unknown unit.',incompatible_units:'These units are not compatible.',assignment_not_allowed:'Assignments are not allowed here. Define variables in the calculator.',random_not_allowed:'Random calls are not available in conversions.',session_expired:'The calculator session has expired. Return to the calculator to start a new session.',invalid_options:'Check the conversion settings.',time_limit:'The calculation took too long. Try a simpler expression.',value_too_large:'The value exceeds the calculation limits.',out_of_memory:'Not enough memory for this calculation.'},
        expressionErrors:{'division by zero':'Division by zero.','variable is undefined':'This variable is not defined in the calculator session.','ans is undefined':'Confirm a result in the calculator before using ans.','invalid argument':'Invalid argument.','domain error':'The expression is outside the function domain.'},
        expression:'Check the expression.', column:'Column'
    } : {
        preview:'Náhľad · Enter potvrdí prevod',confirmed:'Prevod potvrdený',empty:'Zadaj hodnotu alebo výraz.',
        loading:'Načítavam jednotky…',calculating:'Prevádzam…',network:'Nepodarilo sa pripojiť. Skús znova.',
        catalogue:'Jednotky sa nepodarilo načítať. Skús znova.',settings:'Použi celé čísla: výpočtová presnosť 1–10000, desatinné miesta 0–10000.',
        copied:'Číslo je skopírované.',copyFailed:'Kopírovanie sa nepodarilo. Označ výsledok a skopíruj ho ručne.',
        common:'Bežné jednotky',more:'Ostatné jednotky a predpony',approximate:'Približný výpočet',exact:'Presné prevodné faktory',
        pi:'Uhlový prevod používa približnú hodnotu π.',inputApprox:'Výraz obsahuje približný výpočet.',
        display:'Výpis čísla rešpektuje nastavené zobrazenie a zaokrúhľovanie.',source:'Zdroje definícií:',
        readOnly:'Prevod nemení premenné kalkulačky, ans ani históriu.',
        temperature:'Teplotné body a rozdiely teplôt sú samostatné kategórie. Hodnoty pod absolútnou nulou sa prevádzajú matematicky.',
        information:'B = 8 bitov. Desatinné predpony (MB) a binárne predpony (MiB) sú odlíšené.',
        volume:'Americké a britské objemové miery majú odlišné definície; rozlišujú ich ID.',
        defaultNote:'Názvy a symboly sú popisky; prevod používa definície katalógu.',
        errors:{unknown_unit:'Neznáma jednotka.',incompatible_units:'Tieto jednotky nie sú kompatibilné.',assignment_not_allowed:'Priradenia sem nepatria. Premenné definuj v kalkulačke.',random_not_allowed:'Náhodné výpočty nie sú pri prevode dostupné.',session_expired:'Sedenie kalkulačky už nie je dostupné. Vráť sa do kalkulačky a začni nové sedenie.',invalid_options:'Skontroluj nastavenia prevodu.',time_limit:'Výpočet trval príliš dlho. Skús jednoduchší výraz.',value_too_large:'Hodnota presahuje limity výpočtu.',out_of_memory:'Na výpočet nie je dostatok pamäte.'},
        expressionErrors:{'division by zero':'Delenie nulou.','variable is undefined':'Táto premenná nie je definovaná v sedení kalkulačky.','ans is undefined':'Pred použitím ans potvrď výsledok v kalkulačke.','invalid argument':'Neplatný argument.','domain error':'Výraz je mimo definičného oboru funkcie.'},
        expression:'Skontroluj výraz.',column:'Stĺpec'
    };
    const categories = [
        ['length','Dĺžka','Length','m', ['km','m','cm','mm','in','ft','yd','mi'],['km','m'],[['1/3','km','m'],['6','ft','m']]],
        ['area','Plocha','Area','m²',['m2','cm2','km2','ha','acre','ft2'],['m2','cm2'],[['1','ha','m2'],['1','acre','m2']]],
        ['volume','Objem','Volume','L',['L','mL','m3','cm3','gal_US','gal_UK','floz_US'],['L','mL'],[['1','gal_US','L'],['1','m3','L']]],
        ['mass','Hmotnosť','Mass','kg',['kg','g','mg','t','lb','oz'],['kg','g'],[['1','lb','kg'],['1/2','kg','g']]],
        ['time','Čas','Time','s',['s','ms','min','h','day'],['h','min'],[['1/3','h','min'],['1','day','h']]],
        ['speed','Rýchlosť','Speed','m/s',['km/h','m/s','mph','kn'],['km/h','m/s'],[['90','km/h','m/s'],['60','mph','km/h']]],
        ['temperature','Teplota','Temperature','°C',['degC','degF','K'],['degC','degF'],[['100','degC','degF'],['273.15','K','degC']]],
        ['temperature_interval','Rozdiel teplôt','Temp. difference','Δ°C',['deltaC','deltaF','deltaK'],['deltaC','deltaF'],[['10','deltaC','deltaF'],['18','deltaF','deltaC']]],
        ['information','Dáta','Data','B',['B','bit','kB','MB','GB','KiB','MiB','GiB'],['MiB','MB'],[['1','MiB','B'],['8','bit','B']]],
        ['angle','Uhol','Angle','°',['deg','rad','turn','gon','arcmin','arcsec'],['deg','rad'],[['180','deg','rad'],['1','turn','deg']]]
    ];
    let catalogue = [], category = 'length', generation = 0, timer = null;
    let conversion = null, catalogRequest = null, copyValue = '', disposed = false;
    let client = '';
    const input = $('unit-input'), from = $('unit-from'), to = $('unit-to');
    const status = $('unit-status'), result = $('unit-result'), meta = $('unit-result-meta');
    const fields = ['unit-precision','unit-places-mode','unit-places','unit-notation','unit-rounding','unit-angle'];
    let restored = null;
    const compact = window.matchMedia('(max-width: 620px)');
    try {
        const session = JSON.parse(sessionStorage.getItem('numforge-navigation-state') || 'null');
        if (session?.started === true && /^[0-9a-f]{32}$/.test(session.client)) client = session.client;
        restored = JSON.parse(sessionStorage.getItem('numforge-units-state') || 'null');
    } catch (_) {}
    input.value = typeof restored?.input === 'string' ? restored.input.slice(0,4096) : '1';
    if (categories.some(c=>c[0]===restored?.category)) category = restored.category;
    const allowed = {
        'unit-places-mode':['auto','full','custom'],'unit-notation':['auto','plain','scientific','fraction'],
        'unit-rounding':['half_even','half_up','toward_zero','away_from_zero','floor','ceiling'],'unit-angle':['rad','deg']
    };
    for (const id of fields) {
        const value = restored?.[id];
        if (typeof value !== 'string') continue;
        if (allowed[id]?.includes(value) || (!allowed[id] && /^\d+$/.test(value) && Number(value)<=10000 && (id==='unit-places' || Number(value)>=1))) $(id).value=value;
    }
    function save() {
        try { sessionStorage.setItem('numforge-units-state',JSON.stringify({input:input.value,category,from:from.value,to:to.value,...Object.fromEntries(fields.map(id=>[id,$(id).value]))})); } catch (_) {}
    }
    function setStatus(message, error=false) { status.textContent=message; status.classList.toggle('error',error); }
    function invalidate() {
        ++generation; clearTimeout(timer); conversion?.abort();
        copyValue=''; result.textContent=''; $('unit-result-symbol').textContent='';
        $('unit-copy').disabled=true; meta.hidden=true; $('unit-retry').hidden=true;
        return generation;
    }
    function updatePlaces() {
        const custom=$('unit-places-mode').value==='custom';
        $('unit-custom-places').hidden=!custom; $('unit-places').disabled=!custom;
    }
    updatePlaces();
    function setCategoryMenu(open) {
        $('unit-categories').hidden=compact.matches && !open;
        $('unit-category-toggle').setAttribute('aria-expanded',String(!$('unit-categories').hidden));
    }
    setCategoryMenu(false);
    compact.addEventListener('change',()=>setCategoryMenu(false),{signal:lifecycle.signal});
    $('unit-category-toggle').addEventListener('click',()=>setCategoryMenu($('unit-categories').hidden),{signal:lifecycle.signal});
    function info() {
        const node=$('unit-source-links'); node.replaceChildren(document.createTextNode(text.source+' '));
        [from.value,to.value].forEach((id,index)=>{
            const unit=catalogue.find(u=>u.id===id); if (!unit) return;
            if (index) node.append(document.createTextNode(' · '));
            const link=document.createElement('a'); link.textContent=unit[english?'name_en':'name_sk'];
            link.href=unit.source_url; link.target='_blank'; link.rel='noopener noreferrer'; node.append(link);
        });
        $('unit-category-note').textContent=(category.startsWith('temperature') ? text.temperature : category==='information'?text.information:category==='volume'?text.volume:text.defaultNote)+' '+text.readOnly;
    }
    function chooseCategory(key, pair) {
        category=key;
        const c=categories.find(c=>c[0]===key), units=catalogue.filter(u=>u.quantity===key);
        $('unit-category-label').textContent=c[english?2:1];
        $('unit-category-toggle').textContent=c[english?2:1]+' ▾';
        root.querySelectorAll('[data-quantity]').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.quantity===key)));
        for (const select of [from,to]) {
            select.replaceChildren();
            for (const [common,label] of [[true,text.common],[false,text.more]]) {
                const group=document.createElement('optgroup'); group.label=label;
                let subset=units.filter(u=>c[4].includes(u.id)===common);
                subset.sort(common?(a,b)=>c[4].indexOf(a.id)-c[4].indexOf(b.id):(a,b)=>a.id.localeCompare(b.id));
                for (const unit of subset) {
                    const option=document.createElement('option'); option.value=unit.id;
                    option.textContent=unit.symbol+' · '+unit[english?'name_en':'name_sk']; group.append(option);
                }
                if (subset.length) select.append(group);
            }
        }
        const selected=pair || c[5];
        from.value=units.some(u=>u.id===selected[0])?selected[0]:units[0].id;
        to.value=units.some(u=>u.id===selected[1])?selected[1]:units[0].id;
        $('unit-examples').replaceChildren();
        for (const [value,a,b] of c[6]) {
            const button=document.createElement('button'); button.type='button';
            button.textContent=`${value} ${a} → ${b}`;
            button.addEventListener('click',()=>{ input.value=value; from.value=a; to.value=b; info(); schedule(0); input.focus(); },{signal:lifecycle.signal});
            $('unit-examples').append(button);
        }
        info(); schedule(0);
    }
    async function calculate(confirm=false, id=generation) {
        if (disposed || !catalogue.length || id!==generation) return;
        const expression=input.value.trim();
        if (!expression) { setStatus(text.empty); return; }
        const precision=$('unit-precision').value, places=$('unit-places-mode').value==='full'?'full':$('unit-places-mode').value==='custom'?$('unit-places').value:'10';
        if (!/^\d+$/.test(precision) || Number(precision)<1 || Number(precision)>10000 ||
            (places!=='full' && (!/^\d+$/.test(places) || Number(places)>10000))) { setStatus(text.settings,true); return; }
        conversion=new AbortController();
        const query=new URLSearchParams({from:from.value,to:to.value,precision,places,rounding:$('unit-rounding').value,notation:$('unit-notation').value,angle:$('unit-angle').value});
        if (client) query.set('client',client);
        setStatus(text.calculating); root.setAttribute('aria-busy','true');
        try {
            const response=await fetch('/api/convert?'+query,{method:'POST',headers:{'Content-Type':'text/plain;charset=UTF-8'},body:expression,signal:conversion.signal});
            const body=await response.json();
            if (id!==generation || disposed) return;
            if (!response.ok || !body.ok) {
                const message=text.errors[body.code] || text.expressionErrors[body.status] || text.expression;
                setStatus(message+(Number.isSafeInteger(body.column)?` ${text.column} ${body.column}.`:''),true);
                return;
            }
            if (typeof body.result!=='string' || body.unit!==to.value || typeof body.symbol!=='string') throw Error('Invalid result');
            result.textContent=body.result; $('unit-result-symbol').textContent=body.symbol;
            copyValue=body.result; $('unit-copy').disabled=false;
            setStatus(confirm?text.confirmed:text.preview);
            meta.textContent=(body.input_approximate || body.factor_approximate?text.approximate+'. ':text.exact+'. ')+
                (body.input_approximate?text.inputApprox+' ':'')+(body.factor_approximate?text.pi+' ':'')+text.display;
            meta.hidden=false;
        } catch (error) {
            if (id!==generation || disposed || error.name==='AbortError') return;
            setStatus(text.network,true); $('unit-retry').hidden=false;
        } finally { if (id===generation) root.removeAttribute('aria-busy'); }
    }
    function schedule(delay=250) {
        const id=invalidate(); root.removeAttribute('aria-busy'); save();
        if (!input.value.trim()) { setStatus(text.empty); return; }
        setStatus(catalogue.length?text.calculating:text.loading);
        timer=setTimeout(()=>calculate(false,id),delay);
    }
    $('unit-converter').addEventListener('submit',event=>{event.preventDefault(); const id=invalidate(); save(); calculate(true,id);},{signal:lifecycle.signal});
    input.addEventListener('keydown',event=>{if(event.key==='Enter' && !event.shiftKey && !event.isComposing){event.preventDefault();$('unit-converter').requestSubmit();}},{signal:lifecycle.signal});
    input.addEventListener('input',()=>schedule(),{signal:lifecycle.signal});
    for (const id of [...fields,'unit-from','unit-to']) $(id).addEventListener('change',()=>{updatePlaces();info();schedule(0);},{signal:lifecycle.signal});
    $('unit-swap').addEventListener('click',()=>{const a=from.value;from.value=to.value;to.value=a;info();schedule(0);},{signal:lifecycle.signal});
    $('unit-copy').addEventListener('click',async()=>{
        const value=copyValue, id=generation; if (!value) return;
        try { await navigator.clipboard.writeText(value); if(id===generation && !disposed)setStatus(text.copied); }
        catch (_) { if(id===generation && !disposed)setStatus(text.copyFailed,true); }
    },{signal:lifecycle.signal});
    $('unit-retry').addEventListener('click',()=>catalogue.length?schedule(0):loadCatalogue(),{signal:lifecycle.signal});
    window.addEventListener('numforge:navigate',save,{signal:lifecycle.signal});
    async function loadCatalogue() {
        catalogRequest?.abort(); catalogRequest=new AbortController();
        const controller=catalogRequest;
        $('unit-retry').hidden=true;setStatus(text.loading);
        try {
            const response=await fetch('/api/units',{signal:controller.signal}); const body=await response.json();
            if(disposed || controller!==catalogRequest) return;
            if(!response.ok || !body.ok || !Array.isArray(body.units) || !body.units.length ||
                !body.units.every(u=>typeof u.id==='string' && typeof u.symbol==='string' && typeof u.name_sk==='string' && typeof u.name_en==='string' && /^https:\/\//.test(u.source_url) && categories.some(c=>c[0]===u.quantity)) ||
                categories.some(c=>!body.units.some(u=>u.quantity===c[0]))) throw Error('Invalid catalogue');
            catalogue=body.units;
            $('unit-categories').replaceChildren();
            for (const c of categories) {
                const button=document.createElement('button'); button.type='button'; button.dataset.quantity=c[0];
                button.textContent=c[english?2:1]; const symbol=document.createElement('span');symbol.textContent=c[3];symbol.setAttribute('aria-hidden','true');button.append(symbol);
                button.addEventListener('click',()=>{
                    chooseCategory(c[0]);
                    if(compact.matches){setCategoryMenu(false);$('unit-category-toggle').focus();}
                },{signal:lifecycle.signal}); $('unit-categories').append(button);
            }
            for(const id of ['unit-from','unit-to','unit-swap','unit-submit']) $(id).disabled=false;
            chooseCategory(category,restored?[restored.from,restored.to]:null); restored=null;
        } catch(error) { if (!disposed && error.name!=='AbortError') {setStatus(text.catalogue,true);$('unit-retry').hidden=false;} }
    }
    loadCatalogue();
    return ()=>{ save();disposed=true;lifecycle.abort();invalidate();catalogRequest?.abort(); };
}
window.numforgeInitUnits=initUnits;
if(document.querySelector('#unit-converter')) window.numforgeDisposeUnits=initUnits();
})();
