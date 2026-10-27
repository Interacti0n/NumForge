/* File formats and image capture stay independent of the graph sampler. */
window.numforgeGraphFiles=(()=>{
    const namespace='http://www.w3.org/2000/svg', maximumFileBytes=100000, maximumImageBytes=8*1024*1024;
    const validAxis=a=>Array.isArray(a)&&a.length===2&&a.every(Number.isFinite)&&a[1]>a[0]&&Number.isFinite(a[1]-a[0])&&a[1]-a[0]>=Math.max(1,...a.map(Math.abs))*1e-8;
    const validExpression=e=>typeof e==='string'&&e.length<=4096&&!/[\u0000-\u001f\u007f]/.test(e)&&new TextEncoder().encode(e).length<=4096;
    const valid=s=>s&&validAxis(s.view?.x)&&validAxis(s.view?.y)&&Array.isArray(s.curves)&&s.curves.length>=1&&s.curves.length<=4&&
        s.curves.every(c=>c&&validExpression(c.expression)&&typeof c.color==='string'&&/^#[0-9a-f]{6}$/i.test(c.color)&&typeof c.visible==='boolean');
    const clean=s=>({view:{x:s.view.x.slice(),y:s.view.y.slice()},curves:s.curves.map(c=>({expression:c.expression,color:c.color,visible:c.visible}))});
    function workspace(settings){if(!valid(settings))throw Error('Invalid workspace');return new Blob([JSON.stringify({format:'numforge.graph-workspace',version:1,...clean(settings)},null,2)],{type:'application/json'});}
    async function read(file){
        if(file.size>maximumFileBytes)throw Error('Workspace too large');
        const data=JSON.parse(await file.text());
        if(data?.format!=='numforge.graph-workspace'||data.version!==1||!valid(data))throw Error('Invalid workspace');
        return {version:2,...clean(data)};
    }
    function download(blob,filename){
        if(blob.size>(filename.endsWith('.json')?maximumFileBytes:maximumImageBytes))throw Error('File too large');
        const url=URL.createObjectURL(blob),link=document.createElement('a');link.href=url;link.download=filename;link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
    }
    function capture(svg,settings,english,note=''){
        if(!valid(settings))throw Error('Invalid workspace');
        const box=svg.viewBox.baseVal,width=box.width,height=box.height;
        if(!Number.isFinite(width)||!Number.isFinite(height)||width<1||height<1||width>2048||height>1200)throw Error('Invalid image size');
        const source=[svg,...svg.querySelectorAll('*')];if(source.length>12000)throw Error('Image too complex');
        const clone=svg.cloneNode(true),copies=[clone,...clone.querySelectorAll('*')];
        const properties=['fill','stroke','stroke-width','stroke-opacity','fill-opacity','stroke-dasharray','stroke-linecap','stroke-linejoin','opacity','visibility','display','r','font-family','font-size','font-weight','text-anchor'];
        source.forEach((node,i)=>{
            const style=getComputedStyle(node);
            copies[i].removeAttribute('style');
            for(const property of properties){const value=style.getPropertyValue(property);if(value&&!value.includes('url('))copies[i].style.setProperty(property,value);}
            for(const attribute of [...copies[i].attributes])if(/^on/i.test(attribute.name)||['href','xlink:href','tabindex','aria-describedby'].includes(attribute.name))copies[i].removeAttribute(attribute.name);
        });
        clone.querySelectorAll('.graph-inspection,script,foreignObject,image,use,a,style,animate,animateTransform,set').forEach(n=>n.remove());
        const entries=settings.curves.filter(c=>c.visible&&c.expression.trim()),header=48+entries.length*20,totalHeight=header+height+32;
        const create=(tag,attrs={},text)=>{const node=document.createElementNS(namespace,tag);for(const [key,value]of Object.entries(attrs))node.setAttribute(key,String(value));if(text!==undefined)node.textContent=text;return node;};
        const result=create('svg',{width,height:totalHeight,viewBox:`0 0 ${width} ${totalHeight}`,role:'img'});
        result.append(create('title',{},english?'NumForge function graph':'NumForge · Graf funkcií'),create('desc',{},entries.map(c=>c.expression).join('; ')+(note?' — '+note:'')));
        result.append(create('rect',{width,height:totalHeight,fill:getComputedStyle(svg).backgroundColor}));
        result.append(create('text',{x:20,y:27,fill:'#ecf3ff','font-family':'sans-serif','font-size':16,'font-weight':700},'NumForge'));
        entries.forEach((entry,i)=>{
            const limit=Math.max(12,Math.floor((width-65)/8)),label=entry.expression.length>limit?entry.expression.slice(0,limit-1)+'…':entry.expression;
            result.append(create('circle',{cx:24,cy:49+i*20,r:4,fill:entry.color}),create('text',{x:38,y:53+i*20,fill:'#bdcce2','font-family':'monospace','font-size':12},label));
        });
        clone.removeAttribute('id');clone.removeAttribute('hidden');clone.setAttribute('x','0');clone.setAttribute('y',String(header));clone.setAttribute('width',String(width));clone.setAttribute('height',String(height));clone.style.removeProperty('visibility');
        const footer=(english?'Approximate graph':'Približný graf')+' · '+(note||(english?'current view':'aktuálny výrez')),limit=Math.max(20,Math.floor((width-40)/6));
        result.append(clone,create('text',{x:20,y:totalHeight-10,fill:'#9eafca','font-family':'sans-serif','font-size':11},footer.length>limit?footer.slice(0,limit-1)+'…':footer));
        const blob=new Blob([new XMLSerializer().serializeToString(result)],{type:'image/svg+xml;charset=utf-8'});
        if(blob.size>maximumImageBytes)throw Error('Image too large');return {blob,width,height:totalHeight};
    }
    async function png(snapshot){
        const url=URL.createObjectURL(snapshot.blob);
        try {
            const image=new Image();image.src=url;await image.decode();
            const scale=Math.min(2,2048/snapshot.width,2048/snapshot.height),canvas=document.createElement('canvas');
            canvas.width=Math.min(2048,Math.ceil(snapshot.width*scale));canvas.height=Math.min(2048,Math.ceil(snapshot.height*scale));
            const context=canvas.getContext('2d');if(!context)throw Error('Canvas unavailable');context.drawImage(image,0,0,canvas.width,canvas.height);
            const blob=await new Promise(resolve=>canvas.toBlob(resolve,'image/png'));if(!blob)throw Error('PNG unavailable');return blob;
        }finally{URL.revokeObjectURL(url);}
    }
    return {valid,validAxis,validExpression,workspace,read,download,capture,png};
})();
