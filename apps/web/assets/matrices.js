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
    const operations=['determinant','rank','multiply','solve','solve_general'];
    const defaults=()=>({operation:'determinant',a:[['1','2'],['3','4']],b:[['5','6'],['7','8']]});
    const valid=(m,limit=64)=>Array.isArray(m)&&m.length>=1&&m.length<=8&&Array.isArray(m[0])&&m[0].length>=1&&m[0].length<=8&&
        m.every(row=>Array.isArray(row)&&row.length===m[0].length&&row.every(cell=>typeof cell==='string'&&cell.length<=limit&&!/[\u0000-\u001f\u007f]/.test(cell)));
    const validState=s=>s&&operations.includes(s.operation)&&valid(s.a)&&valid(s.b);
    const matrixText=m=>m.map(row=>row.map(s=>s.trim()).join('\t')).join('\n');
    const canonical=value=>typeof value==='string'&&/^-?\d+(?:\/[1-9]\d*)?$/.test(value);
    const validResult=m=>valid(m,128*1024)&&m.every(row=>row.every(canonical));
    let state=defaults(), request=null, generation=0, copied='';
    try {
        const saved=JSON.parse(sessionStorage.getItem(key));
        if(saved?.version===1&&validState(saved)) state={operation:saved.operation,a:saved.a,b:saved.b};
    }catch{}
    function save() {try{sessionStorage.setItem(key,JSON.stringify({version:1,...state}));}catch{}}
    function invalidate() {
        ++generation;request?.abort();request=null;submit.disabled=false;
        output.replaceChildren();copied='';copy.disabled=true;error.hidden=true;
        form.removeAttribute('aria-busy');status.dataset.state='idle';
        status.textContent=t('Uprav maticu a zvoľ Vypočítať.','Edit the matrix and choose Calculate.');
        form.querySelectorAll('[aria-invalid]').forEach(e=>e.removeAttribute('aria-invalid'));
    }
    function render(name) {
        const matrix=state[name], table=document.querySelector('#matrix-'+name);
        table.replaceChildren();
        const header=document.createElement('tr');header.append(document.createElement('th'));
        matrix[0].forEach((_,c)=>{const th=document.createElement('th');th.scope='col';th.textContent=c+1;th.dataset.column=c;header.append(th);});table.append(header);
        matrix.forEach((row,r)=>{
            const tr=document.createElement('tr');
            const th=document.createElement('th');th.scope='row';th.textContent=r+1;tr.append(th);
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
        document.querySelector(`#matrix-${name}-shape`).textContent=`${matrix.length} × ${matrix[0].length}`;
        document.querySelector(`[data-identity="${name}"]`).disabled=matrix.length!==matrix[0].length;
    }
    function mode() {
        const binary=['multiply','solve','solve_general'].includes(state.operation);
        form.dataset.binary=String(binary);document.querySelector('#matrix-swap').hidden=!binary;
        document.querySelector('#matrix-panel-b').hidden=!binary;
        document.querySelectorAll('#matrix-b input').forEach(input=>input.disabled=!binary);
        document.querySelector('#matrix-operation-help').textContent=state.operation==='solve_general'
            ? t('Aj obdĺžnikové a singulárne sústavy. B potrebuje jeden stĺpec a rovnaký počet riadkov ako A.','Includes rectangular and singular systems. B needs one column and the same number of rows as A.')
            : state.operation==='solve'
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
        const button=event.target.closest('[data-clear],[data-identity],[data-transpose]');if(!button)return;
        const name=button.dataset.clear||button.dataset.identity||button.dataset.transpose, matrix=state[name];
        state[name]=button.dataset.transpose ? matrix[0].map((_,c)=>matrix.map(row=>row[c]))
            : matrix.map((row,r)=>row.map((_,c)=>button.dataset.identity&&r===c?'1':'0'));
        invalidate();render(name);mode();save();
    },options);
    form.addEventListener('focusin',event=>{
        const input=event.target;if(!input.dataset.matrix)return;
        const table=document.querySelector('#matrix-'+input.dataset.matrix);
        table.querySelector('.active-column')?.classList.remove('active-column');
        table.querySelector(`th[data-column="${input.dataset.column}"]`)?.classList.add('active-column');
    },options);
    form.addEventListener('keydown',event=>{
        if(event.key==='Enter'&&(event.ctrlKey||event.metaKey)) {event.preventDefault();if(!submit.disabled)form.requestSubmit();return;}
        const input=event.target,name=input.dataset.matrix;if(!name)return;
        let row=Number(input.dataset.row),column=Number(input.dataset.column);
        if(event.key==='Enter')row+=event.shiftKey?-1:1;
        else if(event.altKey&&['ArrowUp','ArrowDown','ArrowLeft','ArrowRight'].includes(event.key)) {
            row+=(event.key==='ArrowDown')-(event.key==='ArrowUp');column+=(event.key==='ArrowRight')-(event.key==='ArrowLeft');
        }else return;
        event.preventDefault();
        document.querySelector(`#matrix-${name} input[data-row="${row}"][data-column="${column}"]`)?.focus();
    },options);
    document.querySelector('#matrix-swap').addEventListener('click',()=>{
        [state.a,state.b]=[state.b,state.a];invalidate();render('a');render('b');mode();save();
    },options);
    document.querySelector('#matrix-preset').addEventListener('change',event=>{
        const presets={unique:{a:[['1','2'],['3','4']],b:[['5'],['6']]},infinite:{a:[['1','2','3'],['2','4','6']],b:[['1'],['2']]},
            none:{a:[['1','2'],['2','4']],b:[['1'],['3']]},fractions:{operation:'determinant',a:[['1/2','0.25'],['-2','3']],b:[['0'],['0']]}};
        const preset=presets[event.target.value];if(!preset)return;
        state={operation:preset.operation||'solve_general',a:preset.a,b:preset.b};operation.value=state.operation;
        invalidate();render('a');render('b');mode();save();event.target.value='';
    },options);
    document.querySelector('#matrix-example').addEventListener('click',()=>{
        state=defaults();operation.value=state.operation;invalidate();render('a');render('b');mode();save();
    },options);
    const fileInput=document.querySelector('#matrix-file');
    document.querySelector('#matrix-load').addEventListener('click',()=>fileInput.click(),options);
    fileInput.addEventListener('change',async()=>{
        const file=fileInput.files[0], id=generation;fileInput.value='';if(!file)return;
        try {
            if(file.size>20000)throw Error('large');
            const data=JSON.parse(await file.text());
            if(lifecycle.signal.aborted||id!==generation)return;
            if(data?.format!=='numforge.matrix-workspace'||data.version!==1||!validState(data))throw Error('invalid');
            state={operation:data.operation,a:data.a,b:data.b};operation.value=state.operation;
            invalidate();render('a');render('b');mode();save();status.textContent=t('Úloha načítaná.','Workspace loaded.');
        }catch{if(!lifecycle.signal.aborted&&id===generation)showError(t('Súbor nie je platná úloha NumForge (najviac 20 kB). Vstupy zostali zachované.','Invalid NumForge workspace file (maximum 20 kB). Inputs were preserved.'));}
    },options);
    document.querySelector('#matrix-save').addEventListener('click',()=>{
        const blob=new Blob([JSON.stringify({format:'numforge.matrix-workspace',version:1,operation:state.operation,a:state.a,b:state.b},null,2)],{type:'application/json'});
        if(blob.size>20000){showError(t('Úloha je príliš veľká na uloženie (20 kB).','Workspace exceeds the 20 kB file limit.'));return;}
        const url=URL.createObjectURL(blob),link=document.createElement('a');link.href=url;link.download='numforge-matrices.json';link.click();
        setTimeout(()=>URL.revokeObjectURL(url),1000);
    },options);
    form.addEventListener('paste',event=>{
        const target=event.target;if(!target.dataset.matrix)return;
        const text=event.clipboardData?.getData('text/plain');if(!text||!/[\t\r\n]/.test(text))return;
        const matrix=text.replace(/\r\n?/g,'\n').replace(/\n$/,'').split('\n').map(row=>row.split('\t').map(cell=>cell.trim()));
        event.preventDefault();
        if(!valid(matrix)){showError(t('Tabuľka musí byť obdĺžniková, najviac 8 × 8 a 64 znakov na bunku. Vstupy zostali zachované.','The table must be rectangular, at most 8 × 8 and 64 characters per cell. Inputs were preserved.'));return;}
        const name=target.dataset.matrix;
        state[name]=matrix;invalidate();render(name);mode();save();
        document.querySelector(`#matrix-${name} input`).focus();
    },options);
    function showError(message,data) {
        error.textContent=message;error.hidden=false;status.dataset.state='error';status.textContent=t('Skontroluj zadanie.','Check the input.');
        if(data?.matrix===0||data?.matrix===1) {
            const input=document.querySelectorAll(`#matrix-${data.matrix?'b':'a'} input`)[data.cell];
            input?.setAttribute('aria-invalid','true');input?.focus();
        }
    }
    function tableResult(matrix,labels) {
        const table=document.createElement('table');table.setAttribute('aria-label',t('Presný výsledok','Exact result'));
        if(labels) {const tr=document.createElement('tr');tr.append(document.createElement('th'));labels.forEach(label=>{const th=document.createElement('th');th.scope='col';th.textContent=label;tr.append(th);});table.append(tr);}
        matrix.forEach((row,r)=>{const tr=document.createElement('tr');if(labels){const th=document.createElement('th');th.scope='row';th.textContent='x'+(r+1);tr.append(th);}row.forEach(value=>{const td=document.createElement('td');td.textContent=value;tr.append(td);});table.append(tr);});return table;
    }
    function solutionResult(solution) {
        const {kind,rank,nullity,particular,basis,free_columns:free}=solution, n=state.a[0].length;
        if(!['none','unique','infinite'].includes(kind)||!Number.isInteger(rank)||rank<0||rank>Math.min(n,state.a.length)||
            !Number.isInteger(nullity)||nullity!==n-rank||!Array.isArray(free)||free.length!==nullity||
            !free.every((v,i)=>Number.isInteger(v)&&v>=0&&v<n&&(!i||v>free[i-1]))||
            (kind==='none'?particular!==null:!validResult(particular)||particular.length!==n||particular[0].length!==1)||
            (nullity? !validResult(basis)||basis.length!==n||basis[0].length!==nullity:basis!==null)||
            (kind==='unique'&&nullity!==0)||(kind==='infinite'&&nullity===0))throw Error('Invalid solution');
        const heading=document.createElement('h3');heading.className='solution-title';
        heading.textContent=kind==='none'?t('Sústava nemá riešenie','The system has no solution'):kind==='unique'?t('Jedno riešenie','Unique solution'):t('Nekonečne veľa riešení','Infinitely many solutions');output.append(heading);
        const meta=document.createElement('p');meta.className='solution-meta';meta.textContent=`rank(A) = ${rank} · ${kind==='none'?'dim ker(A)':t('Voľné parametre','Free parameters')}: ${nullity}`;output.append(meta);
        if(kind==='none') {copied='Ax = b: ∅';return;}
        const vector=m=>'['+m.map(row=>row[0]).join('; ')+']';
        if(kind==='unique') {output.append(tableResult(particular,['x']));copied=matrixText(particular);return;}
        const formula=document.createElement('p');formula.className='solution-formula';formula.textContent='x = x₀ + N · t';output.append(formula);
        const panels=document.createElement('div');panels.className='solution-panels';
        for(const [label,matrix,labels] of [['x₀',particular,['x₀']],['N',basis,free.map((_,i)=>'t'+(i+1))]]) {
            const block=document.createElement('section'),h=document.createElement('h4'),scroll=document.createElement('div');h.textContent=label;scroll.className='matrix-scroll';scroll.tabIndex=0;scroll.append(tableResult(matrix,labels));block.append(h,scroll);panels.append(block);
        }output.append(panels);
        const hint=document.createElement('p');hint.className='matrix-help';hint.textContent=t('Parametre t sú ľubovoľné racionálne čísla. Každý stĺpec N patrí jednému parametru.','Parameters t are arbitrary rational numbers. Each column of N belongs to one parameter.');output.append(hint);
        copied='x = '+vector(particular)+basis[0].map((_,i)=>' + t'+(i+1)+' · '+vector(basis.map(row=>[row[i]]))).join('')+'; t ∈ ℚ';
    }
    form.addEventListener('submit',async event=>{
        event.preventDefault();invalidate();const id=generation;
        const controller=new AbortController();request=controller;submit.disabled=true;
        status.textContent=t('Počítam…','Calculating…');status.dataset.state='busy';form.setAttribute('aria-busy','true');
        const bad=[...form.querySelectorAll('input:not(:disabled)')].find(input=>
            !/^(?:[+-]?(?:\d+(?:\.\d*)?|\.\d+)|[+-]?\d+\/[+-]?\d+)$/.test(input.value.trim()));
        if(bad) {
            showError(t('Bunka musí obsahovať číslo alebo zlomok, napríklad -2, 0.5 alebo 1/3.','A cell must contain a number or fraction, for example -2, 0.5 or 1/3.'));
            bad.setAttribute('aria-invalid','true');bad.focus();submit.disabled=false;form.removeAttribute('aria-busy');return;
        }
        const binary=['multiply','solve','solve_general'].includes(state.operation);
        const shapeError=(['determinant','solve'].includes(state.operation)&&state.a.length!==state.a[0].length)||
            (binary&&(state.operation==='multiply'?state.a[0].length!==state.b.length:state.a.length!==state.b.length))||
            (state.operation==='solve_general'&&state.b[0].length!==1);
        if(shapeError){showError(t('Rozmery nezodpovedajú operácii. Pomôcka pod maticami uvádza požadované rozmery.','Dimensions do not match the operation. The hint below the matrices describes the required shapes.'));submit.disabled=false;form.removeAttribute('aria-busy');return;}
        const body=matrixText(state.a)+(binary?'\n\n'+matrixText(state.b):'');
        if(body.length>6000) {showError(t('Zadanie je príliš dlhé (najviac 6000 znakov).','Input is too long (maximum 6000 characters).'));submit.disabled=false;form.removeAttribute('aria-busy');return;}
        const timeout=setTimeout(()=>controller.abort(),15000);
        try {
            const response=await fetch('/api/matrix?operation='+state.operation,{method:'POST',headers:{'Content-Type':'text/plain'},body,signal:controller.signal});
            const data=await response.json();if(id!==generation)return;
            if(!response.ok||!data.ok) {
                const message=data.status==='division by zero'&&data.cell===-1&&state.operation==='solve'
                    ? t('A je singulárna. Skús Všeobecnú sústavu s jedným stĺpcom B.','A is singular. Try General system with one column in B.')
                    : data.status==='invalid argument' ? t('Skontroluj rozmery a bunky: celé čísla, zlomky alebo konečné desatinné čísla s bodkou.','Check dimensions and cells: integers, fractions or finite dot decimals.')
                    : t('Výpočet sa nepodaril: ','Calculation failed: ')+(data.status||data.code||response.status);
                showError(message,data);return;
            }
            if(data.exact!==true)throw Error('Invalid result');
            if(data.solution) solutionResult(data.solution);
            else if(canonical(data.scalar)) {
                const p=document.createElement('p');p.textContent=data.scalar;output.append(p);copied=data.scalar;
            }else if(validResult(data.matrix)) {
                output.append(tableResult(data.matrix));copied=matrixText(data.matrix);
            }else throw Error('Invalid result');
            copy.disabled=false;status.dataset.state='ready';status.textContent=t('Hotovo · Exaktný výsledok','Done · Exact result');
        }catch(failure) {
            if(id===generation){output.replaceChildren();showError(t('Server neodpovedal alebo odpoveď nie je platná. Skús výpočet znova.','The server did not respond or returned an invalid response. Try again.'));}
        }finally{clearTimeout(timeout);if(id===generation){submit.disabled=false;form.removeAttribute('aria-busy');request=null;}}
    },options);
    copy.addEventListener('click',async()=>{
        const id=generation;
        try{await navigator.clipboard.writeText(copied);if(id===generation)status.textContent=t('Výsledok skopírovaný.','Result copied.');}
        catch{if(id===generation)status.textContent=t('Kopírovanie nie je dostupné. Označ a skopíruj výsledok ručne.','Copy is unavailable. Select and copy the result manually.');}
    },options);
    return ()=>{++generation;request?.abort();lifecycle.abort();save();};
}
window.numforgeInitMatrices=initMatrices;
window.numforgeDisposeMatrices=initMatrices();
