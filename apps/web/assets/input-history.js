/* Bounded, input-only recent tasks. Restoring never evaluates mathematics. */
window.numforgeInputHistory=({root,key,valid,current,restore,label,signal})=>{
    const list=root.querySelector('[data-history-list]'),clear=root.querySelector('[data-history-clear]'),empty=root.querySelector('[data-history-empty]');
    const english=document.documentElement.lang==='en',bytes=value=>new TextEncoder().encode(JSON.stringify(value)).length;
    let entries=[];
    try{const raw=sessionStorage.getItem(key);if(raw&&raw.length<=64000){const data=JSON.parse(raw);if(data?.version===1&&Array.isArray(data.items)&&bytes(data)<=64000)entries=data.items.filter(item=>valid(item)&&bytes(item)<=20000).slice(0,8);}}catch(_){}
    function refresh(){const active=JSON.stringify(current());list.querySelectorAll('[data-restore]').forEach(button=>{if(JSON.stringify(entries[button.dataset.restore])===active)button.setAttribute('aria-current','true');else button.removeAttribute('aria-current');});}
    function render(){
        list.replaceChildren();empty.hidden=!!entries.length;clear.disabled=!entries.length;
        entries.forEach((entry,index)=>{
            const row=document.createElement('li'),button=document.createElement('button'),title=document.createElement('strong'),detail=document.createElement('span'),remove=document.createElement('button'),text=label(entry);
            button.type='button';button.dataset.restore=index;button.className='workspace-history-restore';button.title=text.title;title.textContent=text.title;detail.textContent=text.detail;button.append(title,detail);
            remove.type='button';remove.dataset.remove=index;remove.className='workspace-history-remove';remove.textContent='×';remove.setAttribute('aria-label',(english?'Remove task: ':'Odstrániť zadanie: ')+text.title);
            row.append(button,remove);list.append(row);
        });refresh();
    }
    function persist(){try{sessionStorage.setItem(key,JSON.stringify({version:1,items:entries}));}catch(_){}render();}
    function remember(data){
        if(!valid(data)||bytes(data)>20000)return;
        const encoded=JSON.stringify(data);entries=[JSON.parse(encoded),...entries.filter(entry=>JSON.stringify(entry)!==encoded)].slice(0,8);
        while(entries.length&&bytes({version:1,items:entries})>64000)entries.pop();persist();
    }
    list.addEventListener('click',event=>{
        const button=event.target.closest('[data-restore]');if(button&&entries[button.dataset.restore]){restore(JSON.parse(JSON.stringify(entries[button.dataset.restore])));refresh();}
        const remove=event.target.closest('[data-remove]');if(remove){const index=Number(remove.dataset.remove);entries.splice(index,1);persist();(list.querySelectorAll('[data-remove]')[Math.min(index,entries.length-1)]||root).focus({preventScroll:true});}
    },{signal});
    clear.addEventListener('click',()=>{entries=[];persist();root.focus({preventScroll:true});},{signal});render();
    return {remember,refresh};
};
