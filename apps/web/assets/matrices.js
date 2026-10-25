'use strict';
function initMatrices() {
    const form=document.querySelector('#matrix-form');
    if(!form) return ()=>{};
    const lifecycle=new AbortController(), options={signal:lifecycle.signal};
    const sk=document.documentElement.lang==='sk', t=(a,b)=>sk?a:b;
    const operation=document.querySelector('#matrix-operation');
    const output=document.querySelector('#matrix-output'), status=document.querySelector('#matrix-status');
    const error=document.querySelector('#matrix-error'), submit=document.querySelector('#matrix-submit');
    const copy=document.querySelector('#matrix-copy'), key='numforge-matrix-state';
    const operations=['determinant','rank','multiply','solve'];
    const defaults=()=>({operation:'determinant',a:[['1','2'],['3','4']],b:[['5','6'],['7','8']]});
    const valid=(m,limit=64)=>Array.isArray(m)&&m.length>=1&&m.length<=8&&Array.isArray(m[0])&&m[0].length>=1&&m[0].length<=8&&
        m.every(row=>Array.isArray(row)&&row.length===m[0].length&&row.every(cell=>typeof cell==='string'&&cell.length<=limit));
    let state=defaults(), request=null, generation=0, copied='';
    try {
        const saved=JSON.parse(sessionStorage.getItem(key));
        if(saved?.version===1&&operations.includes(saved.operation)&&valid(saved.a)&&valid(saved.b)) state=saved;
    }catch{}
    function save() {try{sessionStorage.setItem(key,JSON.stringify({version:1,...state}));}catch{}}
    function invalidate() {
        ++generation;request?.abort();request=null;submit.disabled=false;
        output.replaceChildren();copied='';copy.disabled=true;error.hidden=true;
        status.textContent=t('Uprav maticu a zvoľ Vypočítať.','Edit the matrix and choose Calculate.');
        form.querySelectorAll('[aria-invalid]').forEach(e=>e.removeAttribute('aria-invalid'));
    }
    function render(name) {
        const matrix=state[name], table=document.querySelector('#matrix-'+name);
        table.replaceChildren();
        matrix.forEach((row,r)=>{
            const tr=document.createElement('tr');
            row.forEach((value,c)=>{
                const td=document.createElement('td'), input=document.createElement('input');
                input.type='text';input.value=value;input.maxLength=64;input.required=true;
                input.autocomplete='off';input.spellcheck=false;
                input.setAttribute('aria-label',`${name.toUpperCase()} · ${t('riadok','row')} ${r+1}, ${t('stĺpec','column')} ${c+1}`);
                input.dataset.matrix=name;input.dataset.row=r;input.dataset.column=c;
                td.append(input);tr.append(td);
            });table.append(tr);
        });
        document.querySelector(`#matrix-${name}-rows`).value=String(matrix.length);
        document.querySelector(`#matrix-${name}-columns`).value=String(matrix[0].length);
    }
    function mode() {
        const binary=['multiply','solve'].includes(state.operation);
        document.querySelector('#matrix-panel-b').hidden=!binary;
        document.querySelectorAll('#matrix-b input').forEach(input=>input.disabled=!binary);
        document.querySelector('#matrix-operation-help').textContent=state.operation==='solve'
            ? t('A musí byť štvorcová a nesingulárna; B musí mať rovnaký počet riadkov. Každý stĺpec B je samostatná pravá strana.','A must be square and nonsingular; B must have the same number of rows. Each column of B is a separate right-hand side.')
            : state.operation==='multiply' ? t('Počet stĺpcov A musí zodpovedať počtu riadkov B.','The columns of A must match the rows of B.')
            : state.operation==='determinant' ? t('Determinant vyžaduje štvorcovú maticu.','The determinant requires a square matrix.')
            : t('Hodnosť funguje aj pre obdĺžnikové matice.','Rank also supports rectangular matrices.');
    }
    operation.value=state.operation;render('a');render('b');mode();
    form.addEventListener('input',event=>{
        const input=event.target;if(!input.dataset.matrix)return;
        state[input.dataset.matrix][Number(input.dataset.row)][Number(input.dataset.column)]=input.value;
        invalidate();save();
    },options);
    form.addEventListener('change',event=>{
        if(event.target===operation) {state.operation=operation.value;invalidate();mode();save();return;}
        const match=event.target.id.match(/^matrix-([ab])-(rows|columns)$/);if(!match)return;
        const name=match[1], rows=Number(document.querySelector(`#matrix-${name}-rows`).value), columns=Number(document.querySelector(`#matrix-${name}-columns`).value);
        state[name]=Array.from({length:rows},(_,r)=>Array.from({length:columns},(_,c)=>state[name][r]?.[c]??'0'));
        invalidate();render(name);mode();save();
    },options);
    form.addEventListener('click',event=>{
        const name=event.target.closest('[data-clear]')?.dataset.clear;if(!name)return;
        state[name]=state[name].map(row=>row.map(()=>'0'));invalidate();render(name);mode();save();
    },options);
    document.querySelector('#matrix-example').addEventListener('click',()=>{
        state=defaults();operation.value=state.operation;invalidate();render('a');render('b');mode();save();
    },options);
    form.addEventListener('paste',event=>{
        const target=event.target;if(!target.dataset.matrix)return;
        const text=event.clipboardData?.getData('text/plain');if(!text||!/[\t\n]/.test(text))return;
        const matrix=text.replace(/\r/g,'').trim().split('\n').map(row=>row.split('\t').map(cell=>cell.trim()));
        if(!valid(matrix))return;
        event.preventDefault();const name=target.dataset.matrix;
        state[name]=matrix;invalidate();render(name);mode();save();
        document.querySelector(`#matrix-${name} input`).focus();
    },options);
    function showError(message,data) {
        error.textContent=message;error.hidden=false;status.textContent=t('Skontroluj zadanie.','Check the input.');
        if(data?.matrix===0||data?.matrix===1) {
            const input=document.querySelectorAll(`#matrix-${data.matrix?'b':'a'} input`)[data.cell];
            input?.setAttribute('aria-invalid','true');input?.focus();
        }
    }
    form.addEventListener('submit',async event=>{
        event.preventDefault();invalidate();const id=generation;
        const controller=new AbortController();request=controller;submit.disabled=true;
        status.textContent=t('Počítam…','Calculating…');
        const bad=[...form.querySelectorAll('input:not(:disabled)')].find(input=>
            !/^(?:[+-]?(?:\d+(?:\.\d*)?|\.\d+)|[+-]?\d+\/[+-]?\d+)$/.test(input.value.trim()));
        if(bad) {
            showError(t('Bunka musí obsahovať číslo alebo zlomok, napríklad -2, 0.5 alebo 1/3.','A cell must contain a number or fraction, for example -2, 0.5 or 1/3.'));
            bad.setAttribute('aria-invalid','true');bad.focus();submit.disabled=false;return;
        }
        const matrixText=m=>m.map(row=>row.map(s=>s.trim()).join('\t')).join('\n');
        const body=matrixText(state.a)+(['multiply','solve'].includes(state.operation)?'\n\n'+matrixText(state.b):'');
        if(body.length>6000) {showError(t('Zadanie je príliš dlhé (najviac 6000 znakov).','Input is too long (maximum 6000 characters).'));submit.disabled=false;return;}
        const timeout=setTimeout(()=>controller.abort(),15000);
        try {
            const response=await fetch('/api/matrix?operation='+state.operation,{method:'POST',headers:{'Content-Type':'text/plain'},body,signal:controller.signal});
            const data=await response.json();if(id!==generation)return;
            if(!response.ok||!data.ok) {
                const message=data.status==='division by zero'&&data.cell===-1&&state.operation==='solve'
                    ? t('A je singulárna. Táto verzia rieši iba sústavy s jednoznačným riešením.','A is singular. This version solves only systems with a unique solution.')
                    : data.status==='invalid argument' ? t('Skontroluj rozmery a bunky: celé čísla, zlomky alebo konečné desatinné čísla s bodkou.','Check dimensions and cells: integers, fractions or finite dot decimals.')
                    : t('Výpočet sa nepodaril: ','Calculation failed: ')+(data.status||data.code||response.status);
                showError(message,data);return;
            }
            if(typeof data.scalar==='string') {
                const p=document.createElement('p');p.textContent=data.scalar;output.append(p);copied=data.scalar;
            }else if(valid(data.matrix,128*1024)) {
                const table=document.createElement('table');table.setAttribute('aria-label',t('Presný výsledok','Exact result'));
                data.matrix.forEach(row=>{const tr=document.createElement('tr');row.forEach(value=>{const td=document.createElement('td');td.textContent=value;tr.append(td);});table.append(tr);});
                output.append(table);copied=matrixText(data.matrix);
            }else throw Error('Invalid result');
            copy.disabled=false;status.textContent=t('Hotovo · Exaktný výsledok','Done · Exact result');
        }catch(failure) {
            if(id===generation)showError(t('Server neodpovedal alebo odpoveď nie je platná. Skús výpočet znova.','The server did not respond or returned an invalid response. Try again.'));
        }finally{clearTimeout(timeout);if(id===generation){submit.disabled=false;request=null;}}
    },options);
    copy.addEventListener('click',async()=>{
        try{await navigator.clipboard.writeText(copied);status.textContent=t('Výsledok skopírovaný.','Result copied.');}
        catch{status.textContent=t('Kopírovanie nie je dostupné. Označ a skopíruj výsledok ručne.','Copy is unavailable. Select and copy the result manually.');}
    },options);
    return ()=>{++generation;request?.abort();lifecycle.abort();save();};
}
window.numforgeInitMatrices=initMatrices;
window.numforgeDisposeMatrices=initMatrices();
