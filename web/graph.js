function initGraph() {
    const root = document.querySelector('.graph-workspace');
    if (!root) return null;
    const $ = id => root.querySelector('#' + id);
    const english = document.documentElement.lang === 'en';
    const lifecycle = new AbortController();
    const fields = ['expression','min','max','precision','angle','session'];
    const status = $('graph-status'), svg = $('graph-plot'), table = $('graph-table-body');
    let generation = 0, request = null, samples = null, disposed = false, parameter = 'x', view = null, limited = false;
    const text = english ? {
        ready:'Enter a function and plot it.', busy:'Sampling…', network:'Cannot reach the server. Try again.',
        invalid:'The server returned invalid graph data.', session:'Start a calculator session before using its variables.',
        range:'These coordinates cannot be distinguished in the browser’s numeric range. Choose a smaller range.',
        real:'Real', domain_error:'Domain error', complex:'Complex', quantity:'Quantity', outside:'Outside plotting range',
        summary:(shown,total)=>`y = f(${parameter}) · ${shown}/${total} real samples`, column:'column',
        limited:'Some intervals could not be resolved. Try a smaller x range.', ambiguous:'Use one unknown variable, or save the other variables in the calculator and enable its session.',
        caption:'Rounded plotting values. Complex values and quantities are not projected onto the real axis.'
    } : {
        ready:'Zadaj funkciu a vykresli ju.', busy:'Počítam body…', network:'Server nie je dostupný. Skús to znova.',
        invalid:'Server vrátil neplatné údaje grafu.', session:'Pred použitím premenných vytvor sedenie v kalkulačke.',
        range:'Tieto súradnice prehliadač nedokáže rozlíšiť vo svojom číselnom rozsahu. Zvoľ menší rozsah.',
        real:'Reálne', domain_error:'Chyba definičného oboru', complex:'Komplexné', quantity:'Veličina', outside:'Mimo rozsahu grafu',
        summary:(shown,total)=>`y = f(${parameter}) · reálne body: ${shown}/${total}`, column:'pozícia',
        limited:'Niektoré úseky sa nepodarilo rozlíšiť. Skús menší rozsah x.', ambiguous:'Použi jednu neznámu, alebo ulož ostatné premenné v kalkulačke a zapni použitie sedenia.',
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
        view=null;limited=false;parameter='x';draw();
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
        const points=samples||[],valid=points.filter(p=>p.plottable);
        if(samples)message(text.summary(valid.length,points.length)+(limited?' · '+text.limited:''));
        $('graph-values').hidden=!samples;
        $('graph-table-caption').textContent=text.caption;
        if($('graph-values').open)renderTable();
        let xs=valid.length?points.map(p=>p.nx):[-10,10];
        const ys=valid.map(p=>p.ny);
        if(xs.some(x=>x===null) || xs.some((x,i)=>i>0 && x<=xs[i-1])) {
            samples=null;draw();message(text.range,true);return;
        }
        // Scale before subtraction so finite extreme values cannot overflow.
        const xScale=Math.max(Math.abs(xs[0]),Math.abs(xs.at(-1)),Number.MIN_VALUE);
        const projectedView=!valid.length?[-10,10]:view?.every(y=>y!==null) ? view : [Math.min(...ys),Math.max(...ys)];
        const rawScale=Math.max(...projectedView.map(Math.abs));
        const yScale=rawScale===0?1:Math.max(rawScale,Number.MIN_VALUE);
        const xLow=xs[0]/xScale,xHigh=xs.at(-1)/xScale;
        if(xHigh<=xLow){samples=null;draw();message(text.range,true);return;}
        let yLow=projectedView[0]/yScale,yHigh=projectedView[1]/yScale;
        if(yLow===yHigh){yLow-=.5;yHigh+=.5;}
        const padding=valid.length?(yHigh-yLow)*.08:0;yLow-=padding;yHigh+=padding;
        yLow=Math.max(yLow,-Number.MAX_VALUE/yScale);
        yHigh=Math.min(yHigh,Number.MAX_VALUE/yScale);
        const width=Math.max(320,Math.min(800,root.querySelector('.graph-result').clientWidth-48));
        const height=width<600?300:440, right=width-22, bottom=height-46;
        svg.setAttribute('viewBox',`0 0 ${width} ${height}`);
        const px=x=>64+(x/xScale-xLow)/(xHigh-xLow)*(right-64);
        const py=y=>Math.max(-1e6,Math.min(1e6,bottom-(y/yScale-yLow)/(yHigh-yLow)*(bottom-22)));
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
        group.append(element('text',{x:right,y:height-4,class:'graph-tick'},parameter),element('text',{x:14,y:16,class:'graph-tick'},'y'));
        const defs=element('defs'),clip=element('clipPath',{id:'graph-clip'});
        clip.append(element('rect',{x:64,y:22,width:right-64,height:bottom-22}));defs.append(clip);group.append(defs);
        const curve=element('path',{class:'graph-line','clip-path':'url(#graph-clip)'});
        let path='',previous=null;
        for(const point of points) {
            if(!point.plottable){previous=null;continue;}
            const x=px(point.nx),y=py(point.ny);
            if(point.connect && previous)path+=`M${previous.x},${previous.y}L${x},${y}`;
            previous={x,y};
        }
        if(path){curve.setAttribute('d',path);group.append(curve);}
        $('graph-svg-description').textContent=samples?$('graph-expression').value+'; '+text.summary(valid.length,points.length):text.ready;
        svg.append(group);svg.removeAttribute('hidden');
    }
    async function plot(event) {
        event.preventDefault();invalidate();const id=generation;
        if(!$('graph-expression').value.trim())return;
        if(!$('graph-form').reportValidity())return;
        request=new AbortController();const signal=request.signal;
        root.setAttribute('aria-busy','true');message(text.busy);
        try {
            const query=new URLSearchParams({min:$('graph-min').value.trim(),max:$('graph-max').value.trim(),
                precision:$('graph-precision').value,angle:$('graph-angle').value});
            if($('graph-session').checked) {
                let client='';try{client=sessionStorage.getItem('numforge-client-id')||'';}catch(_){}
                if(!/^[0-9a-f]{32}$/.test(client))throw Error(text.session);
                const stateResponse=await fetch('/api/session?client='+client,{signal,cache:'no-store'});
                const state=await stateResponse.json();
                if(!stateResponse.ok||!state.ok)throw Error(text.session);
                query.set('client',client);query.set('at',state.revision);
            }
            const response=await fetch('/api/graph/plot?'+query,{method:'POST',headers:{'Content-Type':'text/plain; charset=utf-8'},body:$('graph-expression').value,signal});
            const body=await response.json();if(id!==generation||disposed)return;
            if(!response.ok||!body.ok)throw Error((body.code==='graph_parameter_ambiguous'?text.ambiguous:errors[body.status]||body.error||body.status||text.invalid)+(Number.isSafeInteger(body.column)?` · ${text.column} ${body.column}`:''));
            if(body.schema_version!==2 || !Array.isArray(body.points) || body.count!==body.points.length || body.count<2 || body.count>1024)throw Error(text.invalid);
            if(typeof body.parameter!=='string'||!/^[A-Za-z]{1,31}$/.test(body.parameter)||typeof body.limited!=='boolean')throw Error(text.invalid);
            parameter=body.parameter;view=[numeric(body.y_min),numeric(body.y_max)];limited=body.limited;
            samples=body.points.map(point=>{
                if(!point || typeof point.connect!=='boolean' || !['real','domain_error','complex','quantity'].includes(point.kind) || typeof point.x!=='string' || (point.kind==='real'?typeof point.y!=='string':point.y!==null))throw Error(text.invalid);
                const nx=numeric(point.x),ny=numeric(point.y);
                return {...point,nx,ny,plottable:point.kind==='real'&&nx!==null&&ny!==null};
            });draw();
        } catch(error) {
            if(id===generation && !disposed && error.name!=='AbortError')message(error instanceof TypeError?text.network:error.message,true);
        } finally {if(id===generation && !disposed){root.removeAttribute('aria-busy');request=null;}}
    }
    $('graph-form').addEventListener('submit',plot,{signal:lifecycle.signal});
    for(const name of fields)$('graph-'+name).addEventListener('input',invalidate,{signal:lifecycle.signal});
    $('graph-values').addEventListener('toggle',()=>{if($('graph-values').open)renderTable();},{signal:lifecycle.signal});
    window.addEventListener('numforge:navigate',save,{signal:lifecycle.signal});
    let previousWidth=0;
    const resize=new ResizeObserver(entries=>{
        const width=entries[0].contentRect.width;
        if(width!==previousWidth){previousWidth=width;if(!disposed)draw();}
    });
    resize.observe(root.querySelector('.graph-result'));
    draw();
    return ()=>{save();disposed=true;request?.abort();resize.disconnect();lifecycle.abort();};
}
window.numforgeInitGraph=initGraph;
window.numforgeDisposeGraph=initGraph();
