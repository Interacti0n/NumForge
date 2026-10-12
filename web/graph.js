function initGraph() {
    const root = document.querySelector('.graph-workspace');
    if (!root) return null;
    const $ = id => root.querySelector('#' + id);
    const english = document.documentElement.lang === 'en';
    const lifecycle = new AbortController();
    const fields = ['expression','parameter','min','max','count','precision','angle','session','connect'];
    const status = $('graph-status'), svg = $('graph-plot'), table = $('graph-table-body');
    let generation = 0, request = null, samples = null, disposed = false;
    const text = english ? {
        ready:'Enter a function and plot it.', busy:'Sampling…', network:'Cannot reach the server. Try again.',
        invalid:'The server returned invalid graph data.', session:'Start a calculator session before using its variables.',
        range:'These coordinates cannot be distinguished in the browser’s numeric range. Choose a smaller range.',
        real:'Real', domain_error:'Domain error', complex:'Complex', quantity:'Quantity', outside:'Outside plotting range',
        summary:(shown,total)=>`${shown} of ${total} samples plotted`, column:'column',
        caption:'Rounded plotting values. Complex values and quantities are not projected onto the real axis.'
    } : {
        ready:'Zadaj funkciu a vykresli ju.', busy:'Počítam body…', network:'Server nie je dostupný. Skús to znova.',
        invalid:'Server vrátil neplatné údaje grafu.', session:'Pred použitím premenných vytvor sedenie v kalkulačke.',
        range:'Tieto súradnice prehliadač nedokáže rozlíšiť vo svojom číselnom rozsahu. Zvoľ menší rozsah.',
        real:'Reálne', domain_error:'Chyba definičného oboru', complex:'Komplexné', quantity:'Veličina', outside:'Mimo rozsahu grafu',
        summary:(shown,total)=>`Vykreslené body: ${shown} z ${total}`, column:'pozícia',
        caption:'Zaokrúhlené hodnoty pre graf. Komplexné čísla a veličiny sa nepremietajú na reálnu os.'
    };
    const errors = english ? {} : {
        'division by zero':'Delenie nulou', 'undefined variable':'Nedefinovaná premenná',
        'undefined answer':'Nie je uložený výsledok ans', 'syntax error':'Chyba syntaxe',
        'time limit exceeded':'Prekročený časový limit', 'invalid argument':'Neplatný argument',
        'session expired; reload the page':'Sedenie vypršalo; obnov stránku'
    };
    function message(value,error=false) {status.textContent=value;status.classList.toggle('error',error);}
    function save() {
        try {sessionStorage.setItem('numforge-graph-state',JSON.stringify(Object.fromEntries(fields.map(name=>{
            const el=$('graph-'+name);return [name,el.type==='checkbox'?el.checked:el.value];
        }))));} catch (_) {}
    }
    try {
        const state=JSON.parse(sessionStorage.getItem('numforge-graph-state')||'null');
        for(const name of fields) {
            const el=$('graph-'+name),value=state?.[name];
            if(el.type==='checkbox' && typeof value==='boolean')el.checked=value;
            else if(typeof value==='string' && value.length<=4096)el.value=value;
        }
    } catch (_) {}
    function invalidate() {
        ++generation;request?.abort();request=null;samples=null;
        svg.setAttribute('hidden','');svg.querySelectorAll('.graph-drawing').forEach(el=>el.remove());
        $('graph-values').hidden=true;table.replaceChildren();root.removeAttribute('aria-busy');
        message(text.ready);save();
    }
    // Projection only: mathematics and sample classification come from the C API.
    function numeric(value) {
        if(typeof value!=='string'||value.length>128||!/^[-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?$/.test(value))return null;
        const number=Number(value);
        if(!Number.isFinite(number))return null;
        // An underflowed nonzero value must not appear as a zero sample.
        if(number===0 && /[1-9]/.test(value.split(/[eE]/)[0]))return null;
        return number;
    }
    function element(tag,attributes={},content) {
        const el=document.createElementNS('http://www.w3.org/2000/svg',tag);
        for(const [key,value] of Object.entries(attributes))el.setAttribute(key,String(value));
        if(content!==undefined)el.textContent=content;return el;
    }
    function renderTable() {
        if(!samples || table.childElementCount)return;
        const fragment=document.createDocumentFragment();
        for(const point of samples) {
            const row=document.createElement('tr');
            for(const value of [point.x,point.y??'—',point.plottable?text.real:point.kind==='real'?text.outside:text[point.kind]]) {
                const cell=document.createElement('td');cell.textContent=value;row.append(cell);
            }
            if(point.status)row.title=errors[point.status]||point.status;
            fragment.append(row);
        }
        table.append(fragment);
    }
    function draw() {
        svg.querySelectorAll('.graph-drawing').forEach(el=>el.remove());
        const valid=samples.filter(p=>p.plottable);
        message(text.summary(valid.length,samples.length));
        $('graph-values').hidden=false;
        $('graph-table-caption').textContent=text.caption;
        if($('graph-values').open)renderTable();
        if(!valid.length){svg.setAttribute('hidden','');return;}
        const xs=samples.map(p=>p.nx),ys=valid.map(p=>p.ny);
        if(xs.some(x=>x===null) || xs.some((x,i)=>i>0 && x<=xs[i-1])) {
            message(text.range,true);svg.setAttribute('hidden','');return;
        }
        // Scale before subtraction so finite extreme values cannot overflow.
        const xScale=Math.max(Math.abs(xs[0]),Math.abs(xs.at(-1)),Number.MIN_VALUE);
        const yScale=Math.max(...ys.map(Math.abs),Number.MIN_VALUE);
        const xLow=xs[0]/xScale,xHigh=xs.at(-1)/xScale;
        if(xHigh<=xLow){message(text.range,true);svg.setAttribute('hidden','');return;}
        let yLow=Math.min(...ys.map(y=>y/yScale)),yHigh=Math.max(...ys.map(y=>y/yScale));
        if(yLow===yHigh){yLow-=.5;yHigh+=.5;}
        const padding=(yHigh-yLow)*.08;yLow-=padding;yHigh+=padding;
        yLow=Math.max(yLow,-Number.MAX_VALUE/yScale);
        yHigh=Math.min(yHigh,Number.MAX_VALUE/yScale);
        const width=Math.max(320,Math.min(800,root.querySelector('.graph-result').clientWidth-48));
        const height=width<600?300:440, right=width-22, bottom=height-46;
        svg.setAttribute('viewBox',`0 0 ${width} ${height}`);
        const px=x=>64+(x/xScale-xLow)/(xHigh-xLow)*(right-64);
        const py=y=>bottom-(y/yScale-yLow)/(yHigh-yLow)*(bottom-22);
        const group=element('g',{class:'graph-drawing'});
        const label=value=>value===0?'0':value.toLocaleString(english?'en':'sk',
            {maximumSignificantDigits:3,notation:Math.abs(value)>=1e5 || Math.abs(value)<.001?'scientific':'standard'});
        for(let i=0;i<=4;i++) {
            const fraction=i/4,x=64+fraction*(right-64),y=bottom-fraction*(bottom-22);
            group.append(element('line',{x1:x,x2:x,y1:22,y2:bottom,class:'graph-grid'}),
                element('line',{x1:64,x2:right,y1:y,y2:y,class:'graph-grid'}),
                element('text',{x,y:height-22,'text-anchor':'middle',class:'graph-tick'},label((xLow+(xHigh-xLow)*fraction)*xScale)),
                element('text',{x:56,y:y+4,'text-anchor':'end',class:'graph-tick'},label((yLow+(yHigh-yLow)*fraction)*yScale)));
        }
        if(xLow<=0 && xHigh>=0)group.append(element('line',{x1:px(0),x2:px(0),y1:22,y2:bottom,class:'graph-axis'}));
        if(yLow<=0 && yHigh>=0)group.append(element('line',{x1:64,x2:right,y1:py(0),y2:py(0),class:'graph-axis'}));
        group.append(element('text',{x:right,y:height-4,class:'graph-tick'},$('graph-parameter').value),element('text',{x:14,y:16,class:'graph-tick'},'y'));
        let previous=null;
        for(const point of samples) {
            if(!point.plottable){previous=null;continue;}
            const x=px(point.nx),y=py(point.ny);
            if($('graph-connect').checked && previous)group.append(element('line',{x1:previous.x,y1:previous.y,x2:x,y2:y,class:'graph-line'}));
            const dot=element('circle',{cx:x,cy:y,r:2.1,class:'graph-point'});
            dot.append(element('title',{},`${$('graph-parameter').value} = ${point.x}; y = ${point.y}`));group.append(dot);previous={x,y};
        }
        $('graph-svg-description').textContent=$('graph-expression').value+'; '+text.summary(valid.length,samples.length);
        svg.append(group);svg.removeAttribute('hidden');
    }
    async function plot(event) {
        event.preventDefault();invalidate();const id=generation;
        if(!$('graph-form').reportValidity())return;
        request=new AbortController();const signal=request.signal;
        root.setAttribute('aria-busy','true');message(text.busy);
        try {
            const query=new URLSearchParams({min:$('graph-min').value.trim(),max:$('graph-max').value.trim(),
                points:$('graph-count').value,parameter:$('graph-parameter').value,
                precision:$('graph-precision').value,angle:$('graph-angle').value});
            if($('graph-session').checked) {
                let client='';try{client=sessionStorage.getItem('numforge-client-id')||'';}catch(_){}
                if(!/^[0-9a-f]{32}$/.test(client))throw Error(text.session);
                const stateResponse=await fetch('/api/session?client='+client,{signal,cache:'no-store'});
                const state=await stateResponse.json();
                if(!stateResponse.ok||!state.ok)throw Error(text.session);
                query.set('client',client);query.set('at',state.revision);
            }
            const response=await fetch('/api/graph/sample?'+query,{method:'POST',headers:{'Content-Type':'text/plain; charset=utf-8'},body:$('graph-expression').value,signal});
            const body=await response.json();if(id!==generation||disposed)return;
            if(!response.ok||!body.ok)throw Error((errors[body.status]||body.error||body.status||text.invalid)+(Number.isSafeInteger(body.column)?` · ${text.column} ${body.column}`:''));
            if(body.schema_version!==1 || !Array.isArray(body.points) || body.count!==body.points.length || body.count<2 || body.count>1024)throw Error(text.invalid);
            samples=body.points.map(point=>{
                if(!point || !['real','domain_error','complex','quantity'].includes(point.kind) || typeof point.x!=='string' || (point.kind==='real'?typeof point.y!=='string':point.y!==null))throw Error(text.invalid);
                const nx=numeric(point.x),ny=numeric(point.y);
                return {...point,nx,ny,plottable:point.kind==='real'&&nx!==null&&ny!==null};
            });draw();
        } catch(error) {
            if(id===generation && !disposed && error.name!=='AbortError')message(error instanceof TypeError?text.network:error.message,true);
        } finally {if(id===generation && !disposed){root.removeAttribute('aria-busy');request=null;}}
    }
    $('graph-form').addEventListener('submit',plot,{signal:lifecycle.signal});
    for(const name of fields.filter(name=>name!=='connect'))$('graph-'+name).addEventListener('input',invalidate,{signal:lifecycle.signal});
    $('graph-connect').addEventListener('change',()=>{save();if(samples)draw();},{signal:lifecycle.signal});
    $('graph-values').addEventListener('toggle',()=>{if($('graph-values').open)renderTable();},{signal:lifecycle.signal});
    window.addEventListener('numforge:navigate',save,{signal:lifecycle.signal});
    let previousWidth=0;
    const resize=new ResizeObserver(entries=>{
        const width=entries[0].contentRect.width;
        if(width!==previousWidth){previousWidth=width;if(samples&&!disposed)draw();}
    });
    resize.observe(root.querySelector('.graph-result'));
    return ()=>{save();disposed=true;request?.abort();resize.disconnect();lifecycle.abort();};
}
window.numforgeInitGraph=initGraph;
window.numforgeDisposeGraph=initGraph();
