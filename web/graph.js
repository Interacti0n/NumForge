function initGraph() {
    const root=document.querySelector('.graph-workspace');if(!root)return null;
    const $=id=>root.querySelector('#'+id),english=document.documentElement.lang==='en';
    const lifecycle=new AbortController(),svg=$('graph-plot'),status=$('graph-status');
    const curves=[],palette=['#dfafff','#20c997','#ffb454','#66b3ff'],maximumCurves=4;
    let disposed=false,viewport=null,yViewport=[-10,10],viewportTimer=null,drawFrame=null,nextId=1,activeCurve=null;
    const pointers=new Map();let gesture=null;
    const text=english?{
        ready:'Enter a function and plot it.',busy:'Sampling…',network:'Cannot reach the server. Try again.',invalid:'The server returned invalid graph data.',
        range:'These coordinates cannot be distinguished in the browser’s numeric range. Adjust or reset the range.',column:'column',
        limited:'Some visible intervals could not be resolved. Zoom in for more detail.',ambiguous:'Use one variable, for example x or t.',
        function:'Function',color:'Curve color',remove:'Remove function',expand:'Expand graph',collapse:'Return to page',unavailable:'Function catalogue unavailable. Try reopening this page.',noMatch:'No matching function. Try another name.'
    }:{
        ready:'Zadaj funkciu a vykresli ju.',busy:'Počítam body…',network:'Server nie je dostupný. Skús to znova.',invalid:'Server vrátil neplatné údaje grafu.',
        range:'Tieto súradnice prehliadač nedokáže rozlíšiť vo svojom číselnom rozsahu. Uprav rozsah alebo obnov pohľad.',column:'pozícia',
        limited:'Niektoré viditeľné úseky sa nepodarilo rozlíšiť. Priblíž graf pre viac detailov.',ambiguous:'Použi jednu premennú, napríklad x alebo t.',
        function:'Funkcia',color:'Farba krivky',remove:'Odstrániť funkciu',expand:'Rozšíriť graf',collapse:'Vrátiť na stránku',unavailable:'Katalóg funkcií nie je dostupný. Skús znovu otvoriť stránku.',noMatch:'Žiadna funkcia sa nenašla. Skús iný názov.'
    };
    const errors=english?{'undefined answer':'Graphs do not use calculator ans.'}:{'division by zero':'Delenie nulou','undefined variable':'Nedefinovaná premenná','undefined answer':'Graf nepoužíva ans z kalkulačky','syntax error':'Chyba syntaxe','time limit exceeded':'Prekročený časový limit','invalid argument':'Neplatný argument'};
    function message(value,error=false){status.textContent=value;status.classList.toggle('error',error);}
    function save(){try{const first=curves[0];sessionStorage.setItem('numforge-graph-state',JSON.stringify({expression:first?.input.value||'',color:first?.color.value||palette[0],curves:curves.map(c=>({expression:c.input.value,color:c.color.value}))}));}catch(_) {}}
    function drawSoon(){if(drawFrame===null)drawFrame=requestAnimationFrame(()=>{drawFrame=null;if(!disposed)draw();});}
    function dimensions(){
        const result=root.querySelector('.graph-result');
        if(!$('graph-expanded').open){const width=Math.max(320,Math.min(800,result.clientWidth-48));return {width,height:width<600?300:440};}
        const rendered=svg.clientWidth||result.clientWidth-48,width=Math.max(320,Math.min(2048,rendered));
        const available=$('graph-expanded').getBoundingClientRect().bottom-svg.getBoundingClientRect().top-38;
        return {width,height:Math.round(Math.max(160,Math.min(1200,available*width/rendered)))};
    }
    function syncStatus(){
        const states=curves.map(c=>c.state());
        root.toggleAttribute('aria-busy',states.some(s=>s.busy));if(root.hasAttribute('aria-busy'))root.setAttribute('aria-busy','true');
        curves.forEach((c,i)=>{const s=states[i],node=c.row.querySelector('.graph-curve-status');node.hidden=curves.length===1||(!s.error&&!s.busy&&!s.message.includes(text.limited));node.textContent=s.message;node.classList.toggle('error',s.error);});
        if(curves.length===1){message(states[0].message,states[0].error);return;}
        const drawn=states.filter(s=>s.samples?.some(p=>p.plottable)).length,failed=states.filter(s=>s.error).length,unresolved=states.filter(s=>s.message.includes(text.limited)).length;
        message(states.some(s=>s.busy)?text.busy:drawn?(english?`${drawn} functions plotted`:`Zobrazené funkcie: ${drawn}`)+(failed?(english?` · ${failed} failed`:` · s chybou: ${failed}`):'')+(unresolved?(english?` · ${unresolved} with unresolved detail`:` · s nevyriešeným detailom: ${unresolved}`):''):states.find(s=>s.error)?.message||text.ready,!drawn&&failed>0);
    }
    // Serialize C requests; user-visible work takes priority over speculative tiles.
    const queue=[];let transmitting=false,foregroundBatches=0;
    function transport(query,expression,signal,background){return new Promise((resolve,reject)=>{
        const job={query,expression,signal,background,resolve,reject};
        job.cancel=()=>{const i=queue.indexOf(job);if(i>=0)queue.splice(i,1);reject(new DOMException('Aborted','AbortError'));};
        if(signal.aborted){job.cancel();return;}signal.addEventListener('abort',job.cancel,{once:true});queue.push(job);pump();
    });}
    async function pump(){
        if(transmitting||disposed||!queue.length)return;
        let i=queue.findIndex(job=>!job.background);if(i<0){if(foregroundBatches)return;i=0;}
        const job=queue.splice(i,1)[0];job.signal.removeEventListener('abort',job.cancel);
        if(job.signal.aborted){job.cancel();pump();return;}transmitting=true;
        try{
            const start=performance.now(),response=await fetch('/api/graph/plot?'+job.query,{method:'POST',headers:{'Content-Type':'text/plain; charset=utf-8'},body:job.expression,signal:job.signal});
            const payload=await response.text();job.resolve({response,payload,cost:performance.now()-start});
        }catch(error){job.reject(error);}finally{transmitting=false;pump();}
    }
    async function plotAll(event,keepView=false){
        event?.preventDefault();clearTimeout(viewportTimer);viewportTimer=null;
        if(!$('graph-form').reportValidity())return;
        ++foregroundBatches;
        try{await Promise.all(curves.map(c=>c.plot(null,keepView)));}finally{--foregroundBatches;pump();}
    }
    function scheduleView(){clearTimeout(viewportTimer);viewportTimer=setTimeout(()=>{viewportTimer=null;if(!disposed)plotAll(null,true);},90);}
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
    // Monotone cubic controls from C slopes. Limiting tangents keeps the curve
    // inside endpoint ordinates, without inventing overshoot or joining gaps.
    function cubic(a,b) {
        const dx=b.nx-a.nx,dy=b.ny-a.ny,secant=dy/dx;
        if(!Number.isFinite(a.derivative)||!Number.isFinite(b.derivative)||!Number.isFinite(secant)||!Number.isFinite(dy)||dx<=0||dy===0)return null;
        let alpha=Math.max(0,a.derivative/secant),beta=Math.max(0,b.derivative/secant);
        if(!Number.isFinite(alpha)||!Number.isFinite(beta))return null;
        const norm=Math.hypot(alpha,beta);if(norm>3){alpha*=3/norm;beta*=3/norm;}
        return [a.nx+dx/3,a.ny+dy*alpha/3,b.nx-dx/3,b.ny-dy*beta/3];
    }
    function draw() {
        svg.querySelectorAll('.graph-drawing').forEach(el=>el.remove());
        const states=curves.map(curve=>({curve,...curve.state()}));
        for(const state of states)if(state.samples&&!state.pendingView)state.curve.message((state.approximate?'≈ ':'')+`y = f(${state.parameter})`+(state.curve.visibleUnresolved()?' · '+text.limited:''));
        let xs=viewport||bounds();
        for(const state of states)if(state.samples&&(state.chunks||[{points:state.samples}]).some(c=>c.points.some((p,i)=>p.nx===null||(i>0&&p.nx<=c.points[i-1].nx)))){state.curve.invalidData();draw();return;}
        // Scale before subtraction so finite extreme values cannot overflow.
        const xScale=Math.max(Math.abs(xs[0]),Math.abs(xs.at(-1)),Number.MIN_VALUE);
        const projectedView=yViewport;
        const rawScale=Math.max(...projectedView.map(Math.abs));
        const yScale=rawScale===0?1:Math.max(rawScale,Number.MIN_VALUE);
        const xLow=xs[0]/xScale,xHigh=xs.at(-1)/xScale;
        if(xHigh<=xLow){message(text.range,true);return;}
        let yLow=projectedView[0]/yScale,yHigh=projectedView[1]/yScale;
        const {width,height}=dimensions(),right=width-22,bottom=height-46;
        svg.setAttribute('viewBox',`0 0 ${width} ${height}`);
        const px=x=>Math.max(-1e6,Math.min(1e6,64+(x/xScale-xLow)/(xHigh-xLow)*(right-64)));
        const py=y=>Math.max(-1e6,Math.min(1e6,bottom-(y/yScale-yLow)/(yHigh-yLow)*(bottom-22)));
        const group=element('g',{class:'graph-drawing'});
        const ticks=(low,high,pixels,spacing)=>{
            const scale=Math.max(Math.abs(low),Math.abs(high),Number.MIN_VALUE);
            const raw=Math.max(Number.MIN_VALUE,(high/scale-low/scale)/Math.max(2,Math.floor(pixels/spacing))*scale);
            const power=Math.max(Number.MIN_VALUE,10**Math.floor(Math.log10(raw))),ratio=raw/power;
            const step=(ratio<=1?1:ratio<=2?2:ratio<=5?5:10)*power;
            const digits=Math.max(0,Math.min(14,-Math.floor(Math.log10(step))));
            const first=Math.ceil(low/step-1e-10),last=Math.floor(high/step+1e-10),values=[];
            for(let k=first;k<=last&&values.length<32;++k){
                const value=k===0?0:Number((k*step).toPrecision(15));
                const scientific=Math.abs(value)>=1e9||(value!==0&&Math.abs(value)<1e-6);
                const label=value===0?'0':value.toLocaleString(english?'en':'sk',scientific?
                    {useGrouping:false,notation:'scientific',maximumSignificantDigits:Math.min(15,Math.max(3,Math.ceil(Math.log10(Math.max(Math.abs(low),Math.abs(high))/step))+2))}:
                    {useGrouping:false,maximumFractionDigits:digits});
                values.push({value,label});
            }return values;
        };
        const xTicks=ticks(xs[0],xs.at(-1),right-64,Math.max(64,String(xs[0]).length*7));
        const yTicks=ticks(projectedView[0],projectedView[1],bottom-22,55);
        for(const {value,label} of xTicks){const x=px(value);
            group.append(element('line',{x1:x,x2:x,y1:22,y2:bottom,class:'graph-grid'}),
                element('text',{x,y:height-22,'text-anchor':x<84?'start':x>right-20?'end':'middle',class:'graph-tick','data-axis':'x','data-value':value},label));
        }
        for(const {value,label} of yTicks){const y=py(value);
            group.append(element('line',{x1:64,x2:right,y1:y,y2:y,class:'graph-grid'}),
                element('text',{x:56,y:y+4,'text-anchor':'end',class:'graph-tick','data-axis':'y','data-value':value,...(label.length>7?{textLength:52,lengthAdjust:'spacingAndGlyphs'}:{})},label));
        }
        if(xs[0]<=0&&xs.at(-1)>=0)group.append(element('line',{x1:px(0),x2:px(0),y1:22,y2:bottom,class:'graph-axis','data-axis':'y'}));
        if(projectedView[0]<=0&&projectedView[1]>=0)group.append(element('line',{x1:64,x2:right,y1:py(0),y2:py(0),class:'graph-axis','data-axis':'x'}));
        group.append(element('text',{x:right,y:height-4,class:'graph-tick'},states[0]?.parameter||'x'),element('text',{x:14,y:16,class:'graph-tick'},'y'));
        svg.dataset.xmin=String(xs[0]);svg.dataset.xmax=String(xs.at(-1));
        svg.dataset.ymin=String(projectedView[0]);svg.dataset.ymax=String(projectedView[1]);
        const defs=element('defs'),clip=element('clipPath',{id:'graph-clip'});
        clip.append(element('rect',{x:64,y:22,width:right-64,height:bottom-22}));defs.append(clip);group.append(defs);
        for(const state of states){
        // Joined tiles share an actual C sample and one stroke. Clipping each
        // tile separately can cut a steep stroke or expose different interpolants.
        const runs=[];
        for(const chunk of state.chunks||[{points:state.samples||[]}]){
            if(chunk.joinedLeft&&runs.length){const run=runs.at(-1);run.chunks.push(chunk);run.high=chunk.high;}
            else runs.push({low:chunk.low,high:chunk.high,chunks:[chunk]});
        }
        for(const [index,run] of runs.entries()) {
            const id='graph-clip-'+state.curve.id+'-'+index,segment=element('clipPath',{id});
            const left=run.low===undefined?64:Math.max(64,px(run.low)),end=run.high===undefined?right:Math.min(right,px(run.high));
            segment.append(element('rect',{x:left,y:22,width:Math.max(0,end-left),height:bottom-22}));defs.append(segment);
            const curve=element('path',{class:'graph-line','data-curve':state.curve.id,style:`stroke:${state.curve.color.value}`,'clip-path':`url(#${id})`});
            let path='',previous=null,pen=false;
            for(const chunk of run.chunks)for(let i=0;i<chunk.points.length;++i) {
                const point=chunk.points[i];
                // Both copies of a joined boundary are the same C sample.
                if(i===0&&chunk.joinedLeft)continue;
                if(!point.plottable){previous=null;pen=false;continue;}
                const x=px(point.nx),y=py(point.ny);
                if(point.connect && previous){
                    if(!pen)path+=`M${previous.x},${previous.y}`;
                    const control=x-previous.x>1?cubic(previous.point,point):null;
                    path+=control?`C${px(control[0])},${py(control[1])},${px(control[2])},${py(control[3])},${x},${y}`:`L${x},${y}`;pen=true;
                }else pen=false;
                previous={x,y,point};
            }
            if(path){curve.setAttribute('d',path);group.append(curve);}
        }
        }
        $('graph-svg-description').textContent=states.some(s=>s.samples)?curves.map(c=>c.input.value).filter(Boolean).join('; '):text.ready;
        svg.append(group);svg.removeAttribute('hidden');
    }
    function createCurve(row){
        const input=row.querySelector('input:not([type=color])'),color=row.querySelector('input[type=color]'),controller=new AbortController();
        const id=nextId++;row.dataset.curve=String(id);
        let generation=0,request=null,samples=null,disposed=false,parameter='x',limited=false,pendingView=false,approximate=false,busy=false;
        let cache=[],cacheIdentity=null,chunks=null,prefetchTimer=null,prefetchRequest=null,prefetchToken=0,cacheMissing=[];
        const prefetchAttempts=new Set(),maxPoints=8192,maxCachePoints=12288,maxCacheBytes=8*1024*1024;
        let lastMessage=text.ready,lastError=false;
        function message(value,error=false){if(lastMessage===value&&lastError===error)return;lastMessage=value;lastError=error;syncStatus();}
        function stopPrefetch(){++prefetchToken;clearTimeout(prefetchTimer);prefetchTimer=null;prefetchRequest?.abort();prefetchRequest=null;}
        function clearCache(){stopPrefetch();cache=[];cacheIdentity=null;prefetchAttempts.clear();}
        function invalidate(){++generation;request?.abort();request=null;busy=false;clearCache();samples=null;chunks=null;pendingView=false;limited=false;parameter='x';message(text.ready);syncStatus();drawSoon();}
        function visibleUnresolved() {
            if(!limited||!samples)return false;
            const [low,high]=bounds(),[bottom,top]=yViewport;
            return (chunks||[{points:samples,limited}]).some(chunk=>chunk.limited&&chunk.points.some((p,i)=>{
                const a=Math.max(low,chunk.low??low),b=Math.min(high,chunk.high??high);
                if(p.kind==='unresolved'&&p.nx>=a&&p.nx<=b)return true;
                const previous=chunk.points[i-1];
                return previous&&previous.plottable&&p.plottable&&!p.connect&&previous.nx<b&&p.nx>a&&
                    Math.max(previous.ny,p.ny)>=bottom&&Math.min(previous.ny,p.ny)<=top;
            }));
        }
        function decode(body) {
            const slopes=body.slopes;
            if(slopes!==undefined&&(body.schema_version!==3||!Array.isArray(slopes)||slopes.length!==body.count||slopes.some(s=>s!==null&&!Number.isFinite(s))))throw Error(text.invalid);
            if(body.schema_version===3){
                const kinds=['real','domain_error','complex','quantity','unresolved'];
                if(body.evaluation!=='double'||!Array.isArray(body.points)||body.count!==body.points.length||body.count<2||body.count>maxPoints)throw Error(text.invalid);
                body={...body,y_min:body.y_min===null?null:String(body.y_min),y_max:body.y_max===null?null:String(body.y_max),points:body.points.map(row=>{
                    if(!Array.isArray(row)||row.length!==5||!Number.isFinite(row[0])||(row[1]!==null&&!Number.isFinite(row[1]))||![0,1].includes(row[2])||!Number.isInteger(row[3])||row[3]<0||row[3]>4||![0,1].includes(row[4]))throw Error(text.invalid);
                    return {x:String(row[0]),y:row[1]===null?null:String(row[1]),connect:!!row[2],kind:kinds[row[3]],source_approximate:!!row[4]};
                })};
            }
            if(![2,3].includes(body.schema_version) || !Array.isArray(body.points) || body.count!==body.points.length || body.count<2 || body.count>maxPoints)throw Error(text.invalid);
            if(typeof body.parameter!=='string'||!/^[A-Za-z]{1,31}$/.test(body.parameter)||typeof body.limited!=='boolean')throw Error(text.invalid);
            const points=body.points.map((point,i)=>{
                if(!point || typeof point.connect!=='boolean' || !['real','domain_error','complex','quantity',...(body.schema_version===3?['unresolved']:[])].includes(point.kind) || typeof point.x!=='string' || (point.kind==='real'?typeof point.y!=='string'&&!(body.schema_version===3&&point.y===null):point.y!==null))throw Error(text.invalid);
                const nx=numeric(point.x),ny=numeric(point.y);
                return {...point,nx,ny,derivative:slopes?.[i]??NaN,plottable:point.kind==='real'&&nx!==null&&ny!==null};
            });
            return {points,parameter:body.parameter,limited:body.limited,approximate:body.evaluation==='double',quality:body.quality||'refined',width:body.width,height:body.height};
        }
        async function fetchPlot(query,expression,signal,background=false) {
            const {response,payload,cost}=await transport(query,expression,signal,background);if(payload.length>512*1024)throw Error(text.invalid);
            const body=JSON.parse(payload);
            if(!response.ok||!body.ok)throw Error((body.code==='graph_parameter_ambiguous'?text.ambiguous:errors[body.status]||body.error||body.status||text.invalid)+(Number.isSafeInteger(body.column)?` · ${text.column} ${body.column}`:''));
            // Conservative cache weight, not an engine-specific heap measurement.
            return {...decode(body),sampleWidth:Number(query.get('width'))||0,verticalSpan:Number(query.get('ymax'))-Number(query.get('ymin'))||0,cacheBytes:payload.length*2+body.points.length*256,cost,requestedLow:numeric(query.get('min')),requestedHigh:numeric(query.get('max'))};
        }
        function remember(entry) {
            let low=entry.points[0].nx,high=entry.points.at(-1).nx;
            if(low===null||high===null||high<=low||!Number.isFinite(high-low)||entry.points.some((p,i)=>p.nx===null||(i>0&&p.nx<=entry.points[i-1].nx)))return false;
            const a=entry.requestedLow,b=entry.requestedHigh;
            if(a!==null&&b!==null&&b>a&&Number.isFinite(b-a)) {
                const near=(actual,requested)=>Math.abs(actual-requested)<=Math.max((b-a)*1e-12,Math.abs(requested)*Number.EPSILON*4);
                // HTTP plotting coordinates are rounded. Use requested coverage
                // when endpoints agree to projection precision, avoiding tiny gaps.
                if(near(low,a)&&near(high,b)){low=a;high=b;}
            }
            entry.low=low;entry.high=high;entry.span=high-low;
            cache=cache.filter(e=>e.low!==low||e.high!==high);cache.push(entry);
            while(cache.length>12||cache.reduce((n,e)=>n+e.points.length,0)>maxCachePoints||cache.reduce((n,e)=>n+e.cacheBytes,0)>maxCacheBytes){
                const [a,b]=bounds();
                let victim=cache.findIndex(e=>e.high<=a||e.low>=b);
                if(victim<0)victim=cache.findIndex(e=>!resolutionReady(e,a,b));
                cache.splice(victim<0?0:victim,1);
            }
            return true;
        }
        // Trim only an already accepted edge to an actual sample at a tile boundary.
        // Never bridge a false connection, a non-real value or missing coverage.
        function seamPoints(chunk,anchor,left) {
            const p=chunk.points,i=p.findIndex(point=>point.nx>=anchor.nx);
            if(i<0)return null;
            const exact=p[i].nx===anchor.nx;
            if(left){
                if(i===0||!p[i].connect||!p[i-1].plottable||!p[i].plottable)return null;
                return [...p.slice(0,i),{...anchor,connect:true}];
            }
            const next=exact?i+1:i,previous=exact?i:i-1;
            if(previous<0||next>=p.length||!p[next].connect||!p[previous].plottable||!p[next].plottable)return null;
            return [{...anchor,connect:false},...p.slice(next)];
        }
        function showCached(low,high,paint=true) {
            cacheMissing=[[low,high]];
            const entries=cache.filter(e=>e.low<high&&e.high>low);
            if(!entries.length)return false;
            const stops=[...new Set([low,high,...entries.flatMap(e=>[Math.max(low,e.low),Math.min(high,e.high)])])].sort((a,b)=>a-b);
            const segments=[];let covered=true,ready=true;cacheMissing=[];
            const missing=(a,b)=>{const previous=cacheMissing.at(-1);if(previous&&previous[1]===a)previous[1]=b;else cacheMissing.push([a,b]);};
            for(let i=1;i<stops.length;++i) {
                const a=stops[i-1],b=stops[i],entry=entries.filter(e=>e.low<=a&&e.high>=b).sort((x,y)=>x.span-y.span)[0];
                if(!entry){covered=false;missing(a,b);continue;}
                const resolution=resolutionReady(entry,low,high);
                ready=ready&&entry.quality!=='coarse'&&resolution;
                if(entry.quality==='coarse'||!resolution)missing(a,b);
                const last=segments.at(-1);
                if(last?.entry===entry&&last.high===a)last.high=b;
                else segments.push({low:a,high:b,entry});
            }
            if(!segments.length)return false;
            chunks=segments.map(segment=>{
                const p=segment.entry.points;
                let start=p.findIndex(point=>point.nx>=segment.low);if(start<0)start=p.length-1;
                let end=p.findIndex(point=>point.nx>segment.high);if(end<0)end=p.length;
                return {low:segment.low,high:segment.high,limited:segment.entry.limited,points:p.slice(Math.max(0,start-1),Math.min(p.length,end+1))};
            });
            for(let i=1;i<chunks.length;++i){
                const left=chunks[i-1],right=chunks[i];
                if(left.high!==right.low)continue;
                const preferred=segments[i-1].entry.span<=segments[i].entry.span?[left,right]:[right,left];
                const anchor=preferred.map(c=>c.points.find(p=>p.nx===right.low&&p.plottable)).find(Boolean);
                if(!anchor)continue;
                const a=seamPoints(left,anchor,true),b=seamPoints(right,anchor,false);
                if(!a||!b)continue;
                left.points=a;right.points=b;right.joinedLeft=true;
            }
            samples=chunks.flatMap(c=>c.points);parameter=segments[0].entry.parameter;
            limited=segments.some(s=>s.entry.limited);
            approximate=segments.some(s=>s.entry.approximate);
            pendingView=!(covered&&ready);if(paint)draw();
            return covered&&ready;
        }
        function resolutionReady(entry,low,high){
            const {width,height}=dimensions(),sampleWidth=entry.width||entry.sampleWidth;
            return entry.quality!=='coarse'&&(!sampleWidth?entry.span<=(high-low)*4:entry.span/sampleWidth<=(high-low)/width*1.25)&&(!entry.verticalSpan||entry.verticalSpan/(entry.height||height)<=(yViewport[1]-yViewport[0])/height*1.25);
        }
        function missingRanges(low,high) {
            let cursor=low;const gaps=[];
            const prepared=cache.filter(e=>resolutionReady(e,low,high)&&e.high>low&&e.low<high).sort((a,b)=>a.low-b.low);
            for(const entry of prepared) {
                if(entry.low>cursor)gaps.push([cursor,Math.min(entry.low,high)]);
                cursor=Math.max(cursor,entry.high);if(cursor>=high)break;
            }
            if(cursor<high)gaps.push([cursor,high]);return gaps;
        }
        function prefetch(query,expression,identity,low,high) {
            stopPrefetch();
            const span=high-low,token=prefetchToken;
            const ranges=[[low,high],[low-span,low],[high,high+span]];
            prefetchTimer=setTimeout(async()=>{
                prefetchTimer=null;
                let fetched=0;
                for(const [index,range] of ranges.entries()) {
                const jobs=index===0?[range]:missingRanges(...range);
                for(const [a,b] of jobs) {
                    if(disposed||token!==prefetchToken||cacheIdentity!==identity||pointers.size)return;
                    if(fetched>=3)return;
                    if(!Number.isFinite(a)||!Number.isFinite(b)||b<=a||!Number.isFinite(b-a))continue;
                    const nextZoom=index===0,center=low+span/2;
                    const targetLow=nextZoom?center-span/4:a,targetHigh=nextZoom?center+span/4:b;
                    const key=JSON.stringify([identity,a,b,nextZoom]);
                    if(prefetchAttempts.has(key)||cache.some(e=>e.low<=a&&e.high>=b&&resolutionReady(e,targetLow,targetHigh)&&(!nextZoom||e.verticalSpan<=(yViewport[1]-yViewport[0])*.625)))continue;
                    prefetchAttempts.add(key);if(prefetchAttempts.size>48)prefetchAttempts.delete(prefetchAttempts.values().next().value);
                    prefetchRequest=new AbortController();const signal=prefetchRequest.signal;
                    ++fetched;
                    const next=new URLSearchParams(query);next.set('min',String(a));next.set('max',String(b));
                    if(nextZoom){
                        next.set('width',String(Math.min(2048,Number(query.get('width'))*2)));
                        const middle=yViewport[0]+(yViewport[1]-yViewport[0])/2,quarter=(yViewport[1]-yViewport[0])/4;
                        next.set('ymin',String(middle-quarter));next.set('ymax',String(middle+quarter));
                    }
                    try {
                        const entry=await fetchPlot(next,expression,signal,true);
                        if(disposed||token!==prefetchToken||cacheIdentity!==identity){prefetchAttempts.delete(key);return;}
                        // Speculative detail cannot evict the current prepared view.
                        const retained=cache.filter(e=>e.low<high&&e.high>low);
                        if(retained.reduce((n,e)=>n+e.points.length,0)+entry.points.length>maxCachePoints||retained.reduce((n,e)=>n+e.cacheBytes,0)+entry.cacheBytes>maxCacheBytes)return;
                        remember(entry);
                        if(entry.limited||entry.cost>1500||!entry.points.some(p=>p.plottable))return;
                    } catch(_){if(signal.aborted)prefetchAttempts.delete(key);return;}
                    finally {if(token===prefetchToken)prefetchRequest=null;}
                }
                }
            },180);
        }
        async function plot(event,keepView=false) {
            event?.preventDefault();
            stopPrefetch();
            ++generation;request?.abort();request=null;busy=false;syncStatus();
            if(!keepView){samples=null;chunks=null;pendingView=false;draw();}
            const id=generation,expression=input.value;
            if(!expression.trim()){pendingView=false;draw();message(text.ready);return;}
            if(!input.reportValidity())return;
            request=new AbortController();const signal=request.signal;
            busy=true;message(text.busy);syncStatus();
            try {
                const [low,high]=bounds(),[yLow,yHigh]=yViewport;
                // The C screen sampler selects density from the camera and falls
                // back to the precision evaluator when fast arithmetic is unsuitable.
                const query=new URLSearchParams({min:String(low),max:String(high),angle:'rad',ymin:String(yLow),ymax:String(yHigh)});
                const {width,height}=dimensions();
                query.set('mode','fast');query.set('width',String(Math.round(width)));query.set('height',String(height));query.set('quality','refined');query.set('interpolation','hermite');
                if(id!==generation||disposed)return;
                const identity=expression;
                if(cacheIdentity!==identity){clearCache();cacheIdentity=identity;samples=null;chunks=null;draw();message(text.busy);}
                if(low!==null&&high!==null&&high>low&&Number.isFinite(high-low))viewport=[low,high];
                if(low!==null&&high!==null&&showCached(low,high)) {
                    const fast=cache.find(e=>!e.limited&&e.cost<=1500&&e.points.some(p=>p.plottable));
                    if(fast)prefetch(query,expression,identity,low,high);return;
                }
                let jobs=low!==null&&high!==null&&cacheMissing.length&&cacheMissing.length<=4?cacheMissing.map(r=>r.slice()):[[low,high]];
                // A dense visible tile plus another full-size tile can exceed the
                // cache point bound. Replace the viewport as one bounded result.
                if(low!==null&&high!==null&&cache.filter(e=>e.low<high&&e.high>low).reduce((n,e)=>n+e.points.length,0)>maxCachePoints-maxPoints)jobs=[[low,high]];
                if(!keepView&&low!==null&&high!==null&&!cache.length&&Number.isFinite(low-(high-low))&&Number.isFinite(high+(high-low)))jobs=[[low-(high-low),high+(high-low)]];
                let fast=true;
                for(const [a,b] of jobs) {
                    const next=new URLSearchParams(query);
                    if(a!==null&&b!==null&&(a!==low||b!==high)){next.set('min',String(a));next.set('max',String(b));}
                    if(a!==null&&b!==null&&low!==null&&high!==null)next.set('width',String(Math.max(320,Math.min(2048,Math.round(width*(b-a)/(high-low))))));
                    let entry;
                    if(keepView){
                        const preview=new URLSearchParams(next);preview.set('quality','coarse');
                        const draft=await fetchPlot(preview,expression,signal);if(id!==generation||disposed)return;
                        if(remember(draft)&&low!==null&&high!==null)showCached(low,high);
                        await new Promise(resolve=>requestAnimationFrame(resolve));if(id!==generation||disposed)return;
                        if(draft.quality==='refined')entry=draft;
                    }
                    if(!entry)entry=await fetchPlot(next,expression,signal);if(id!==generation||disposed)return;
                    fast=fast&&!entry.limited&&entry.cost<=1500&&entry.points.some(p=>p.plottable);
                    if(remember(entry)&&low!==null&&high!==null)showCached(low,high);
                    else {parameter=entry.parameter;limited=entry.limited;approximate=entry.approximate;pendingView=false;samples=entry.points;chunks=null;draw();fast=false;}
                }
                if(low!==null&&high!==null&&fast)prefetch(query,expression,identity,low,high);
            } catch(error) {
                if(id===generation && !disposed && error.name!=='AbortError')message(error instanceof TypeError?text.network:error.message,true);
            } finally {if(id===generation && !disposed){busy=false;request=null;syncStatus();}}
        }

        const curve={id,row,input,color,plot,message,visibleUnresolved,
            state:()=>({samples,chunks,parameter,limited,pendingView,approximate,busy,message:lastMessage,error:lastError}),
            invalidData(){samples=null;chunks=null;pendingView=false;message(text.range,true);},
            cameraChanged(){++generation;request?.abort();request=null;busy=false;stopPrefetch();pendingView=!!input.value.trim();const hit=showCached(...bounds(),false);if(!hit)message(input.value.trim()?text.busy:text.ready);syncStatus();},
            dispose(){disposed=true;invalidate();controller.abort();}
        };
        input.addEventListener('input',()=>{clearTimeout(viewportTimer);viewportTimer=null;if(curves.length===1){viewport=null;yViewport=[-10,10];}invalidate();save();renderLegend();},{signal:controller.signal});
        row.addEventListener('focusin',()=>{activeCurve=curve;highlightActive();},{signal:controller.signal});
        color.addEventListener('input',()=>{save();renderLegend();drawSoon();},{signal:controller.signal});
        row.querySelector('.graph-remove').addEventListener('click',()=>removeCurve(curve),{signal:controller.signal});
        return curve;
    }
    function bounds() {return viewport||[-10,10];}
    function horizontal(clientX) {
        const rect=svg.getBoundingClientRect(),width=svg.viewBox.baseVal.width;
        return Math.max(0,Math.min(1,((clientX-rect.left)/rect.width*width-64)/(width-86)));
    }
    function vertical(clientY){
        const rect=svg.getBoundingClientRect(),height=svg.viewBox.baseVal.height;
        return Math.max(0,Math.min(1,(height-46-(clientY-rect.top)/rect.height*height)/(height-68)));
    }
    function moveView(low,high,yLow=yViewport[0],yHigh=yViewport[1]) {
        if(!Number.isFinite(low)||!Number.isFinite(high)||high<=low||!Number.isFinite(high-low))return;
        if(!Number.isFinite(yLow)||!Number.isFinite(yHigh)||yHigh<=yLow||!Number.isFinite(yHigh-yLow))return;
        // Keep ample coordinate precision for pixel positions, not just endpoints.
        if(high-low<Math.max(1,Math.abs(low),Math.abs(high))*1e-8||yHigh-yLow<Math.max(1,Math.abs(yLow),Math.abs(yHigh))*1e-8){message(text.range,true);return;}
        const old=bounds(),verticalOnly=(yLow!==yViewport[0]||yHigh!==yViewport[1])&&low===old[0]&&high===old[1]&&Math.abs((yHigh-yLow)-(yViewport[1]-yViewport[0]))<=(yHigh-yLow)*1e-12;
        yViewport=[yLow,yHigh];
        if(verticalOnly){drawSoon();return;}
        viewport=[low,high];
        curves.forEach(curve=>curve.cameraChanged());drawSoon();save();scheduleView();
    }
    function zoom(factor,anchor=.5,yAnchor=.5) {
        const [low,high]=bounds(),span=high-low,center=low+span*anchor;
        const ySpan=yViewport[1]-yViewport[0],yCenter=yViewport[0]+ySpan*yAnchor;
        moveView(center-span*factor*anchor,center+span*factor*(1-anchor),yCenter-ySpan*factor*yAnchor,yCenter+ySpan*factor*(1-yAnchor));
    }
    $('graph-reset').addEventListener('click',()=>moveView(-10,10,-10,10),{signal:lifecycle.signal});
    svg.addEventListener('wheel',event=>{
        if(!event.deltaY)return;
        event.preventDefault();zoom(event.deltaY<0?.8:1.25,horizontal(event.clientX),vertical(event.clientY));
    },{passive:false,signal:lifecycle.signal});
    function startGesture() {
        const points=[...pointers.values()];
        gesture=points.length?{bounds:bounds().slice(),yBounds:yViewport.slice(),center:points.reduce((sum,p)=>sum+p.x,0)/points.length,centerY:points.reduce((sum,p)=>sum+p.y,0)/points.length,
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
        const centerY=points.reduce((sum,p)=>sum+p.y,0)/points.length;
        const [low,high]=gesture.bounds,span=high-low;
        const rect=svg.getBoundingClientRect(),width=svg.viewBox.baseVal.width;
        const shift=(gesture.center-center)/(rect.width*(width-86)/width)*span;
        const distance=points.length>1?Math.hypot(points[1].x-points[0].x,points[1].y-points[0].y):0;
        const factor=gesture.distance>0&&distance>0?gesture.distance/distance:1;
        const anchor=horizontal(gesture.center),origin=low+span*anchor+shift;
        const ySpan=gesture.yBounds[1]-gesture.yBounds[0],height=svg.viewBox.baseVal.height;
        const yShift=(centerY-gesture.centerY)/(rect.height*(height-68)/height)*ySpan;
        const yAnchor=vertical(gesture.centerY),yOrigin=gesture.yBounds[0]+ySpan*yAnchor+yShift;
        moveView(origin-span*factor*anchor,origin+span*factor*(1-anchor),yOrigin-ySpan*factor*yAnchor,yOrigin+ySpan*factor*(1-yAnchor));
        // Hold sampling until a drag/pinch settles or the pointers are released.
        clearTimeout(viewportTimer);viewportTimer=null;
    },{signal:lifecycle.signal});
    function endGesture(event) {
        pointers.delete(event.pointerId);startGesture();
        if(!pointers.size&&curves.some(c=>c.state().pendingView))scheduleView();
    }
    for(const name of ['pointerup','pointercancel','lostpointercapture'])svg.addEventListener(name,endGesture,{signal:lifecycle.signal});
    svg.addEventListener('keydown',event=>{
        if(event.key==='+'||event.key==='='){event.preventDefault();zoom(.5);}
        else if(event.key==='-'){event.preventDefault();zoom(2);}
        else if(event.key==='Home'){event.preventDefault();moveView(-10,10,-10,10);}
        else if(event.key==='ArrowUp'||event.key==='ArrowDown'){
            event.preventDefault();const [low,high]=bounds(),shift=(yViewport[1]-yViewport[0])*(event.key==='ArrowUp'?.2:-.2);moveView(low,high,yViewport[0]+shift,yViewport[1]+shift);
        }
        else if(event.key==='ArrowLeft'||event.key==='ArrowRight'){
            event.preventDefault();const [low,high]=bounds(),shift=(high-low)*(event.key==='ArrowLeft'?-.2:.2);moveView(low+shift,high+shift);
        }
    },{signal:lifecycle.signal});

    function highlightActive(){curves.forEach(c=>c.row.classList.toggle('active',c===activeCurve));}
    function renderLegend(){
        const legend=$('graph-legend');legend.replaceChildren();legend.hidden=curves.length<2;
        curves.forEach((c,i)=>{const item=document.createElement('li'),dot=document.createElement('span'),label=document.createElement('span');dot.className='graph-legend-dot';dot.style.backgroundColor=c.color.value;dot.setAttribute('aria-hidden','true');label.textContent=`${i+1}. ${c.input.value||'…'}`;label.title=c.input.value;item.append(dot,label);legend.append(item);});
    }
    function relabel(){
        curves.forEach((c,i)=>{c.row.querySelector('.graph-expression span').textContent=`${text.function} ${i+1} · y =`;c.input.id=i?'graph-expression-'+c.id:'graph-expression';c.color.id=i?'graph-color-'+c.id:'graph-color';c.row.querySelector('.graph-color-option span').textContent=`${text.color} ${i+1}`;const remove=c.row.querySelector('.graph-remove');remove.hidden=curves.length===1;remove.setAttribute('aria-label',`${text.remove} ${i+1}`);});
        $('graph-add').textContent=(english?'+ Add function':'+ Pridať funkciu')+` · ${curves.length}/${maximumCurves}`;$('graph-add').disabled=curves.length>=maximumCurves;$('graph-add').title=english?'Up to four functions':'Najviac štyri funkcie';
        highlightActive();renderLegend();syncStatus();
    }
    function addCurve(expression='',color=null,focus=true){
        if(curves.length>=maximumCurves)return;
        const row=curves[0].row.cloneNode(true);row.querySelector('.graph-curve-status').hidden=true;
        row.querySelector('input:not([type=color])').value=expression;
        row.querySelector('input[type=color]').value=color||palette.find(p=>!curves.some(c=>c.color.value===p))||palette[curves.length];
        $('graph-curves').append(row);const curve=createCurve(row);curves.push(curve);activeCurve=curve;relabel();save();if(focus)curve.input.focus();drawSoon();
    }
    function removeCurve(curve){
        if(curves.length===1)return;const index=curves.indexOf(curve);if(index<0)return;
        curve.dispose();curves.splice(index,1);curve.row.remove();activeCurve=curves[Math.min(index,curves.length-1)];relabel();save();drawSoon();activeCurve.input.focus();
    }
    const initialRow=root.querySelector('.graph-curve');let restored;
    try{restored=JSON.parse(sessionStorage.getItem('numforge-graph-state')||'null');}catch(_){}
    const valid=entry=>entry&&typeof entry.expression==='string'&&entry.expression.length<=4096;
    const restoredCurves=Array.isArray(restored?.curves)?restored.curves.filter(valid).slice(0,maximumCurves):valid(restored)?[restored]:[];
    if(restoredCurves.length){initialRow.querySelector('input:not([type=color])').value=restoredCurves[0].expression;if(/^#[0-9a-f]{6}$/i.test(restoredCurves[0].color))initialRow.querySelector('input[type=color]').value=restoredCurves[0].color;}
    curves.push(createCurve(initialRow));activeCurve=curves[0];
    for(const entry of restoredCurves.slice(1))addCurve(entry.expression,/^#[0-9a-f]{6}$/i.test(entry.color)?entry.color:null,false);
    activeCurve=curves[0];relabel();
    $('graph-form').addEventListener('submit',event=>plotAll(event),{signal:lifecycle.signal});
    $('graph-add').addEventListener('click',()=>addCurve(),{signal:lifecycle.signal});
    window.addEventListener('numforge:navigate',save,{signal:lifecycle.signal});

    // Move the same canvas into a native modal; keep its camera and curve state.
    const result=root.querySelector('.graph-result'),home=document.createElement('div'),dialog=$('graph-expanded'),expand=$('graph-expand');home.hidden=true;result.before(home);let pagePosition=null;
    function leaveExpanded(focus=true){
        if(dialog.open)dialog.close();home.after(result);home.hidden=true;document.body.classList.remove('graph-expanded-open');expand.textContent=text.expand;expand.setAttribute('aria-expanded','false');
        if(!disposed){if(pagePosition)window.scrollTo({...pagePosition,behavior:'instant'});drawSoon();if(focus)expand.focus({preventScroll:true});}
    }
    expand.addEventListener('click',()=>{
        if(dialog.open){leaveExpanded();return;}
        pagePosition={left:window.scrollX,top:window.scrollY};home.style.height=result.getBoundingClientRect().height+'px';home.hidden=false;dialog.append(result);document.body.classList.add('graph-expanded-open');expand.textContent=text.collapse;expand.setAttribute('aria-expanded','true');dialog.showModal();drawSoon();
    },{signal:lifecycle.signal});
    dialog.addEventListener('close',()=>leaveExpanded(),{signal:lifecycle.signal});
    let previousSize='';
    function resized(){
        if(disposed)return;const d=dimensions(),size=`${d.width}/${d.height}`;
        if(size===previousSize)return;
        const initial=!previousSize;previousSize=size;drawSoon();
        if(!initial&&curves.some(c=>c.state().samples||c.state().busy)){curves.forEach(c=>c.cameraChanged());scheduleView();}
    }
    const resize=new ResizeObserver(resized);resize.observe(result);
    window.addEventListener('resize',resized,{signal:lifecycle.signal});

    function insertLibrary(value,isFunction=false,name=''){
        const curve=activeCurve||curves[0],input=curve.input,start=input.selectionStart??input.value.length,end=input.selectionEnd??start;
        const selected=input.value.slice(start,end),arity=registry.get(name)?.minimum_arguments||1;
        const argument=selected||curve.state().parameter||'x';
        const insertion=isFunction?value+argument+(arity>1?';'+(name==='complex'?'0':name==='min'||name==='max'?'0':'2'):'')+')':value;
        if(input.value.length-(end-start)+insertion.length>4096){curve.message(english?'The expression is too long.':'Zadanie je príliš dlhé.',true);return;}
        input.setRangeText(insertion,start,end,'end');input.dispatchEvent(new Event('input',{bubbles:true}));input.focus();
        if(isFunction&&!selected)input.setSelectionRange(start+value.length,start+value.length+argument.length);
    }
    let registry=new Map();
    async function loadLibrary(){
        try{
            const response=await fetch('/api/functions',{signal:lifecycle.signal}),body=await response.json();
            if(!response.ok||!body.ok||!Array.isArray(body.functions))throw Error();
            if(disposed)return;
            registry=new Map(body.functions.map(entry=>[entry.name,entry]));
            const library=window.numforgeFunctionLibrary,{functionHelp,functionSearchAliases,inverseFunctionAliases}=library;
            const categories=[
                [english?'Basic':'Základné',['abs','sign','min','max','floor','ceil','trunc','round']],
                [english?'Powers & logs':'Mocniny a logaritmy',['pow','sqrt','cbrt','root','exp','ln','log']],
                [english?'Trigonometry':'Goniometria',['sin','cos','tan','asin','acos','atan','radians','degrees']],
                [english?'Hyperbolic':'Hyperbolické',['sinh','cosh','tanh','asinh','acosh','atanh']],
                [english?'Statistics':'Štatistika',['sum','product','mean','median','geomean','harmean','variance','stdevp','stdev']],
                [english?'Integers':'Celé čísla',['gcd','lcm','mod','npr','ncr','factorial','isqrt']],
                [english?'Complex components':'Komplexné zložky',['complex','re','im','conj','arg']],
                [english?'Constants':'Konštanty',[]]
            ];
            const buttons=[],container=$('graph-functions'),selector=$('graph-function-category'),search=$('function-search'),searchStatus=$('search-status'),help=$('graph-function-help');
            const makeButton=(label,insertion,description,terms,category,name='')=>{
                const button=document.createElement('button');button.type='button';button.textContent=label;button.dataset.insert=insertion;button.dataset.function=name;button.title=description;button.dataset.category=String(category);
                const searchable=library.normalize([label,insertion,description,...terms].join(' '));
                button.addEventListener('click',()=>{help.textContent=description;insertLibrary(insertion,!!name,name);},{signal:lifecycle.signal});
                button.addEventListener('focus',()=>{help.textContent=description;},{signal:lifecycle.signal});
                container.append(button);buttons.push({button,searchable,category});
            };
            categories.forEach(([label,names],i)=>{
                const option=document.createElement('option');option.value=String(i);option.textContent=label;selector.append(option);
                for(const name of names){const entry=registry.get(name),info=functionHelp[name];if(!entry?.implemented||!info)continue;
                    const aliases=body.functions.filter(e=>e.canonical===name).map(e=>e.name);
                    const description=info[1]+' · '+info[english?3:2].replaceAll('RAD/DEG',english?'radians':'radiány');
                    makeButton(name,name+'(',description,[...aliases,...(functionSearchAliases[name]||[]),...(inverseFunctionAliases[name]||[]),info[0]],i,name);
                }
            });
            const constants=library.constantAliasesFor(english);
            for(const [symbol,en,sk] of [['π','Pi','Pí'],['e','Euler’s number','Eulerovo číslo'],['φ','Golden ratio','Zlatý rez']])makeButton(symbol,symbol,english?en:sk,constants[symbol],categories.length-1);
            function filter(){const query=library.normalize(search.value.trim()),category=selector.value;let matches=0;
                buttons.forEach(({button,searchable,category:group})=>{button.hidden=!!(!query&&category&&String(group)!==category)||!searchable.includes(query);if(!button.hidden)++matches;});
                selector.closest('label').hidden=!!query;searchStatus.hidden=!query;searchStatus.textContent=matches?(english?`${matches} matching functions`:`Zodpovedajúce funkcie: ${matches}`):text.noMatch;
            }
            search.addEventListener('input',filter,{signal:lifecycle.signal});selector.addEventListener('change',filter,{signal:lifecycle.signal});
            search.addEventListener('keydown',event=>{if(event.key==='Escape'){search.value='';filter();}},{signal:lifecycle.signal});filter();
        }catch(error){if(!disposed&&error.name!=='AbortError'){$('search-status').hidden=false;$('search-status').textContent=text.unavailable;}}
    }
    loadLibrary();draw();
    return ()=>{save();disposed=true;leaveExpanded(false);curves.forEach(c=>c.dispose());clearTimeout(viewportTimer);if(drawFrame!==null)cancelAnimationFrame(drawFrame);pointers.clear();resize.disconnect();lifecycle.abort();};

}
window.numforgeInitGraph=initGraph;
window.numforgeDisposeGraph=initGraph();
