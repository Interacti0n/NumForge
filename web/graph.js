function initGraph() {
    const root = document.querySelector('.graph-workspace');
    if (!root) return null;
    const $ = id => root.querySelector('#' + id);
    const english = document.documentElement.lang === 'en';
    const lifecycle = new AbortController();
    const fields = ['expression','min','max','precision','angle','session'];
    const status = $('graph-status'), svg = $('graph-plot'), table = $('graph-table-body');
    let generation = 0, request = null, samples = null, disposed = false, parameter = 'x', view = null, limited = false;
    let viewport=null, viewportTimer=null, pendingView=false;
    const pointers=new Map();let gesture=null;
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
        clearTimeout(viewportTimer);viewportTimer=null;viewport=null;pendingView=false;
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
        if(samples&&!pendingView)message(text.summary(valid.length,points.length)+(limited?' · '+text.limited:''));
        $('graph-values').hidden=!samples||pendingView;
        $('graph-table-caption').textContent=text.caption;
        if($('graph-values').open)renderTable();
        const sampleXs=valid.length?points.map(p=>p.nx):[];
        let xs=viewport|| (valid.length?sampleXs:[-10,10]);
        const ys=valid.map(p=>p.ny);
        if(sampleXs.some(x=>x===null) || sampleXs.some((x,i)=>i>0 && x<=sampleXs[i-1])) {
            samples=null;viewport=null;draw();message(text.range,true);return;
        }
        // Scale before subtraction so finite extreme values cannot overflow.
        const xScale=Math.max(Math.abs(xs[0]),Math.abs(xs.at(-1)),Number.MIN_VALUE);
        const projectedView=!valid.length?[-10,10]:view?.every(y=>y!==null) ? view : [Math.min(...ys),Math.max(...ys)];
        const rawScale=Math.max(...projectedView.map(Math.abs));
        const yScale=rawScale===0?1:Math.max(rawScale,Number.MIN_VALUE);
        const xLow=xs[0]/xScale,xHigh=xs.at(-1)/xScale;
        if(xHigh<=xLow){samples=null;viewport=null;draw();message(text.range,true);return;}
        let yLow=projectedView[0]/yScale,yHigh=projectedView[1]/yScale;
        if(yLow===yHigh){yLow-=.5;yHigh+=.5;}
        const padding=valid.length?(yHigh-yLow)*.08:0;yLow-=padding;yHigh+=padding;
        yLow=Math.max(yLow,-Number.MAX_VALUE/yScale);
        yHigh=Math.min(yHigh,Number.MAX_VALUE/yScale);
        const width=Math.max(320,Math.min(800,root.querySelector('.graph-result').clientWidth-48));
        const height=width<600?300:440, right=width-22, bottom=height-46;
        svg.setAttribute('viewBox',`0 0 ${width} ${height}`);
        const px=x=>Math.max(-1e6,Math.min(1e6,64+(x/xScale-xLow)/(xHigh-xLow)*(right-64)));
        const py=y=>Math.max(-1e6,Math.min(1e6,bottom-(y/yScale-yLow)/(yHigh-yLow)*(bottom-22)));
        const group=element('g',{class:'graph-drawing'});
        const label=value=>value===0?'0':value.toLocaleString(english?'en':'sk',
            {maximumSignificantDigits:3,notation:Math.abs(value)>=1e5 || Math.abs(value)<.001?'scientific':'standard'});
        // Snap a near-zero axis tick relative to this view, not to an absolute
        // epsilon: genuine small-scale coordinates must retain their labels.
        const tick=(low,high,fraction)=>{
            const value=low+(high-low)*fraction;
            return low<=0&&high>=0&&Math.abs(value)<=(high-low)*1e-4?0:value;
        };
        for(let i=0;i<=4;i++) {
            const fraction=i/4,tx=tick(xLow,xHigh,fraction),ty=tick(yLow,yHigh,fraction);
            const x=64+(tx-xLow)/(xHigh-xLow)*(right-64),y=bottom-(ty-yLow)/(yHigh-yLow)*(bottom-22);
            group.append(element('line',{x1:x,x2:x,y1:22,y2:bottom,class:'graph-grid'}),
                element('line',{x1:64,x2:right,y1:y,y2:y,class:'graph-grid'}),
                element('text',{x,y:height-22,'text-anchor':'middle',class:'graph-tick'},label(tx*xScale)),
                element('text',{x:56,y:y+4,'text-anchor':'end',class:'graph-tick'},label(ty*yScale)));
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
    async function plot(event,keepView=false) {
        event?.preventDefault();
        const retained=keepView?viewport:null;invalidate();viewport=retained;draw();const id=generation;
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
            parameter=body.parameter;view=[numeric(body.y_min),numeric(body.y_max)];limited=body.limited;pendingView=false;
            samples=body.points.map(point=>{
                if(!point || typeof point.connect!=='boolean' || !['real','domain_error','complex','quantity'].includes(point.kind) || typeof point.x!=='string' || (point.kind==='real'?typeof point.y!=='string':point.y!==null))throw Error(text.invalid);
                const nx=numeric(point.x),ny=numeric(point.y);
                return {...point,nx,ny,plottable:point.kind==='real'&&nx!==null&&ny!==null};
            });draw();
        } catch(error) {
            if(id===generation && !disposed && error.name!=='AbortError')message(error instanceof TypeError?text.network:error.message,true);
        } finally {if(id===generation && !disposed){root.removeAttribute('aria-busy');request=null;}}
    }
    function bounds() {
        if(viewport)return viewport;
        const low=numeric($('graph-min').value.trim()),high=numeric($('graph-max').value.trim());
        return low!==null&&high!==null&&high>low?[low,high]:[-10,10];
    }
    function horizontal(clientX) {
        const rect=svg.getBoundingClientRect(),width=svg.viewBox.baseVal.width;
        return Math.max(0,Math.min(1,((clientX-rect.left)/rect.width*width-64)/(width-86)));
    }
    function moveView(low,high) {
        if(!Number.isFinite(low)||!Number.isFinite(high)||high<=low||!Number.isFinite(high-low))return;
        ++generation;request?.abort();request=null;root.removeAttribute('aria-busy');
        viewport=[low,high];$('graph-min').value=String(low);$('graph-max').value=String(high);
        pendingView=true;draw();message($('graph-expression').value.trim()?text.busy:text.ready);save();
        clearTimeout(viewportTimer);
        viewportTimer=setTimeout(()=>{viewportTimer=null;if(!disposed)plot(null,true);},300);
    }
    function zoom(factor,anchor=.5) {
        const [low,high]=bounds(),span=high-low,center=low+span*anchor;
        moveView(center-span*factor*anchor,center+span*factor*(1-anchor));
    }
    $('graph-zoom-in').addEventListener('click',()=>zoom(.5),{signal:lifecycle.signal});
    $('graph-zoom-out').addEventListener('click',()=>zoom(2),{signal:lifecycle.signal});
    $('graph-reset').addEventListener('click',()=>moveView(-10,10),{signal:lifecycle.signal});
    svg.addEventListener('wheel',event=>{
        if(!event.deltaY)return;
        event.preventDefault();zoom(event.deltaY<0?.8:1.25,horizontal(event.clientX));
    },{passive:false,signal:lifecycle.signal});
    function startGesture() {
        const points=[...pointers.values()];
        gesture=points.length?{bounds:bounds().slice(),center:points.reduce((sum,p)=>sum+p.x,0)/points.length,
            distance:points.length>1?Math.hypot(points[1].x-points[0].x,points[1].y-points[0].y):0}:null;
    }
    svg.addEventListener('pointerdown',event=>{
        if(event.pointerType==='mouse'&&event.button!==0)return;
        pointers.set(event.pointerId,{x:event.clientX,y:event.clientY});svg.setPointerCapture(event.pointerId);startGesture();
    },{signal:lifecycle.signal});
    svg.addEventListener('pointermove',event=>{
        if(!pointers.has(event.pointerId)||!gesture)return;
        pointers.set(event.pointerId,{x:event.clientX,y:event.clientY});
        const points=[...pointers.values()],center=points.reduce((sum,p)=>sum+p.x,0)/points.length;
        const [low,high]=gesture.bounds,span=high-low;
        const rect=svg.getBoundingClientRect(),width=svg.viewBox.baseVal.width;
        const shift=(gesture.center-center)/(rect.width*(width-86)/width)*span;
        const distance=points.length>1?Math.hypot(points[1].x-points[0].x,points[1].y-points[0].y):0;
        const factor=gesture.distance>0&&distance>0?gesture.distance/distance:1;
        const anchor=horizontal(gesture.center),origin=low+span*anchor+shift;
        moveView(origin-span*factor*anchor,origin+span*factor*(1-anchor));
        // Hold sampling until a drag/pinch settles or the pointers are released.
        clearTimeout(viewportTimer);viewportTimer=null;
    },{signal:lifecycle.signal});
    function endGesture(event) {
        pointers.delete(event.pointerId);startGesture();
        if(!pointers.size&&pendingView){clearTimeout(viewportTimer);viewportTimer=setTimeout(()=>{viewportTimer=null;if(!disposed)plot(null,true);},300);}
    }
    for(const name of ['pointerup','pointercancel','lostpointercapture'])svg.addEventListener(name,endGesture,{signal:lifecycle.signal});
    svg.addEventListener('keydown',event=>{
        if(event.key==='+'||event.key==='='){event.preventDefault();zoom(.5);}
        else if(event.key==='-'){event.preventDefault();zoom(2);}
        else if(event.key==='Home'){event.preventDefault();moveView(-10,10);}
        else if(event.key==='ArrowLeft'||event.key==='ArrowRight'){
            event.preventDefault();const [low,high]=bounds(),shift=(high-low)*(event.key==='ArrowLeft'?-.2:.2);moveView(low+shift,high+shift);
        }
    },{signal:lifecycle.signal});
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
    return ()=>{save();disposed=true;clearTimeout(viewportTimer);request?.abort();pointers.clear();resize.disconnect();lifecycle.abort();};
}
window.numforgeInitGraph=initGraph;
window.numforgeDisposeGraph=initGraph();
