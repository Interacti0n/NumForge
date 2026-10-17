function initGraph() {
    const root=document.querySelector('.graph-workspace');if(!root)return null;
    // One bounded, DOM-free snapshot for this tab's SPA navigation. Keep the
    // existing cache arrays by reference rather than serializing sample data.
    const retained=initGraph.retained;initGraph.retained=null;
    const verificationCache=retained?.verifications||new Map();
    let verificationBytes=[...verificationCache.keys()].reduce((sum,key)=>sum+key.length*2+128,0);
    function rememberVerification(key,value){if(verificationCache.has(key))return;verificationCache.set(key,value);verificationBytes+=key.length*2+128;while(verificationCache.size>256||verificationBytes>512*1024){const oldest=verificationCache.keys().next().value;verificationBytes-=oldest.length*2+128;verificationCache.delete(oldest);}}
    const $=id=>root.querySelector('#'+id),english=document.documentElement.lang==='en';
    const lifecycle=new AbortController(),svg=$('graph-plot'),status=$('graph-status');
    const curves=[],palette=['#dfafff','#20c997','#ffb454','#66b3ff'],maximumCurves=4;
    let disposed=false,restoring=true,viewport=null,yViewport=[-10,10],viewportTimer=null,drawFrame=null,nextId=1,activeCurve=null;
    const pointers=new Map();let gesture=null;
    let projection=null,inspection=null,inspectionFrame=null,inspectPointer=null,features=[],displayedFeatures=new Set(),featureCacheKey=null,locateRequest=null,verificationRequest=null,verificationTimer=null,verificationState='ready';
    lifecycle.signal.addEventListener('abort',()=>{locateRequest?.abort();verificationRequest?.abort();clearTimeout(verificationTimer);},{once:true});
    const readout=$('graph-inspector'),inspectionLayer=element('g',{class:'graph-inspection',visibility:'hidden','pointer-events':'none','aria-hidden':'true'});
    const inspectX=element('line',{class:'graph-inspection-guide'}),inspectY=element('line',{class:'graph-inspection-guide'}),inspectDot=element('circle',{r:4.5,class:'graph-inspection-dot'});
    inspectionLayer.append(inspectX,inspectY,inspectDot);svg.append(inspectionLayer);
    const text=english?{
        ready:'Enter a function and plot it.',busy:'Sampling…',network:'Cannot reach the server. Try again.',invalid:'The server returned invalid graph data.',
        range:'These coordinates cannot be distinguished in the browser’s numeric range. Adjust or reset the range.',column:'column',
        limited:'Some visible intervals could not be resolved. Zoom in for more detail.',ambiguous:'Use one variable, for example x or t.',
        dense:'Dense oscillations shown as an approximate range. Zoom in to see the curve.',noStroke:'No connected curve could be resolved here. Zoom in for more detail.',noReal:'No real-valued points found in this x range. Try a different range.',outside:'Curve is outside this view. Drag or zoom out to find it.',domain:'Parts outside the real domain are left empty.',numeric:'Values exceed the browser’s plotting range. Adjust the view.',hidden:'All functions are hidden.',hide:'Hide function',show:'Show function',function:'Function',color:'Curve color',remove:'Remove function',expand:'Expand graph',collapse:'Back',unavailable:'Function catalogue unavailable. Try reopening this page.',noMatch:'No matching function. Try another name.'
    }:{
        ready:'Zadaj funkciu a vykresli ju.',busy:'Počítam body…',network:'Server nie je dostupný. Skús to znova.',invalid:'Server vrátil neplatné údaje grafu.',
        range:'Tieto súradnice prehliadač nedokáže rozlíšiť vo svojom číselnom rozsahu. Uprav rozsah alebo obnov pohľad.',column:'pozícia',
        limited:'Niektoré viditeľné úseky sa nepodarilo rozlíšiť. Priblíž graf pre viac detailov.',ambiguous:'Použi jednu premennú, napríklad x alebo t.',
        dense:'Husté oscilácie sú zobrazené ako približný rozsah. Priblíž graf pre jednotlivé vlny.',noStroke:'Tu sa nepodarilo rozlíšiť súvislú krivku. Priblíž graf pre viac detailov.',noReal:'V tomto rozsahu x sa nenašli reálne body. Skús iný rozsah.',outside:'Krivka je mimo tohto pohľadu. Posuň graf alebo ho oddiaľ.',domain:'Časti mimo reálnej domény sa nevykresľujú.',numeric:'Hodnoty presahujú číselný rozsah grafu v prehliadači. Uprav pohľad.',hidden:'Všetky funkcie sú skryté.',hide:'Skryť funkciu',show:'Zobraziť funkciu',function:'Funkcia',color:'Farba krivky',remove:'Odstrániť funkciu',expand:'Rozšíriť graf',collapse:'Späť',unavailable:'Katalóg funkcií nie je dostupný. Skús znovu otvoriť stránku.',noMatch:'Žiadna funkcia sa nenašla. Skús iný názov.'
    };
    const errors=english?{'undefined answer':'Graphs do not use calculator ans.'}:{'division by zero':'Delenie nulou','undefined variable':'Nedefinovaná premenná','undefined answer':'Graf nepoužíva ans z kalkulačky','syntax error':'Chyba syntaxe','time limit exceeded':'Prekročený časový limit','invalid argument':'Neplatný argument'};
    function message(value,error=false){status.textContent=value+(!error&&verificationState==='limited'?(english?' · Some intersections could not be verified. Zoom in to retry.':' · Niektoré priesečníky sa nepodarilo overiť. Skús priblížiť graf.'):'');status.classList.toggle('error',error);status.dataset.detail=String(error||verificationState==='limited'||curves.some(c=>{const s=c.state();return s.visible&&(s.error||s.notice);}));}
    function settings(){const first=curves[0];return {version:2,expression:first?.input.value||'',color:first?.color.value||palette[0],view:{x:bounds().slice(),y:yViewport.slice()},curves:curves.map(c=>({expression:c.input.value,color:c.color.value,render:'auto',visible:c.state().visible,plotted:c.hasResult()}))};}
    function save(){if(restoring||disposed)return;try{sessionStorage.setItem('numforge-graph-state',JSON.stringify(settings()));}catch(_) {}}
    function drawSoon(){if(drawFrame===null)drawFrame=requestAnimationFrame(()=>{drawFrame=null;if(!disposed)draw();});}
    function dimensions(){
        const result=root.querySelector('.graph-result');
        if(!$('graph-expanded').open){const width=Math.max(320,Math.min(800,result.clientWidth-48));return {width,height:width<600?300:440};}
        const rendered=svg.clientWidth||result.clientWidth-48,width=Math.max(320,Math.min(2048,rendered));
        const available=root.querySelector('.graph-canvas').clientHeight;
        return {width,height:Math.round(Math.max(160,Math.min(1200,available*width/rendered)))};
    }
    function hasVisibleStroke(state){
        if(visibleBands(state).length)return true;
        const [low,high]=bounds(),[bottom,top]=yViewport;
        if(state.discrete)return (state.chunks||[{points:state.samples||[]}]).some(chunk=>chunk.points.some(p=>p.plottable&&p.nx>=Math.max(low,chunk.low??low)&&p.nx<=Math.min(high,chunk.high??high)&&p.ny>=bottom&&p.ny<=top));
        return (state.chunks||[{points:state.samples||[]}]).some(chunk=>chunk.points.some((p,i)=>{
            const a=chunk.points[i-1];return a?.plottable&&p.plottable&&p.connect&&
                a.nx<Math.min(high,chunk.high??high)&&p.nx>Math.max(low,chunk.low??low)&&
                Math.max(a.ny,p.ny)>=bottom&&Math.min(a.ny,p.ny)<=top;
        }));
    }
    function visibleBands(state){
        const [low,high]=bounds(),[bottom,top]=yViewport;
        return (state.chunks||[{bands:state.bands}]).flatMap(chunk=>(chunk.bands||[]).filter(b=>b[0]<Math.min(high,chunk.high??high)&&b[1]>Math.max(low,chunk.low??low)&&Math.max(b[2],bottom)<Math.min(b[3],top)).map(b=>[Math.max(low,chunk.low??low,b[0]),Math.min(high,chunk.high??high,b[1]),b[2],b[3]]));
    }
    function syncStatus(){
        const allStates=curves.map(c=>c.state()),states=allStates.filter(s=>s.visible);
        root.toggleAttribute('aria-busy',states.some(s=>s.busy));if(root.hasAttribute('aria-busy'))root.setAttribute('aria-busy','true');
        curves.forEach((c,i)=>{const s=allStates[i],node=c.row.querySelector('.graph-curve-status');node.hidden=!s.visible||curves.length===1||(!s.error&&!s.busy&&!s.notice);node.textContent=s.message;node.classList.toggle('error',s.error);node.dataset.notice=s.notice;});
        if(!states.length){message(text.hidden);return;}
        if(states.length===1){message(states[0].message,states[0].error);return;}
        const drawn=states.filter(hasVisibleStroke).length,failed=states.filter(s=>s.error).length,unresolved=states.filter(s=>s.notice==='limited').length,dense=states.filter(s=>visibleBands(s).length).length,other=states.filter(s=>s.notice&&!['limited','dense'].includes(s.notice)).length;
        message(states.some(s=>s.busy||s.pendingView)?text.busy:drawn?(english?`${drawn} ${drawn===1?'function':'functions'} plotted`:`Zobrazené funkcie: ${drawn}`)+(dense?(english?` · ${dense} as dense ranges`:` · husté oscilácie: ${dense}`):'')+(failed?(english?` · ${failed} failed`:` · s chybou: ${failed}`):'')+(unresolved?(english?` · ${unresolved} with unresolved detail`:` · s nevyriešeným detailom: ${unresolved}`):'')+(other?(english?' · See function notes.':' · Pozri poznámky pri funkciách.'):''):states.find(s=>s.error)?.message||states.find(s=>s.notice)?.message||text.noStroke,failed>0);
    }
    // Serialize C requests; user-visible work takes priority over speculative tiles.
    const queue=[];let transmitting=false,foregroundBatches=0;
    function transport(query,expression,signal,background,path='/api/graph/plot'){return new Promise((resolve,reject)=>{
        const job={query,expression,signal,background,path,resolve,reject};
        job.cancel=()=>{const i=queue.indexOf(job);if(i>=0)queue.splice(i,1);reject(new DOMException('Aborted','AbortError'));};
        if(signal.aborted){job.cancel();return;}signal.addEventListener('abort',job.cancel,{once:true});queue.push(job);pump();
    });}
    async function pump(){
        if(transmitting||disposed||!queue.length)return;
        let i=queue.findIndex(job=>!job.background);if(i<0){if(foregroundBatches)return;i=0;}
        const job=queue.splice(i,1)[0];job.signal.removeEventListener('abort',job.cancel);
        if(job.signal.aborted){job.cancel();pump();return;}transmitting=true;
        try{
            const start=performance.now(),response=await fetch(job.path+(String(job.query)?'?'+job.query:''),{method:'POST',headers:{'Content-Type':'text/plain; charset=utf-8'},body:job.expression,signal:job.signal});
            const payload=await response.text();job.resolve({response,payload,cost:performance.now()-start});
        }catch(error){job.reject(error);}finally{transmitting=false;pump();}
    }
    async function plotAll(event,keepView=false){
        event?.preventDefault();clearTimeout(viewportTimer);viewportTimer=null;
        if(!$('graph-form').reportValidity())return;
        svg.dataset.featuresState='pending';
        ++foregroundBatches;
        try{await Promise.all(curves.filter(c=>c.state().visible).map(c=>c.plot(null,keepView)));}finally{--foregroundBatches;pump();}
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
    // Intersections of the displayed interpolants, not a symbolic equation
    // solver. Partition a normalized cubic at its stationary points so tangent
    // contacts and multiple crossings in one accepted edge are not skipped.
    function featureRoots(values){
        const scale=Math.max(...values.map(Math.abs));if(!Number.isFinite(scale)||scale===0)return [];
        const [v0,v1,v2,v3]=values.map(v=>v/scale),a=(-v0+3*v1-3*v2+v3)*4.5,b=(2*v0-5*v1+4*v2-v3)*4.5,c=(-11*v0+18*v1-9*v2+2*v3)/2,d=v0;
        const value=t=>((a*t+b)*t+c)*t+d,tolerance=128*Number.EPSILON,turns=[0,1],roots=[];
        if(Math.abs(a)<tolerance){if(Math.abs(b)>tolerance)turns.push(-c/(2*b));}
        else{
            const A=3*a,B=2*b,discriminant=B*B-4*A*c;
            if(discriminant>=0){const q=-.5*(B+(B>=0?1:-1)*Math.sqrt(discriminant));if(q!==0)turns.push(q/A,c/q);else turns.push(-B/(2*A));}
        }
        const split=turns.filter(t=>Number.isFinite(t)&&t>=0&&t<=1).sort((x,y)=>x-y);
        for(let i=0;i<split.length;++i){
            let low=split[i],left=value(low);if(Math.abs(left)<=tolerance)roots.push(low);
            if(i+1===split.length)continue;let high=split[i+1],right=value(high);
            if(high<=low||Math.abs(left)<=tolerance||Math.abs(right)<=tolerance||(left<0)===(right<0))continue;
            for(let j=0;j<42;++j){const mid=low+(high-low)/2,y=value(mid);if(y===0){low=high=mid;break;}if((y<0)===(left<0)){low=mid;left=y;}else high=mid;}
            roots.push(low+(high-low)/2);
        }
        return roots;
    }
    function graphFeatures(states,p){
        const groups=new Map(),edgeSets=[],maximum=128;let work=0;
        function add(kind,curve,other,x,y,low=x,high=x,point=false,trusted=false){
            if(!Number.isFinite(x)||!Number.isFinite(y)||x<p.xBounds[0]||x>p.xBounds[1]||y<p.yBounds[0]||y>p.yBounds[1])return;
            const ids=[curve.id,...(other?[other.id]:[])].sort((a,b)=>a-b),key=kind+':'+ids.join(':');
            const column=Math.min(63,Math.max(0,Math.floor((p.px(x)-64)/(p.right-64)*64))),row=Math.min(15,Math.max(0,Math.floor((p.py(y)-22)/(p.bottom-22)*16))),cell=column+64*row;
            if(!groups.has(key))groups.set(key,new Map());const bucket=groups.get(key),previous=bucket.get(cell);
            const distance=(p.px(x)-(64+(column+.5)*(p.right-64)/64))**2+(p.py(y)-(22+(row+.5)*(p.bottom-22)/16))**2;
            if(!previous||distance<previous.distance)bucket.set(cell,{kind,key,curve,other,x:x===0?0:x,y:y===0?0:y,low:Math.min(low,x),high:Math.max(high,x),point,verified:trusted,distance});
        }
        for(const state of states){
            const edges=[];
            if(state.error||!state.samples){edgeSets.push(edges);continue;}
            if(state.discrete){for(const point of state.samples)if(point.plottable){if(point.nx===0)add('y-axis',state.curve,null,0,point.ny,0,0,true,true);if(point.ny===0)add('x-axis',state.curve,null,point.nx,0,point.nx,point.nx,true,true);}edgeSets.push(edges);continue;}
            for(const chunk of state.chunks||[{points:state.samples}]){
                const bands=chunk.bands||state.bands||[];
                for(let i=1;i<chunk.points.length;++i){
                    if(++work>65536)break;
                    const a=chunk.points[i-1],b=chunk.points[i];
                    if(!a.plottable||!b.plottable||!b.connect||b.nx<=a.nx)continue;
                    const low=Math.max(a.nx,chunk.low??-Infinity,p.xBounds[0]),high=Math.min(b.nx,chunk.high??Infinity,p.xBounds[1]);if(high<=low)continue;
                    let first=0,last=bands.length;while(first<last){const mid=(first+last)>>>1;if(bands[mid][1]<=low)first=mid+1;else last=mid;}
                    if(bands[first]&&bands[first][0]<high)continue;
                    const control=b.subpixelHermite||p.px(b.nx)-p.px(a.nx)>1?cubic(a,b):null;
                    const at=x=>{const t=(x-a.nx)/(b.nx-a.nx);return control?(1-t)**3*a.ny+3*(1-t)**2*t*control[1]+3*(1-t)*t*t*control[3]+t**3*b.ny:a.ny*(1-t)+b.ny*t;};
                    const edge={low,high,at};edges.push(edge);
                    if(low<=0&&high>=0)add('y-axis',state.curve,null,0,at(0));
                    if(p.yBounds[0]<=0&&p.yBounds[1]>=0){
                        for(const t of featureRoots([at(low),at(low+(high-low)/3),at(low+2*(high-low)/3),at(high)]))add('x-axis',state.curve,null,low+(high-low)*t,0,low,high);
                    }
                }
            }
            edges.sort((a,b)=>a.low-b.low);edgeSets.push(edges);
        }
        for(let i=0;i<edgeSets.length;++i)for(let j=i+1;j<edgeSets.length;++j){
            if(states[i].discrete||states[j].discrete){
                const discrete=states[i].discrete?states[i]:states[j],other=discrete===states[i]?states[j]:states[i];
                for(const point of discrete.samples||[]){if(++work>65536)break;if(!point.plottable)continue;const candidate=inspectAt(other.curve,point.nx);
                    if(candidate&&candidate.x===point.nx&&Math.abs(p.py(candidate.y)-p.py(point.ny))<=1)add('intersection',discrete.curve,other.curve,point.nx,point.ny,point.nx,point.nx,true);
                }continue;
            }
            const left=edgeSets[i],right=edgeSets[j];let a=0,b=0;
            while(a<left.length&&b<right.length&&work++<65536){
                const e=left[a],f=right[b],low=Math.max(e.low,f.low),high=Math.min(e.high,f.high);
                if(high>low){
                    const xs=[low,low+(high-low)/3,low+2*(high-low)/3,high],ys=xs.map(x=>e.at(x)),zs=xs.map(x=>f.at(x));
                    // Coincident strokes have no isolated intersection marker.
                    const differences=ys.map((y,k)=>y-zs[k]),scale=Math.max(...ys.map(Math.abs),...zs.map(Math.abs));
                    if(differences.some(v=>Math.abs(v)>128*Number.EPSILON*scale))for(const t of featureRoots(differences)){
                        const x=low+(high-low)*t;add('intersection',states[i].curve,states[j].curve,x,e.at(x),low,high);
                    }
                }
                if(e.high<=f.high)++a;if(f.high<=e.high)++b;
            }
        }
        // Give each axis/function-pair group a turn. Dyadic ordering spreads
        // each group's quota over the full view instead of consuming it at the
        // left edge or on the first oscillating function.
        const queues=[...groups.values()].map(bucket=>{
            const values=[...bucket.values()].sort((a,b)=>a.x-b.x||a.y-b.y),order=[],pending=[[0,values.length-1]];
            if(values.length){order.push(values[0]);if(values.length>1)order.push(values.at(-1));}
            pending[0]=[1,values.length-2];
            for(let i=0;i<pending.length;i++){const [low,high]=pending[i];if(high<low)continue;const mid=(low+high)>>>1;order.push(values[mid]);pending.push([low,mid-1],[mid+1,high]);}return order;
        });
        const found=[];for(let i=0;found.length<maximum&&queues.some(q=>i<q.length);i++)for(const queue of queues)if(queue[i]&&found.length<maximum)found.push(queue[i]);
        return found;
    }
    function verifyFeatures(key){
        verificationRequest?.abort();clearTimeout(verificationTimer);
        for(const feature of features){
            feature.verificationKey=JSON.stringify([feature.curve.input.value,feature.other?.input.value||'',feature.kind,feature.point,feature.x,feature.low,feature.high,...projection.xBounds,...projection.yBounds,projection.right,projection.bottom]);
            if(verificationCache.has(feature.verificationKey)){const cached=verificationCache.get(feature.verificationKey);feature.checked=true;if(cached){feature.x=cached.x;feature.y=cached.y;feature.residual=cached.residual;feature.verified=true;}}
        }
        const request=verificationRequest=new AbortController(),pending=features.filter(f=>!f.verified&&!f.checked);
        verificationState=pending.length?'pending':'ready';svg.dataset.featuresState=verificationState;syncStatus();
        if(!pending.length)return;
        // Let camera gestures settle. This work shares the transport queue but
        // never delays foreground plots and is cancelled by newer geometry.
        verificationTimer=setTimeout(async()=>{
            const groups=new Map(),encoder=new TextEncoder();let limited=false;
            for(const feature of pending){const id=feature.curve.id+':'+(feature.other?.id||'');if(!groups.has(id))groups.set(id,[]);groups.get(id).push(feature);}
            try{
                for(const group of groups.values()){
                    const first=group[0],prefix=first.curve.input.value+'\n'+(first.other?.input.value||'')+'\n';
                    let batch=[],body=prefix;
                    async function send(){
                        if(!batch.length)return;
                        const current=batch;batch=[];
                        const result=await transport('',body,request.signal,true,'/api/graph/verify');
                        if(result.payload.length>128*1024)throw Error(text.invalid);const data=JSON.parse(result.payload);
                        if(!result.response.ok||!data.ok||data.schema_version!==1||data.count!==current.length||typeof data.limited!=='boolean'||!Array.isArray(data.points)||data.points.length!==current.length)throw Error(text.invalid);
                        if(disposed||request.signal.aborted||featureCacheKey!==key)return;
                        limited=limited||data.limited;
                        data.points.forEach((point,i)=>{const feature=current[i];if(point===null){if(!data.limited)rememberVerification(feature.verificationKey,null);return;}if(!Number.isFinite(point.x)||!Number.isFinite(point.y)||!Number.isFinite(point.residual)||point.residual<0||typeof point.approximate!=='boolean'||point.x<feature.low||point.x>feature.high||Math.abs(point.x-feature.x)>Math.max(feature.high-feature.low,(projection.xBounds[1]-projection.xBounds[0])/(projection.right-64)))throw Error(text.invalid);feature.x=point.x;feature.y=point.y;feature.verified=true;feature.residual=point.residual;rememberVerification(feature.verificationKey,point);});
                        body=prefix;drawSoon();
                    }
                    for(const feature of group){
                        const kind=feature.kind==='y-axis'?1:feature.kind==='x-axis'?0:feature.point?3:2,p=projection;
                        const row=[kind,feature.x,feature.low,feature.high,Math.max(Number.EPSILON*Math.max(1,Math.abs(feature.x))*16,(p.xBounds[1]-p.xBounds[0])/(p.right-64)*.001),Math.max(Number.MIN_VALUE,(p.yBounds[1]-p.yBounds[0])/(p.bottom-22)*.001)].join(' ')+'\n';
                        if(batch.length===32||encoder.encode(body+row).length>4096)await send();
                        if(encoder.encode(prefix+row).length>4096){limited=true;continue;}
                        batch.push(feature);body+=row;
                    }
                    await send();
                }
            }catch(error){if(error.name!=='AbortError')limited=true;}
            finally{if(!disposed&&!request.signal.aborted&&featureCacheKey===key){verificationState=limited?'limited':'ready';syncStatus();drawSoon();}}
        },120);
    }
    function featureMarkers(){
        const confirmed=features.filter(feature=>feature.verified),matrix=svg.getScreenCTM();
        const compact=svg.getBoundingClientRect().width<=600||window.matchMedia('(max-width:900px)').matches;
        const spacing=compact&&confirmed.length>12&&matrix?24:0;
        svg.dataset.featureCount=String(confirmed.length);svg.dataset.markerSpacing=String(spacing);
        if(!spacing)return confirmed;
        // Thin only the presentation, in real CSS pixels. Give each curve a
        // turn and keep its original candidate order; inspection retains all.
        const shown=[],positions=[],groups=new Map();
        for(const feature of confirmed){if(!groups.has(feature.curve))groups.set(feature.curve,{values:[],next:0});groups.get(feature.curve).values.push(feature);}
        let added=true;
        while(added){added=false;for(const queue of groups.values())while(queue.next<queue.values.length){
            const feature=queue.values[queue.next++];
            if(feature.x<projection.xBounds[0]||feature.x>projection.xBounds[1]||feature.y<projection.yBounds[0]||feature.y>projection.yBounds[1])continue;
            const x=projection.px(feature.x),y=projection.py(feature.y),sx=matrix.a*x+matrix.c*y,sy=matrix.b*x+matrix.d*y;
            if(positions.some(point=>Math.hypot(point.x-sx,point.y-sy)<spacing))continue;
            positions.push({x:sx,y:sy});shown.push(feature);added=true;break;
        }}
        return shown;
    }
    // Read only accepted cached edges. Binary search and the drawing's own cubic
    // interpolation keep inspection local, bounded and consistent with the stroke.
    function inspectAt(curve,x){
        if(!curve)return null;
        const state=curve.state();if(!state.visible||!state.samples)return null;
        if(state.discrete){let best=null,distance=13;for(const point of state.samples){if(!point.plottable||point.nx<projection.xBounds[0]||point.nx>projection.xBounds[1])continue;const delta=Math.abs(projection.px(point.nx)-projection.px(x));if(delta<distance){distance=delta;best={curve,x:point.nx,y:point.ny,kind:'discrete'};}}return best;}
        for(const chunk of state.chunks||[{points:state.samples}]){
            if(x<(chunk.low??-Infinity)||x>(chunk.high??Infinity))continue;
            if((chunk.bands||state.bands||[]).some(b=>x>=b[0]&&x<=b[1]))continue;
            const points=chunk.points;let low=0,high=points.length;
            while(low<high){const mid=(low+high)>>>1;if(points[mid].nx<x)low=mid+1;else high=mid;}
            const b=points[low],a=points[low-1];if(!b?.plottable)continue;
            if(b.nx===x){
                const next=points[low+1];
                if((a?.plottable&&b.connect)||(next?.plottable&&next.connect))return {curve,x,y:b.ny};
                continue;
            }
            if(!a?.plottable||!b.connect||b.nx<=a.nx)continue;
            const t=(x-a.nx)/(b.nx-a.nx),control=b.subpixelHermite||projection.px(b.nx)-projection.px(a.nx)>1?cubic(a,b):null;
            const y=control?(1-t)**3*a.ny+3*(1-t)**2*t*control[1]+3*(1-t)*t*t*control[3]+t**3*b.ny:a.ny*(1-t)+b.ny*t;
            if(Number.isFinite(y))return {curve,x,y};
        }
        return null;
    }
    function inspectionPoint(clientX,clientY,near=false){
        if(!projection)return null;
        const matrix=svg.getScreenCTM();if(!matrix)return null;
        const point=new DOMPoint(clientX,clientY).matrixTransform(matrix.inverse()),p=projection;
        if(point.x<64||point.x>p.right||point.y<22||point.y>p.bottom)return null;
        const fraction=(point.x-64)/(p.right-64),x=(p.xLow+(p.xHigh-p.xLow)*fraction)*p.xScale;
        let closest=null,featureDistance=13;
        for(const feature of features){if(!feature.verified)continue;const dx=p.px(feature.x)-point.x,dy=p.py(feature.y)-point.y,delta=Math.hypot(matrix.a*dx+matrix.c*dy,matrix.b*dx+matrix.d*dy);if(delta<featureDistance-1e-6||(closest&&Math.abs(delta-featureDistance)<=1e-6&&displayedFeatures.has(feature)&&!displayedFeatures.has(closest))){closest=feature;featureDistance=delta;}}
        if(closest)return closest;
        let best=null,distance=Infinity;
        for(const curve of curves){
            const candidate=inspectAt(curve,x);if(!candidate||candidate.y<p.yBounds[0]||candidate.y>p.yBounds[1])continue;
            const dx=candidate.kind==='discrete'?p.px(candidate.x)-point.x:0,dy=p.py(candidate.y)-point.y;
            const delta=Math.hypot(matrix.a*dx+matrix.c*dy,matrix.b*dx+matrix.d*dy);
            if(delta<distance){best=candidate;distance=delta;}
        }
        return near&&distance>32?null:best;
    }
    function clearInspection(){inspection=null;readout.hidden=true;inspectionLayer.setAttribute('visibility','hidden');}
    function coordinateLabel(value,span,pixels){
        const digits=Math.max(0,Math.min(14,Math.ceil(-Math.log10(span/Math.max(1,pixels)))+2));
        const rounded=Math.abs(value)<.5*10**-digits?0:value;
        return rounded.toLocaleString(english?'en':'sk',Math.abs(rounded)>=1e9||(rounded!==0&&Math.abs(rounded)<1e-6)?
            {useGrouping:false,notation:'scientific',maximumSignificantDigits:15}:{useGrouping:false,maximumFractionDigits:digits});
    }
    function updateInspection(){
        if(disposed||!projection||!inspection){readout.hidden=true;inspectionLayer.setAttribute('visibility','hidden');return;}
        const selected=inspection.kind==='hover'?inspectionPoint(inspection.clientX,inspection.clientY):
            inspection.featureKey?features.find(f=>f.key===inspection.featureKey&&Math.abs(f.x-inspection.x)<(projection.xBounds[1]-projection.xBounds[0])*1e-6):inspectAt(curves.find(c=>c.id===inspection.curveId),inspection.x);
        const p=projection;
        if(!selected||selected.x<p.xBounds[0]||selected.x>p.xBounds[1]||selected.y<p.yBounds[0]||selected.y>p.yBounds[1]){
            readout.hidden=true;inspectionLayer.setAttribute('visibility','hidden');return;
        }
        const {curve,x,y}=selected,screenX=p.px(x),screenY=p.py(y);
        for(const [node,attrs] of [[inspectX,{x1:screenX,x2:screenX,y1:22,y2:p.bottom}],[inspectY,{x1:64,x2:p.right,y1:screenY,y2:screenY}],[inspectDot,{cx:screenX,cy:screenY}]])for(const [key,value] of Object.entries(attrs))node.setAttribute(key,value);
        inspectionLayer.style.color=curve.color.value;inspectionLayer.setAttribute('clip-path','url(#graph-clip)');inspectionLayer.setAttribute('visibility','visible');svg.append(inspectionLayer);
        readout.style.setProperty('--inspection-color',curve.color.value);readout.dataset.curve=String(curve.id);readout.dataset.x=String(x);readout.dataset.y=String(y);
        const label=selected.kind==='x-axis'?(english?'x-axis intercept':'Priesečník s osou x'):selected.kind==='y-axis'?(english?'y-axis intercept':'Priesečník s osou y'):selected.kind==='intersection'?(english?'Intersection':'Priesečník'):'';
        $('graph-inspector-name').textContent=label?`${label} · ${curves.indexOf(curve)+1}${selected.other?' ∩ '+(curves.indexOf(selected.other)+1):''}`:`${curves.indexOf(curve)+1}. ${curve.input.value}`;
        readout.dataset.feature=selected.kind||'';
        $('graph-inspector-coordinates').textContent=`≈ x = ${coordinateLabel(x,p.xBounds[1]-p.xBounds[0],p.right-64)}, y = ${coordinateLabel(y,p.yBounds[1]-p.yBounds[0],p.bottom-22)}`;
        readout.hidden=false;
    }
    function inspectSoon(){if(inspectionFrame===null)inspectionFrame=requestAnimationFrame(()=>{inspectionFrame=null;updateInspection();});}
    function draw() {
        svg.querySelectorAll('.graph-drawing').forEach(el=>el.remove());
        const states=curves.filter(c=>c.state().visible).map(curve=>({curve,...curve.state()}));
        for(const state of states)if(state.samples&&!state.pendingView&&!state.busy&&!state.error)state.curve.describe();
        let xs=viewport||bounds();
        for(const state of states)if(state.samples&&(state.chunks||[{points:state.samples}]).some(c=>c.points.some((p,i)=>p.nx===null||(i>0&&p.nx<=c.points[i-1].nx)))){state.curve.invalidData();draw();return;}
        // Scale before subtraction so finite extreme values cannot overflow.
        const xScale=Math.max(Math.abs(xs[0]),Math.abs(xs.at(-1)),Number.MIN_VALUE);
        const projectedView=yViewport;
        const rawScale=Math.max(...projectedView.map(Math.abs));
        const yScale=rawScale===0?1:Math.max(rawScale,Number.MIN_VALUE);
        const xLow=xs[0]/xScale,xHigh=xs.at(-1)/xScale;
        if(xHigh<=xLow){projection=null;clearInspection();message(text.range,true);return;}
        let yLow=projectedView[0]/yScale,yHigh=projectedView[1]/yScale;
        const {width,height}=dimensions(),right=width-22,bottom=height-46;
        svg.setAttribute('viewBox',`0 0 ${width} ${height}`);
        svg.setAttribute('preserveAspectRatio',$('graph-expanded').open?'none':'xMidYMid meet');
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
        if(!hasVisibleStroke(state))continue;
        if(state.discrete){
            const seen=new Set();
            for(const chunk of state.chunks||[{points:state.samples}])for(const point of chunk.points){
                if(!point.plottable||point.nx<Math.max(xs[0],chunk.low??-Infinity)||point.nx>Math.min(xs.at(-1),chunk.high??Infinity)||point.ny<projectedView[0]||point.ny>projectedView[1]||seen.has(point.nx))continue;
                seen.add(point.nx);group.append(element('circle',{class:'graph-discrete-point',cx:px(point.nx),cy:py(point.ny),r:3.5,'data-curve':state.curve.id,'data-x':point.nx,'data-y':point.ny,style:`fill:${state.curve.color.value}`,'clip-path':'url(#graph-clip)'}));
            }continue;
        }
        const bands=visibleBands(state);let bandPath='';
        for(const b of bands){const a=px(b[0]),end=px(b[1]);let top=py(Math.min(projectedView[1],b[3])),base=py(Math.max(projectedView[0],b[2]));
            // Raster visibility only: retain numeric bounds, but keep a subpixel
            // range visible as a one-pixel trace clipped to the camera.
            if(base-top<1){const middle=(base+top)/2;top=middle-.5;base=middle+.5;}
            bandPath+=`M${a},${top}H${end}V${base}H${a}Z`;}
        if(bandPath)group.append(element('path',{d:bandPath,class:'graph-band','data-curve':state.curve.id,style:`fill:${state.curve.color.value}`,'clip-path':'url(#graph-clip)'}));
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
            const subpixel=run.chunks.some(chunk=>chunk.points.some(point=>point.subpixelHermite));
            const curve=element('path',{class:'graph-line'+(subpixel?' graph-line-subpixel':''),'data-curve':state.curve.id,style:`stroke:${state.curve.color.value}`,'clip-path':`url(#${id})`});
            let path='',previous=null,pen=false;
            for(const chunk of run.chunks)for(let i=0;i<chunk.points.length;++i) {
                const point=chunk.points[i];
                // Both copies of a joined boundary are the same C sample.
                if(i===0&&chunk.joinedLeft)continue;
                if(!point.plottable){previous=null;pen=false;continue;}
                const x=px(point.nx),y=py(point.ny);
                if(point.connect && previous){
                    if(!pen)path+=`M${previous.x},${previous.y}`;
                    const control=point.subpixelHermite||x-previous.x>1?cubic(previous.point,point):null;
                    path+=control?`C${px(control[0])},${py(control[1])},${px(control[2])},${py(control[3])},${x},${y}`:`L${x},${y}`;pen=true;
                }else pen=false;
                previous={x,y,point};
            }
            if(path){curve.setAttribute('d',path);group.append(curve);}
        }
        }
        $('graph-svg-description').textContent=states.some(s=>s.samples)?states.map(s=>s.curve.input.value).filter(Boolean).join('; '):text.ready;
        svg.append(group);svg.removeAttribute('hidden');
        projection={px,py,xScale,xLow,xHigh,xBounds:xs,yBounds:projectedView,right,bottom};
        // One bounded geometry snapshot: color/status-only redraws must not
        // solve the same intersections again. Camera, samples or visibility
        // changes invalidate it; it never holds a second set of sample arrays.
        const featureKey=[...xs,...projectedView,width,height,...states.flatMap(state=>[state.curve,state.curve.input.value,state.samples,state.chunks,state.bands,state.error])];
        if(!featureCacheKey||featureKey.length!==featureCacheKey.length||featureKey.some((value,i)=>value!==featureCacheKey[i])){features=graphFeatures(states,projection);featureCacheKey=featureKey;verifyFeatures(featureKey);}
        const markers=element('g',{class:'graph-features','clip-path':'url(#graph-clip)'});
        const shownFeatures=featureMarkers();displayedFeatures=new Set(shownFeatures);svg.dataset.markerCount=String(shownFeatures.length);
        for(const feature of shownFeatures){const marker=element('circle',{class:'graph-feature',cx:px(feature.x),cy:py(feature.y),r:4,'data-kind':feature.kind,'data-x':feature.x,'data-y':feature.y,'data-verified':'true','data-curve':feature.curve.id,...(feature.other?{'data-other':feature.other.id}:{}),style:`fill:${feature.curve.color.value}`});markers.append(marker);}
        group.append(markers);svg.dataset.featuresState=states.some(s=>s.busy||s.pendingView)?'pending':verificationState;updateInspection();
    }
    function createCurve(row){
        const input=row.querySelector('input:not([type=color])'),color=row.querySelector('input[type=color]'),controller=new AbortController();
        const id=nextId++;row.dataset.curve=String(id);
        let visible=row.querySelector('.graph-visibility').getAttribute('aria-pressed')!=='false',hiddenCamera=null,hiddenReady=false;
        const cameraKey=()=>JSON.stringify([bounds(),yViewport,dimensions()]);
        let generation=0,request=null,samples=null,bands=null,disposed=false,parameter='x',limited=false,pendingView=false,approximate=false,busy=false,discrete=false,pointStride=1;
        const identityKey=()=>JSON.stringify([input.value,'auto']);
        let cache=[],cacheIdentity=null,chunks=null,prefetchTimer=null,prefetchRequest=null,prefetchToken=0,prefetchCamera=null,cacheMissing=[],cacheCovered=false;
        const prefetchAttempts=new Set(),maxPoints=8192,maxCachePoints=12288,maxCacheBytes=8*1024*1024;
        let lastMessage=text.ready,lastError=false,lastNotice='';
        function message(value,error=false,notice=''){if(lastMessage===value&&lastError===error&&lastNotice===notice)return;lastMessage=value;lastError=error;lastNotice=notice;syncStatus();}
        function stopPrefetch(){++prefetchToken;clearTimeout(prefetchTimer);prefetchTimer=null;prefetchRequest?.abort();prefetchRequest=null;}
        function clearCache(){stopPrefetch();cache=[];cacheIdentity=null;prefetchCamera=null;prefetchAttempts.clear();}
        function invalidate(){verificationRequest?.abort();clearTimeout(verificationTimer);++generation;request?.abort();request=null;busy=false;clearCache();samples=null;bands=null;chunks=null;pendingView=false;limited=false;discrete=false;pointStride=1;parameter='x';message(text.ready);syncStatus();drawSoon();}
        function visibleUnresolved() {
            if(discrete||!limited||!samples)return false;
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
            const discrete=body.render==='points'&&body.schema_version===3&&body.evaluation==='precision'&&body.sampling==='discrete';
            if(body.render!==undefined&&!discrete)throw Error(text.invalid);
            if(discrete&&(!Number.isInteger(body.point_stride)||body.point_stride<1||body.count>256||body.points?.some(p=>p[2]!==0||!Number.isSafeInteger(p[0]))))throw Error(text.invalid);
            const cameraSchema=(body.schema_version===3&&body.evaluation==='double')||(body.schema_version===2&&body.evaluation==='precision');
            if(body.screen_clipped!==undefined&&(!cameraSchema||typeof body.screen_clipped!=='boolean'))throw Error(text.invalid);
            if(body.bands!==undefined&&(body.schema_version!==3||body.evaluation!=='double'||body.dense_representation!=='pixel-envelope'||!Array.isArray(body.bands)||body.bands.length>2048||body.bands.some((b,i)=>!Array.isArray(b)||b.length!==4||b.some(v=>!Number.isFinite(v))||b[0]>=b[1]||b[2]>b[3]||(i>0&&b[0]<body.bands[i-1][1]))))throw Error(text.invalid);
            const slopes=body.slopes;
            if(body.subpixel_hermite!==undefined&&(body.subpixel_hermite!==true||body.schema_version!==3||body.evaluation!=='double'||!Array.isArray(slopes)))throw Error(text.invalid);
            if(slopes!==undefined&&(body.schema_version!==3||!Array.isArray(slopes)||slopes.length!==body.count||slopes.some(s=>s!==null&&!Number.isFinite(s))))throw Error(text.invalid);
            if(body.schema_version===3){
                const kinds=['real','domain_error','complex','quantity','unresolved'];
                if((body.evaluation!=='double'&&!discrete)||!Array.isArray(body.points)||body.count!==body.points.length||body.count<(discrete?0:2)||body.count>maxPoints)throw Error(text.invalid);
                body={...body,y_min:body.y_min===null?null:String(body.y_min),y_max:body.y_max===null?null:String(body.y_max),points:body.points.map(row=>{
                    if(!Array.isArray(row)||row.length!==5||!Number.isFinite(row[0])||(row[1]!==null&&!Number.isFinite(row[1]))||![0,1].includes(row[2])||!Number.isInteger(row[3])||row[3]<0||row[3]>4||![0,1].includes(row[4]))throw Error(text.invalid);
                    return {x:String(row[0]),y:row[1]===null?null:String(row[1]),connect:!!row[2],kind:kinds[row[3]],source_approximate:!!row[4]};
                })};
            }
            if(![2,3].includes(body.schema_version) || !Array.isArray(body.points) || body.count!==body.points.length || body.count<(discrete?0:2) || body.count>maxPoints)throw Error(text.invalid);
            if(typeof body.parameter!=='string'||!/^[A-Za-z]{1,31}$/.test(body.parameter)||typeof body.limited!=='boolean')throw Error(text.invalid);
            const points=body.points.map((point,i)=>{
                if(!point || typeof point.connect!=='boolean' || !['real','domain_error','complex','quantity',...(body.schema_version===3?['unresolved']:[])].includes(point.kind) || typeof point.x!=='string' || (point.kind==='real'?typeof point.y!=='string'&&!(body.schema_version===3&&point.y===null):point.y!==null))throw Error(text.invalid);
                const nx=numeric(point.x),ny=numeric(point.y);
                return {...point,nx,ny,derivative:slopes?.[i]??NaN,subpixelHermite:body.subpixel_hermite===true,plottable:point.kind==='real'&&nx!==null&&ny!==null};
            });
            if(body.bands?.some(b=>!points.length||b[0]<points[0].nx||b[1]>points.at(-1).nx))throw Error(text.invalid);
            return {points,discrete,pointStride:discrete?body.point_stride:1,bands:body.bands||[],screenClipped:body.screen_clipped===true,parameter:body.parameter,limited:body.limited,approximate:body.evaluation==='double',quality:body.quality||'refined',width:body.width,height:body.height};
        }
        async function fetchPlot(query,expression,signal,background=false) {
            const {response,payload,cost}=await transport(query,expression,signal,background);if(payload.length>512*1024)throw Error(text.invalid);
            const body=JSON.parse(payload);
            if(!response.ok||!body.ok)throw Error((body.code==='graph_parameter_ambiguous'?text.ambiguous:errors[body.status]||body.error||body.status||text.invalid)+(Number.isSafeInteger(body.column)?` · ${text.column} ${body.column}`:''));
            // Conservative cache weight, not an engine-specific heap measurement.
            return {...decode(body),sampleWidth:Number(query.get('width'))||0,verticalSpan:Number(query.get('ymax'))-Number(query.get('ymin'))||0,viewYLow:numeric(query.get('ymin')),viewYHigh:numeric(query.get('ymax')),cacheBytes:payload.length*2+body.points.length*256+(body.bands?.length||0)*128,cost,requestedLow:numeric(query.get('min')),requestedHigh:numeric(query.get('max'))};
        }
        function remember(entry) {
            // Precision previews can already be refined and become the final
            // result. Remembering that same object again must be idempotent,
            // including limited entries which cannot dominate other levels.
            if(cache.includes(entry))return true;
            let low=entry.discrete?entry.requestedLow:entry.points[0].nx,high=entry.discrete?entry.requestedHigh:entry.points.at(-1).nx;
            if(low===null||high===null||high<=low||!Number.isFinite(high-low)||entry.points.some((p,i)=>p.nx===null||(i>0&&p.nx<=entry.points[i-1].nx)))return false;
            const a=entry.requestedLow,b=entry.requestedHigh;
            if(a!==null&&b!==null&&b>a&&Number.isFinite(b-a)) {
                const near=(actual,requested)=>Math.abs(actual-requested)<=Math.max((b-a)*1e-12,Math.abs(requested)*Number.EPSILON*4);
                // HTTP plotting coordinates are rounded. Use requested coverage
                // when endpoints agree to projection precision, avoiding tiny gaps.
                if(near(low,a)&&near(high,b)){low=a;high=b;}
            }
            entry.low=low;entry.high=high;entry.span=high-low;
            // Keep useful resolution levels of the same interval. A coarse
            // preview must not discard a previously refined zoom level.
            const xStep=e=>e.span/(e.width||e.sampleWidth||1),yStep=e=>e.verticalSpan/(e.height||dimensions().height);
            const dominates=(a,b)=>a.quality!=='coarse'&&!a.limited&&a.approximate===b.approximate&&(!a.screenClipped||(b.screenClipped&&coversY(a,b.viewYLow,b.viewYHigh)))&&xStep(a)<=xStep(b)&&yStep(a)<=yStep(b);
            const sameRange=cache.filter(e=>e.low===low&&e.high===high);
            if(sameRange.some(e=>dominates(e,entry)))return true;
            cache=cache.filter(e=>e.low!==low||e.high!==high||!dominates(entry,e));cache.push(entry);
            while(cache.length>12||cache.reduce((n,e)=>n+e.points.length+e.bands.length,0)>maxCachePoints||cache.reduce((n,e)=>n+e.cacheBytes,0)>maxCacheBytes){
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
            cacheMissing=[[low,high]];cacheCovered=false;
            const entries=cache.filter(e=>e.low<high&&e.high>low);
            if(!entries.length)return false;
            const stops=[...new Set([low,high,...entries.flatMap(e=>[Math.max(low,e.low),Math.min(high,e.high)])])].sort((a,b)=>a-b);
            const epsilon=coverageEpsilon(low,high);
            const segments=[];let covered=true,ready=true;cacheMissing=[];
            const missing=(a,b)=>{const previous=cacheMissing.at(-1);if(previous&&previous[1]===a)previous[1]=b;else cacheMissing.push([a,b]);};
            for(let i=1;i<stops.length;++i) {
                const a=stops[i-1],b=stops[i];
                // Inverse zooms can differ by a few floating-point ulps. Such
                // tails need no visible detail and may collapse to repeated
                // samples. Do not create a tile; accepted edges still decide joins.
                if(b-a<=epsilon)continue;
                const entry=entries.filter(e=>e.low<=a+epsilon&&e.high>=b-epsilon).sort((x,y)=>Number(resolutionReady(y,low,high))-Number(resolutionReady(x,low,high))||Number(x.quality==='coarse')-Number(y.quality==='coarse')||x.span-y.span)[0];
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
                return {low:segment.low,high:segment.high,limited:segment.entry.limited,bands:segment.entry.bands.filter(b=>b[0]<segment.high&&b[1]>segment.low),points:p.slice(Math.max(0,start-1),Math.min(p.length,end+1))};
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
            samples=chunks.flatMap(c=>c.points);parameter=segments[0].entry.parameter;discrete=segments[0].entry.discrete;pointStride=Math.max(...segments.map(s=>s.entry.pointStride));
            limited=segments.some(s=>s.entry.limited);
            approximate=segments.some(s=>s.entry.approximate);
            cacheCovered=covered;pendingView=!(covered&&ready);if(paint)drawSoon();
            return covered&&ready;
        }
        function resolutionReady(entry,low,high,yLow=yViewport[0],yHigh=yViewport[1]){
            const {width,height}=dimensions(),sampleWidth=entry.width||entry.sampleWidth;
            if(entry.discrete)return entry.pointStride<=Math.max(1,Math.ceil((high-low+1)/(Math.min(256,Math.floor(width/8))-1)));
            const tolerance=entry.bands.length?1:1.25;
            const roundoff=1+Number.EPSILON*16;
            return coversY(entry,yLow,yHigh)&&entry.quality!=='coarse'&&(!sampleWidth?entry.span<=(high-low)*4*roundoff:entry.span/sampleWidth<=(high-low)/width*tolerance*roundoff)&&(!entry.verticalSpan||entry.verticalSpan/(entry.height||height)<=(yHigh-yLow)/height*1.25*roundoff);
        }
        function coversY(entry,low,high){const epsilon=coverageEpsilon(low,high);return !entry.screenClipped||(entry.viewYLow!==null&&entry.viewYHigh!==null&&low>=entry.viewYLow-epsilon&&high<=entry.viewYHigh+epsilon);}
        function coverageEpsilon(low,high){return Math.max((high-low)*1e-12,Math.max(Math.abs(low),Math.abs(high))*Number.EPSILON*8);}
        function missingPrepared(low,high,viewLow,viewHigh,yLow,yHigh){
            const entries=cache.filter(e=>!e.limited&&resolutionReady(e,viewLow,viewHigh,yLow,yHigh)&&e.low<high&&e.high>low).sort((a,b)=>a.low-b.low);
            const epsilon=coverageEpsilon(low,high),missing=[];let end=low;
            for(const entry of entries){if(entry.low-end>epsilon)missing.push([end,Math.min(high,entry.low)]);end=Math.max(end,Math.min(high,entry.high));if(end>=high-epsilon)break;}
            if(high-end>epsilon)missing.push([end,high]);return missing;
        }
        function prefetch(query,expression,identity,low,high) {
            stopPrefetch();
            const camera=JSON.stringify([low,high,yViewport[1]-yViewport[0]]);
            // A resize with adequate cached foreground coverage does not restart
            // the same speculative batch. Missing visible detail still refines.
            if(prefetchCamera===camera)return;
            const span=high-low,token=prefetchToken;
            const epsilon=coverageEpsilon(low,high);
            const current=cache.find(e=>e.low<=low+epsilon&&e.high>=high-epsilon&&resolutionReady(e,low,high)&&!e.limited);
            // A cheap full-view tile may prepare both neighbors together. Dense
            // views spend their bounded cache/work on the view and next zoom only.
            const buffer=!!current&&current.points.length<=Math.floor(maxPoints/3);
            const center=low+span/2,yMiddle=yViewport[0]+(yViewport[1]-yViewport[0])/2,yQuarter=(yViewport[1]-yViewport[0])/4;
            // The normal canvas also prepares enough width for expansion. Once
            // expanded, prepare just the central next view at the same density.
            const zoomLow=$('graph-expanded').open?center-span/4:low,zoomHigh=$('graph-expanded').open?center+span/4:high;
            const ranges=[...(buffer?[{low:low-span,high:high+span,zoom:false}]:[]),{low:zoomLow,high:zoomHigh,zoom:true}];
            prefetchTimer=setTimeout(async()=>{
                prefetchTimer=null;
                let fetched=0;
                for(const range of ranges) {
                const nextZoom=range.zoom,yLow=nextZoom?yMiddle-yQuarter:yViewport[0],yHigh=nextZoom?yMiddle+yQuarter:yViewport[1];
                const missing=missingPrepared(range.low,range.high,nextZoom?center-span/4:low,nextZoom?center+span/4:high,yLow,yHigh);
                if(!missing.length)continue;
                // Reuse prepared coverage. One missing tail can be sampled alone;
                // two neighbors share one bounded buffer request.
                const jobs=missing.length===1?missing:[[range.low,range.high]];
                for(const [a,b] of jobs) {
                    if(disposed||!visible||token!==prefetchToken||cacheIdentity!==identity||pointers.size)return;
                    if(fetched>=2)return;
                    if(!Number.isFinite(a)||!Number.isFinite(b)||b<=a||!Number.isFinite(b-a))continue;
                    const key=JSON.stringify([identity,a,b,nextZoom,query.get('width'),query.get('height'),yHigh-yLow]);
                    if(prefetchAttempts.has(key))continue;
                    prefetchAttempts.add(key);if(prefetchAttempts.size>48)prefetchAttempts.delete(prefetchAttempts.values().next().value);
                    prefetchRequest=new AbortController();const signal=prefetchRequest.signal;
                    ++fetched;
                    const next=new URLSearchParams(query);next.set('min',String(a));next.set('max',String(b));
                    const targetSpan=nextZoom?span/2:span;
                    next.set('width',String(Math.max(320,Math.min(2048,Math.round(Number(query.get('width'))*(b-a)/targetSpan)))));
                    next.set('ymin',String(yLow));next.set('ymax',String(yHigh));
                    try {
                        const entry=await fetchPlot(next,expression,signal,true);
                        if(disposed||token!==prefetchToken||cacheIdentity!==identity){prefetchAttempts.delete(key);return;}
                        // Speculative detail cannot evict the current prepared view.
                        const retained=cache.filter(e=>e.low<high&&e.high>low);
                        if(retained.reduce((n,e)=>n+e.points.length+e.bands.length,0)+entry.points.length+entry.bands.length>maxCachePoints||retained.reduce((n,e)=>n+e.cacheBytes,0)+entry.cacheBytes>maxCacheBytes)return;
                        remember(entry);
                        if(entry.limited||entry.cost>1500||!entry.points.some(p=>p.plottable))return;
                    } catch(_){if(signal.aborted)prefetchAttempts.delete(key);return;}
                    finally {if(token===prefetchToken)prefetchRequest=null;}
                }
                }
                prefetchCamera=camera;
            },180);
        }
        async function plot(event,keepView=false) {
            event?.preventDefault();
            if(!visible)return;
            stopPrefetch();
            ++generation;request?.abort();request=null;busy=false;syncStatus();
            if(!keepView){samples=null;bands=null;chunks=null;pendingView=false;drawSoon();}
            const id=generation,expression=input.value;
            if(!expression.trim()){pendingView=false;draw();message(text.ready);return;}
            if(!input.reportValidity())return;
            request=new AbortController();const signal=request.signal;
            busy=true;message(text.busy);syncStatus();
            try {
                const [low,high]=bounds(),[yLow,yHigh]=yViewport;
                // The C screen sampler selects density from the camera and falls
                // back to the precision evaluator when fast arithmetic is unsuitable.
                const query=new URLSearchParams({min:String(low),max:String(high),render:'auto',angle:'rad',ymin:String(yLow),ymax:String(yHigh)});
                const {width,height}=dimensions();
                query.set('mode','fast');query.set('width',String(Math.round(width)));query.set('height',String(height));query.set('quality','refined');query.set('interpolation','hermite-subpixel');query.set('dense','band');
                if(id!==generation||disposed)return;
                const identity=identityKey();
                if(cacheIdentity!==identity){clearCache();cacheIdentity=identity;samples=null;bands=null;chunks=null;draw();message(text.busy);}
                if(low!==null&&high!==null&&high>low&&Number.isFinite(high-low))viewport=[low,high];
                if(low!==null&&high!==null&&showCached(low,high)) {
                    const fast=cache.find(e=>!e.discrete&&!e.limited&&e.cost<=1500&&e.points.some(p=>p.plottable));
                    if(fast)prefetch(query,expression,identity,low,high);return;
                }
                let jobs=low!==null&&high!==null&&cacheMissing.length&&cacheMissing.length<=4?cacheMissing.map(r=>r.slice()):[[low,high]];
                // A dense visible tile plus another full-size tile can exceed the
                // cache point bound. Replace the viewport as one bounded result.
                if(low!==null&&high!==null&&cache.filter(e=>e.low<high&&e.high>low).reduce((n,e)=>n+e.points.length+e.bands.length,0)>maxCachePoints-maxPoints)jobs=[[low,high]];
                // Foreground jobs cover the visible camera only. Optional wide
                // coverage is prepared after all visible functions have completed.
                let fast=true;
                for(const [a,b] of jobs) {
                    const next=new URLSearchParams(query);
                    if(a!==null&&b!==null&&(a!==low||b!==high)){next.set('min',String(a));next.set('max',String(b));}
                    if(a!==null&&b!==null&&low!==null&&high!==null)next.set('width',String(Math.max(320,Math.min(2048,Math.round(width*(b-a)/(high-low))))));
                    let entry;
                    // Accepted cached geometry already supplies the immediate
                    // preview. Request coarse data only for missing/gapped views.
                    if(keepView&&(!cacheCovered||visibleUnresolved()||!hasVisibleStroke(curve.state()))){
                        const preview=new URLSearchParams(next);preview.set('quality','coarse');
                        const draft=await fetchPlot(preview,expression,signal);if(id!==generation||disposed)return;
                        if(remember(draft)&&low!==null&&high!==null)showCached(low,high);
                        await new Promise(resolve=>requestAnimationFrame(resolve));if(id!==generation||disposed)return;
                        if(draft.quality==='refined')entry=draft;
                    }
                    if(!entry)entry=await fetchPlot(next,expression,signal);if(id!==generation||disposed)return;
                    fast=fast&&!entry.discrete&&!entry.limited&&entry.cost<=1500&&entry.points.some(p=>p.plottable);
                    if(remember(entry)&&low!==null&&high!==null)showCached(low,high);
                    else {discrete=entry.discrete;pointStride=entry.pointStride;parameter=entry.parameter;limited=entry.limited;approximate=entry.approximate;pendingView=false;samples=entry.points;bands=entry.bands;chunks=null;draw();fast=false;}
                }
                // Partial results can evict an earlier visible tile under the
                // bounded cache. Reconcile once with a complete foreground
                // result, rather than leaving a hole or retrying indefinitely.
                if(low!==null&&high!==null&&pendingView){
                    const entry=await fetchPlot(query,expression,signal);if(id!==generation||disposed)return;
                    fast=fast&&!entry.limited&&entry.cost<=1500&&entry.points.some(p=>p.plottable);
                    if(!remember(entry)||!showCached(low,high))throw Error(text.invalid);
                }
                if(low!==null&&high!==null&&fast)prefetch(query,expression,identity,low,high);
            } catch(error) {
                if(id===generation && !disposed && error.name!=='AbortError'){pendingView=false;message(error instanceof TypeError?text.network:error.message,true);}
            } finally {if(id===generation && !disposed){busy=false;request=null;if(samples&&!pendingView&&!lastError)curve.describe();syncStatus();drawSoon();save();}}
        }

        const curve={id,row,input,color,plot,message,visibleUnresolved,
            async originValue(signal){
                if(cacheIdentity===identityKey()){for(const entry of cache){const point=entry.points.find(p=>p.nx===0&&p.plottable);if(point)return point.ny;}}
                // Use the same read-only C graph API for variable inference and
                // domain checks. The boundary x=0 is an actual sample, never an
                // extrapolation from cached strokes outside the visible camera.
                const query=new URLSearchParams({min:'0',max:'1',angle:'rad',mode:'fast',width:'320',height:'160',quality:'coarse',ymin:'-10',ymax:'10'});
                const entry=await fetchPlot(query,input.value,signal),point=entry.points.find(p=>p.nx===0&&p.plottable);
                return point?.ny??null;
            },
            state:()=>({visible,samples,bands,chunks,parameter,limited,pendingView,approximate,busy,discrete,pointStride,message:lastMessage,error:lastError,notice:lastNotice}),
            hasResult:()=>!!samples&&!busy&&!pendingView&&!lastError&&cacheIdentity===identityKey(),
            retain(){return curve.hasResult()?{expression:input.value,render:'auto',samples,bands,chunks,cache,cacheIdentity,parameter,limited,approximate,hiddenReady,hiddenCamera,discrete,pointStride}:null;},
            restore(entry){if(!entry||entry.expression!==input.value||entry.render!=='auto')return false;({samples,bands,chunks,cache,cacheIdentity,parameter,limited,approximate,hiddenReady,hiddenCamera,discrete,pointStride}=entry);return true;},
            describe(){
                const state=curve.state(),[low,high]=bounds(),points=(chunks||[{points:samples}]).flatMap(c=>c.points.filter(p=>p.nx>=Math.max(low,c.low??low)&&p.nx<=Math.min(high,c.high??high)));
                let notice='',detail='';
                if(discrete&&pointStride>1){notice='point-density';detail=english?`Showing every ${pointStride}th integer x. Zoom in for all points.`:`Zobrazené každé ${pointStride}. celé x. Priblíž graf pre všetky body.`;}
                else if(discrete&&!points.length){notice='no-integers';detail=english?'No integer x values in this view.':'V tomto pohľade nie sú celé hodnoty x.';}
                else if(visibleUnresolved()){notice='limited';detail=text.limited;}
                else if(visibleBands(state).length){notice='dense';detail=text.dense;}
                else if(!hasVisibleStroke(state)){
                    if(points.some(p=>p.plottable)){const inside=points.some(p=>p.plottable&&p.ny>=yViewport[0]&&p.ny<=yViewport[1]);notice=inside?'disconnected':'outside';detail=inside?text.noStroke:text.outside;}
                    else if(points.some(p=>p.kind==='real')){notice='numeric';detail=text.numeric;}
                    else if(points.some(p=>p.kind==='unresolved')){notice='limited';detail=text.limited;}
                    else if(points.some(p=>p.kind==='quantity')){notice='quantity';detail=english?'Values with units cannot be plotted on these axes.':'Hodnoty s jednotkami sa na tomto grafe nezobrazujú.';}
                    else{notice='no-real';detail=text.noReal;}
                }else if(points.some(p=>['complex','domain_error','quantity'].includes(p.kind))){notice='domain';detail=text.domain;}
                message((approximate?'≈ ':'')+`y = f(${parameter})`+(detail?' · '+detail:''),false,notice);
            },
            needsVerticalView(){return visible&&(busy||pendingView||!!samples&&!showCached(...bounds(),false));},
            invalidData(){samples=null;bands=null;chunks=null;pendingView=false;message(text.range,true);},
            cameraChanged(){if(!visible)return;++generation;request?.abort();request=null;busy=false;stopPrefetch();pendingView=!!input.value.trim();const hit=showCached(...bounds(),false);if(!hit)message(input.value.trim()?text.busy:text.ready);syncStatus();},
            setVisible(value){
                if(visible===value)return;
                locateRequest?.abort();
                if(!value){hiddenCamera=cameraKey();hiddenReady=!!samples&&!pendingView;}
                visible=value;++generation;request?.abort();request=null;busy=false;stopPrefetch();
                const retained=visible&&hiddenReady&&!!samples&&hiddenCamera===cameraKey();
                pendingView=visible&&!!input.value.trim()&&!retained&&!showCached(...bounds(),false);
                relabel();save();drawSoon();if(pendingView)scheduleView();
            },
            dispose(){disposed=true;invalidate();controller.abort();}
        };
        input.addEventListener('input',()=>{locateRequest?.abort();clearTimeout(viewportTimer);viewportTimer=null;if(curves.length===1){viewport=null;yViewport=[-10,10];}invalidate();save();renderLegend();},{signal:controller.signal});

        row.addEventListener('focusin',()=>{activeCurve=curve;highlightActive();},{signal:controller.signal});
        color.addEventListener('input',()=>{row.querySelector('.graph-color-swatch').style.setProperty('--swatch',color.value);save();renderLegend();drawSoon();},{signal:controller.signal});
        row.querySelector('.graph-color-swatch').style.setProperty('--swatch',color.value);
        row.querySelector('.graph-color-swatch').addEventListener('click',()=>openColor(curve),{signal:controller.signal});
        row.querySelector('.graph-edit').addEventListener('click',()=>{activeCurve=curve;highlightActive();$('graph-library').open=true;},{signal:controller.signal});
        row.querySelector('.graph-visibility').addEventListener('click',()=>curve.setVisible(!visible),{signal:controller.signal});
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
        locateRequest?.abort();
        if(!Number.isFinite(low)||!Number.isFinite(high)||high<=low||!Number.isFinite(high-low))return;
        if(!Number.isFinite(yLow)||!Number.isFinite(yHigh)||yHigh<=yLow||!Number.isFinite(yHigh-yLow))return;
        // Keep ample coordinate precision for pixel positions, not just endpoints.
        if(high-low<Math.max(1,Math.abs(low),Math.abs(high))*1e-8||yHigh-yLow<Math.max(1,Math.abs(yLow),Math.abs(yHigh))*1e-8){message(text.range,true);return;}
        const old=bounds(),verticalOnly=(yLow!==yViewport[0]||yHigh!==yViewport[1])&&low===old[0]&&high===old[1]&&Math.abs((yHigh-yLow)-(yViewport[1]-yViewport[0]))<=(yHigh-yLow)*1e-12;
        verificationRequest?.abort();clearTimeout(verificationTimer);clearInspection();yViewport=[yLow,yHigh];
        if(verticalOnly){const changed=curves.filter(c=>c.needsVerticalView());if(changed.length){changed.forEach(c=>c.cameraChanged());scheduleView();}drawSoon();save();return;}
        viewport=[low,high];
        curves.forEach(curve=>curve.cameraChanged());drawSoon();save();scheduleView();
    }
    function zoom(factor,anchor=.5,yAnchor=.5) {
        const [low,high]=bounds(),span=high-low,center=low+span*anchor;
        const ySpan=yViewport[1]-yViewport[0],yCenter=yViewport[0]+ySpan*yAnchor;
        moveView(center-span*factor*anchor,center+span*factor*(1-anchor),yCenter-ySpan*factor*yAnchor,yCenter+ySpan*factor*(1-yAnchor));
    }
    $('graph-reset').addEventListener('click',()=>moveView(-10,10,-10,10),{signal:lifecycle.signal});
    $('graph-locate').addEventListener('click',async()=>{
        const curve=activeCurve?.state().visible&&activeCurve.input.value.trim()?activeCurve:curves.find(c=>c.state().visible&&c.input.value.trim());
        if(!curve){message(text.ready);return;}
        locateRequest?.abort();const request=locateRequest=new AbortController(),button=$('graph-locate');button.disabled=true;button.setAttribute('aria-busy','true');
        const expression=curve.input.value;message(text.busy);
        try{
            const y=await curve.originValue(request.signal);
            if(disposed||request.signal.aborted||curve.input.value!==expression||!curves.includes(curve)||!curve.state().visible)return;
            if(y===null){message(english?'This function has no real, displayable value at x = 0.':'Táto funkcia nemá pri x = 0 reálnu zobraziteľnú hodnotu.',true);return;}
            const radius=Math.max(10,Math.abs(y)*1e-6);
            if(!Number.isFinite(y-radius)||!Number.isFinite(y+radius)){message(text.range,true);return;}
            moveView(-10,10,y-radius,y+radius);
        }catch(error){if(!disposed&&error.name!=='AbortError')message(error instanceof TypeError?text.network:error.message,true);}
        finally{if(locateRequest===request)locateRequest=null;button.disabled=false;button.removeAttribute('aria-busy');}
    },{signal:lifecycle.signal});
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
        const selected=event.pointerType!=='mouse'&&!pointers.size?inspectionPoint(event.clientX,event.clientY,true):null;
        pointers.set(event.pointerId,{x:event.clientX,y:event.clientY});svg.setPointerCapture(event.pointerId);
        if(selected){inspectPointer=event.pointerId;inspection={kind:'pinned',curveId:selected.curve.id,x:selected.x,featureKey:selected.key};updateInspection();gesture=null;return;}
        inspectPointer=null;clearInspection();startGesture();
    },{signal:lifecycle.signal});
    svg.addEventListener('pointermove',event=>{
        if(!pointers.has(event.pointerId)){
            if(event.pointerType==='mouse'||event.pointerType==='pen'){inspection={kind:'hover',clientX:event.clientX,clientY:event.clientY};inspectSoon();}
            return;
        }
        pointers.set(event.pointerId,{x:event.clientX,y:event.clientY});
        if(inspectPointer===event.pointerId){
            const matrix=svg.getScreenCTM();
            if(matrix&&projection&&inspection){const feature=inspectionPoint(event.clientX,event.clientY,true);inspection.featureKey=feature?.key;if(feature?.key)inspection.x=feature.x;else{const point=new DOMPoint(event.clientX,event.clientY).matrixTransform(matrix.inverse());inspection.x=(projection.xLow+(projection.xHigh-projection.xLow)*Math.max(0,Math.min(1,(point.x-64)/(projection.right-64))))*projection.xScale;}}
            inspectSoon();return;
        }
        if(!gesture)return;
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
    svg.addEventListener('pointerleave',event=>{if(!pointers.size&&event.pointerType==='mouse')clearInspection();},{signal:lifecycle.signal});
    function endGesture(event) {
        if(!pointers.has(event.pointerId))return;
        const inspected=inspectPointer===event.pointerId;
        if(inspected){inspectPointer=null;if(event.type!=='pointerup')clearInspection();}
        pointers.delete(event.pointerId);startGesture();
        if(inspected)return;
        if(!pointers.size&&curves.some(c=>c.state().pendingView))scheduleView();
    }
    for(const name of ['pointerup','pointercancel','lostpointercapture'])svg.addEventListener(name,endGesture,{signal:lifecycle.signal});
    svg.addEventListener('keydown',event=>{
        if(event.key==='Escape'){clearInspection();return;}
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
        const legend=$('graph-legend'),focused=legend.contains(document.activeElement)?document.activeElement.dataset.curve:null;legend.replaceChildren();legend.hidden=!curves.some(c=>c.input.value.trim());
        curves.forEach((c,i)=>{const item=document.createElement('li'),button=document.createElement('button'),dot=document.createElement('span'),label=document.createElement('span');button.type='button';button.className='graph-legend-toggle';button.setAttribute('aria-pressed',String(c.state().visible));button.setAttribute('aria-label',`${c.state().visible?text.hide:text.show} ${i+1}`);button.title=`${c.state().visible?text.hide:text.show} ${i+1}`;button.dataset.curve=String(c.id);dot.className='graph-legend-dot';dot.style.backgroundColor=c.color.value;dot.setAttribute('aria-hidden','true');label.textContent=`${i+1}. ${c.input.value||'…'}`;label.title=c.input.value;button.append(dot,label);item.append(button);legend.append(item);});
        if(focused)legend.querySelector(`button[data-curve="${focused}"]`)?.focus({preventScroll:true});
    }
    function relabel(){
        curves.forEach((c,i)=>{c.row.querySelector('.graph-expression span').textContent=`${text.function} ${i+1} · y =`;c.input.id=i?'graph-expression-'+c.id:'graph-expression';c.color.id=i?'graph-color-'+c.id:'graph-color';c.row.querySelector('.graph-color-option span').textContent=`${text.color} ${i+1}`;c.row.querySelector('.graph-edit').setAttribute('aria-label',`${english?'Edit function':'Upraviť funkciu'} ${i+1}`);c.row.querySelector('.graph-color-swatch').setAttribute('aria-label',`${text.color} ${i+1}`);const toggle=c.row.querySelector('.graph-visibility'),visible=c.state().visible;toggle.setAttribute('aria-pressed',String(visible));toggle.setAttribute('aria-label',`${visible?text.hide:text.show} ${i+1}`);toggle.title=`${visible?text.hide:text.show} ${i+1}`;c.row.classList.toggle('curve-hidden',!visible);const remove=c.row.querySelector('.graph-remove');remove.hidden=curves.length===1;remove.setAttribute('aria-label',`${text.remove} ${i+1}`);});
        $('graph-add').textContent=(english?'+ Add function':'+ Pridať funkciu')+` · ${curves.length}/${maximumCurves}`;$('graph-add').disabled=curves.length>=maximumCurves;$('graph-add').title=english?'Up to four functions':'Najviac štyri funkcie';
        highlightActive();renderLegend();syncStatus();
    }
    function addCurve(expression='',color=null,focus=true,visible=true){
        if(curves.length>=maximumCurves)return;
        const row=curves[0].row.cloneNode(true);row.querySelector('.graph-curve-status').hidden=true;row.querySelector('.graph-visibility').setAttribute('aria-pressed',String(visible));
        row.querySelector('input:not([type=color])').value=expression;
        row.querySelector('input[type=color]').value=color||palette.find(p=>!curves.some(c=>c.color.value===p))||palette[curves.length];
        $('graph-curves').append(row);const curve=createCurve(row);curves.push(curve);activeCurve=curve;relabel();save();if(focus)curve.input.focus();drawSoon();
    }
    function removeCurve(curve){
        if(curves.length===1)return;const index=curves.indexOf(curve);if(index<0)return;
        locateRequest?.abort();
        curve.dispose();curves.splice(index,1);curve.row.remove();activeCurve=curves[Math.min(index,curves.length-1)];relabel();save();drawSoon();activeCurve.input.focus();
    }
    const initialRow=root.querySelector('.graph-curve');let restored;
    try{restored=JSON.parse(sessionStorage.getItem('numforge-graph-state')||'null');}catch(_){}
    const valid=entry=>entry&&typeof entry.expression==='string'&&entry.expression.length<=4096;
    const restoredCurves=Array.isArray(restored?.curves)?restored.curves.filter(valid).slice(0,maximumCurves):valid(restored)?[restored]:[];
    const validAxis=a=>Array.isArray(a)&&a.length===2&&a.every(Number.isFinite)&&a[1]>a[0]&&Number.isFinite(a[1]-a[0])&&a[1]-a[0]>=Math.max(1,...a.map(Math.abs))*1e-8;
    if(restored?.version===2&&validAxis(restored.view?.x)&&validAxis(restored.view?.y)){viewport=restored.view.x.slice();yViewport=restored.view.y.slice();}
    if(restoredCurves.length){initialRow.querySelector('input:not([type=color])').value=restoredCurves[0].expression;if(/^#[0-9a-f]{6}$/i.test(restoredCurves[0].color))initialRow.querySelector('input[type=color]').value=restoredCurves[0].color;}

    if(restoredCurves.length)initialRow.querySelector('.graph-visibility').setAttribute('aria-pressed',String(restoredCurves[0].visible!==false));
    curves.push(createCurve(initialRow));activeCurve=curves[0];
    for(const entry of restoredCurves.slice(1))addCurve(entry.expression,/^#[0-9a-f]{6}$/i.test(entry.color)?entry.color:null,false,entry.visible!==false);
    activeCurve=curves[0];relabel();
    $('graph-form').addEventListener('submit',event=>plotAll(event),{signal:lifecycle.signal});
    $('graph-add').addEventListener('click',()=>addCurve(),{signal:lifecycle.signal});
    $('graph-legend').addEventListener('click',event=>{const button=event.target.closest('.graph-legend-toggle'),curve=curves.find(c=>String(c.id)===button?.dataset.curve);if(curve)curve.setVisible(!curve.state().visible);},{signal:lifecycle.signal});
    window.addEventListener('numforge:navigate',save,{signal:lifecycle.signal});

    // Keep the native color value as the curve model; the palette is shared by all rows.
    const colorDialog=$('graph-color-dialog'),customColor=$('graph-custom-color');let colorCurve=null;
    function applyColor(value){if(!colorCurve||!curves.includes(colorCurve))return;colorCurve.color.value=value;colorCurve.color.dispatchEvent(new Event('input',{bubbles:true}));customColor.value=value;for(const b of $('graph-color-palette').children)b.setAttribute('aria-pressed',String(b.dataset.color===value));}
    function openColor(curve){colorCurve=curve;$('graph-color-title').textContent=`${text.color} ${curves.indexOf(curve)+1}`;applyColor(curve.color.value);colorDialog.showModal();}
    for(const value of [...palette,'#ff6b88','#ffef85','#ffffff','#a0a9ff']){const button=document.createElement('button');button.type='button';button.dataset.color=value;button.style.setProperty('--swatch',value);button.setAttribute('aria-label',(english?['Purple','Mint','Orange','Blue','Rose','Yellow','White','Indigo']:['Fialová','Mätová','Oranžová','Modrá','Ružová','Žltá','Biela','Indigová'])[$('graph-color-palette').childElementCount]);button.addEventListener('click',()=>applyColor(value),{signal:lifecycle.signal});$('graph-color-palette').append(button);}
    customColor.addEventListener('input',()=>applyColor(customColor.value),{signal:lifecycle.signal});
    $('graph-color-close').addEventListener('click',()=>colorDialog.close(),{signal:lifecycle.signal});
    colorDialog.addEventListener('close',()=>{if(!disposed&&curves.includes(colorCurve))colorCurve.row.querySelector('.graph-color-swatch').focus({preventScroll:true});colorCurve=null;},{signal:lifecycle.signal});

    // Move the same canvas into a native modal; keep its camera and curve state.
    const result=root.querySelector('.graph-result'),home=document.createElement('div'),dialog=$('graph-expanded'),expand=$('graph-expand');home.hidden=true;result.before(home);let pagePosition=null;
    const toolsToggle=$('graph-tools-toggle');
    function closeTools(){result.classList.remove('graph-tools-open');toolsToggle.setAttribute('aria-expanded','false');}
    toolsToggle.addEventListener('click',()=>{const open=result.classList.toggle('graph-tools-open');toolsToggle.setAttribute('aria-expanded',String(open));},{signal:lifecycle.signal});
    $('graph-expanded-back').addEventListener('click',()=>leaveExpanded(),{signal:lifecycle.signal});
    svg.addEventListener('pointerdown',closeTools,{signal:lifecycle.signal});
    function leaveExpanded(focus=true){
        closeTools();
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

    const libraryDetails=$('graph-library'),libraryDialog=$('graph-library-dialog'),libraryExpression=$('graph-library-expression');let libraryCurve=null;
    function syncLibraryExpression(){if(!libraryCurve)return;const input=libraryCurve.input,changed=input.value!==libraryExpression.value;input.value=libraryExpression.value;input.setSelectionRange(libraryExpression.selectionStart,libraryExpression.selectionEnd);if(changed)input.dispatchEvent(new Event('input',{bubbles:true}));}
    libraryDetails.addEventListener('toggle',()=>{
        if(disposed)return;
        if(libraryDetails.open&&!libraryDialog.open){libraryCurve=activeCurve||curves[0];libraryExpression.value=libraryCurve.input.value;$('graph-library-target').textContent=`${text.function} ${curves.indexOf(libraryCurve)+1} · y =`;libraryExpression.setSelectionRange(libraryCurve.input.selectionStart,libraryCurve.input.selectionEnd);libraryDialog.showModal();document.body.classList.add('graph-library-open');libraryExpression.focus({preventScroll:true});}
        else if(!libraryDetails.open&&libraryDialog.open)libraryDialog.close();
    },{signal:lifecycle.signal});
    $('graph-library-close').addEventListener('click',()=>libraryDialog.close(),{signal:lifecycle.signal});
    libraryDialog.addEventListener('close',()=>{if(!disposed)syncLibraryExpression();libraryDetails.open=false;document.body.classList.remove('graph-library-open');if(!disposed)libraryCurve?.input.focus({preventScroll:true});},{signal:lifecycle.signal});
    libraryExpression.addEventListener('input',syncLibraryExpression,{signal:lifecycle.signal});
    function plotFromLibrary(){syncLibraryExpression();libraryDialog.close();plotAll(null,true);}
    $('graph-library-plot').addEventListener('click',plotFromLibrary,{signal:lifecycle.signal});
    $('graph-library-clear').addEventListener('click',()=>{libraryExpression.value='';libraryExpression.setSelectionRange(0,0);syncLibraryExpression();libraryExpression.focus({preventScroll:true});},{signal:lifecycle.signal});
    libraryExpression.addEventListener('keydown',event=>{if(event.key==='Enter'){event.preventDefault();plotFromLibrary();}},{signal:lifecycle.signal});

    function insertLibrary(value,isFunction=false,name=''){
        const curve=libraryDialog.open?libraryCurve:activeCurve||curves[0],input=libraryDialog.open?libraryExpression:curve.input,start=input.selectionStart??input.value.length,end=input.selectionEnd??start;
        const selected=input.value.slice(start,end),arity=registry.get(name)?.minimum_arguments||1;
        const separators=';'.repeat(Math.max(0,arity-1));
        const insertion=isFunction?value+selected+separators+')':value;
        if(input.value.length-(end-start)+insertion.length>4096){curve.message(english?'The expression is too long.':'Zadanie je príliš dlhé.',true);return;}
        input.setRangeText(insertion,start,end,'end');input.dispatchEvent(new Event('input',{bubbles:true}));input.focus({preventScroll:true});
        if(isFunction){const caret=selected?(arity>1?start+value.length+selected.length+1:start+insertion.length):start+value.length;input.setSelectionRange(caret,caret);}
        if(libraryDialog.open)syncLibraryExpression();
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
            const categoryButtons=[...selector.options].map(option=>{
                const button=document.createElement('button');button.type='button';button.textContent=option.textContent;button.dataset.category=option.value;
                button.addEventListener('click',()=>{selector.value=option.value;search.value='';filter();},{signal:lifecycle.signal});
                $('graph-function-categories').append(button);return button;
            });
            let page=0;
            function filter(reset=true){if(reset)page=0;const query=library.normalize(search.value.trim()),category=selector.value;
                categoryButtons.forEach(button=>button.setAttribute('aria-pressed',String(button.dataset.category===category)));
                const matches=buttons.filter(({searchable,category:group})=>(!category||String(group)===category)&&searchable.includes(query));
                const perPage=innerHeight<=460?3:innerHeight<=760?6:9,pages=Math.max(1,Math.ceil(matches.length/perPage));page=Math.min(page,pages-1);
                const shown=new Set(matches.slice(page*perPage,(page+1)*perPage));
                buttons.forEach(entry=>{entry.button.hidden=!shown.has(entry);});
                searchStatus.hidden=!!matches.length;searchStatus.textContent=text.noMatch;
                $('graph-library-page').textContent=`${page+1} / ${pages}`;
                $('graph-library-previous').disabled=page===0;$('graph-library-next').disabled=page===pages-1;
            }
            search.addEventListener('input',()=>{if(search.value.trim())selector.value='';filter();},{signal:lifecycle.signal});selector.addEventListener('change',()=>filter(),{signal:lifecycle.signal});
            $('graph-library-previous').addEventListener('click',()=>{--page;filter(false);},{signal:lifecycle.signal});
            $('graph-library-next').addEventListener('click',()=>{++page;filter(false);},{signal:lifecycle.signal});
            window.addEventListener('resize',()=>filter(false),{signal:lifecycle.signal});
            search.addEventListener('keydown',event=>{if(event.key==='Escape'){search.value='';filter();}},{signal:lifecycle.signal});filter();
        }catch(error){if(!disposed&&error.name!=='AbortError'){$('search-status').hidden=false;$('search-status').textContent=text.unavailable;}}
    }
    const sameSession=retained&&JSON.stringify(restored)===retained.key;
    const restoredResults=curves.map((c,i)=>sameSession&&c.restore(retained.curves[i]));
    restoring=false;
    curves.forEach((c,i)=>{if(restoredResults[i])c.describe();});
    loadLibrary();draw();
    const restoredSize=dimensions();
    if(sameSession&&retained.size&&(retained.size.width!==restoredSize.width||retained.size.height!==restoredSize.height)&&restoredResults.some(Boolean)){
        curves.forEach(c=>c.cameraChanged());scheduleView();
    }
    // Reloads have settings but no in-memory samples. Recalculate only curves
    // that had a completed plot; unfinished/edited input stays unsubmitted.
    const resume=curves.filter((c,i)=>!restoredResults[i]&&restored?.version===2&&restoredCurves[i]?.plotted===true&&c.state().visible);
    if(resume.length){++foregroundBatches;Promise.all(resume.map(c=>c.plot(null,true))).finally(()=>{--foregroundBatches;pump();save();});}
    else save();
    return ()=>{save();initGraph.retained={key:JSON.stringify(settings()),size:dimensions(),curves:curves.map(c=>c.retain()),verifications:verificationCache};disposed=true;leaveExpanded(false);colorDialog.close();libraryDialog.close();document.body.classList.remove('graph-library-open');curves.forEach(c=>c.dispose());clearTimeout(viewportTimer);if(drawFrame!==null)cancelAnimationFrame(drawFrame);if(inspectionFrame!==null)cancelAnimationFrame(inspectionFrame);clearInspection();inspectionLayer.remove();pointers.clear();resize.disconnect();lifecycle.abort();};

}
window.numforgeInitGraph=initGraph;
window.numforgeDisposeGraph=initGraph();
