/* Provider data is separate from fixed units. All amount arithmetic uses C. */
window.numforgeInitCurrency=function(root){
    'use strict';
    const $=id=>root.querySelector('#'+id),english=document.documentElement.lang==='en',life=new AbortController();
    const endpoint='https://api.frankfurter.dev/v2/providers/ecb/rates?base=EUR',cacheKey='numforge-ecb-rates-v1',hour=3600000;
    const text=english?{
        loading:'Checking reference rates…',ready:'Daily ECB reference rates',cached:'Saved rates · update unavailable',old:'Older reference rates',
        unavailable:'Rates are unavailable. Try Refresh rates.',invalid:'Enter a decimal amount (up to 200 digits; dot or comma).',
        failed:'Conversion failed. Try again.',busy:'Converting…',approx:'Indicative conversion · no bank fees or spreads included.',
        copied:'Number copied.',copyFailed:'Copy failed. Select the number and copy it.',date:'Rate date',checked:'Last checked',
        empty:'No matching currency.'
    }:{
        loading:'Overujem referenčné kurzy…',ready:'Denné referenčné kurzy ECB',cached:'Uložené kurzy · aktualizácia nedostupná',old:'Staršie referenčné kurzy',
        unavailable:'Kurzy nie sú dostupné. Skús Obnoviť kurzy.',invalid:'Zadaj desatinnú sumu (najviac 200 číslic; bodka alebo čiarka).',
        failed:'Prevod sa nepodaril. Skús znova.',busy:'Prevádzam…',approx:'Orientačný prevod · bez bankových poplatkov a kurzovej prirážky.',
        copied:'Číslo skopírované.',copyFailed:'Kopírovanie zlyhalo. Označ číslo a skopíruj ho.',date:'Dátum kurzu',checked:'Posledné overenie',
        empty:'Žiadna zodpovedajúca mena.'
    };
    const locale=english?'en':'sk',names=new Intl.DisplayNames([locale],{type:'currency'}),number=new Intl.NumberFormat(locale,{maximumFractionDigits:12});
    let disposed=false,active=false,snapshot=null,refresh=null,conversion=null,timer=null,cooldown=null,generation=0,copy='',failed=false,lastAttempt=0;
    const input=$('currency-amount'),from=$('currency-from'),to=$('currency-to'),places=$('currency-places'),result=$('currency-result');
    const tabs=[$('units-tab'),$('currencies-tab')],unitPanels=[$('unit-converter-panel'),$('unit-sidebar-panel')],panel=$('currency-panel');
    const dateValid=date=>typeof date==='string'&&/^\d{4}-\d{2}-\d{2}$/.test(date)&&Number.isFinite(Date.parse(date+'T00:00:00Z'))&&new Date(date+'T00:00:00Z').toISOString().slice(0,10)===date;
    const rateValid=rate=>typeof rate==='string'&&/^(?:0|[1-9]\d{0,11})(?:\.\d{1,12})?$/.test(rate)&&Number(rate)>0;
    function valid(data){return data?.provider==='ecb'&&dateValid(data.date)&&Date.parse(data.date+'T00:00:00Z')<=Date.now()&&Date.now()-Date.parse(data.date+'T00:00:00Z')<10*24*hour&&
        Number.isFinite(data.checkedAt)&&data.checkedAt<=Date.now()&&Date.now()-data.checkedAt<10*24*hour&&data.rates&&Object.keys(data.rates).length>=2&&Object.keys(data.rates).length<=101&&data.rates.EUR==='1'&&
        Object.entries(data.rates).every(([code,rate])=>/^[A-Z]{3}$/.test(code)&&rateValid(rate));}
    function decode(rows){
        if(!Array.isArray(rows)||!rows.length||rows.length>100)throw Error('Invalid rates');
        const rates={EUR:'1'},seen=new Set(),date=rows[0].date;
        for(const row of rows){
            if(row?.base!=='EUR'||row.date!==date||typeof row.quote!=='string'||!/^[A-Z]{3}$/.test(row.quote)||seen.has(row.quote)||typeof row.rate!=='number'||!Number.isFinite(row.rate)||(row.quote==='EUR'&&row.rate!==1))throw Error('Invalid rates');
            seen.add(row.quote);rates[row.quote]=String(row.rate);
        }
        const data={provider:'ecb',date,rates,checkedAt:Date.now()};if(!valid(data))throw Error('Invalid rates');return data;
    }
    let saved={};try{saved=JSON.parse(sessionStorage.getItem('numforge-currency-input')||'{}')||{};const cached=JSON.parse(sessionStorage.getItem(cacheKey)||'null');if(valid(cached))snapshot=cached;}catch(_){}
    input.value=typeof saved.amount==='string'&&saved.amount.length<=256?saved.amount:'100';
    if(['auto','2','4','8'].includes(saved.places))places.value=saved.places;
    function save(){try{sessionStorage.setItem('numforge-currency-input',JSON.stringify({amount:input.value,from:from.value||saved.from||'EUR',to:to.value||saved.to||'USD',places:places.value}));}catch(_) {}}
    function message(value,error=false){$('currency-status').textContent=value;$('currency-status').classList.toggle('error',error);}
    function invalidate(){++generation;conversion?.abort();conversion=null;copy='';result.textContent='';$('currency-result-code').textContent='';$('currency-used').textContent='';$('currency-copy').disabled=true;delete result.dataset.rawResult;result.removeAttribute('aria-label');$('currency-form').removeAttribute('aria-busy');}
    function sync(){
        const available=!!snapshot&&valid(snapshot);for(const id of ['currency-from','currency-to','currency-swap','currency-submit'])$(id).disabled=!available;
        $('currency-refresh').disabled=!!refresh||Date.now()-lastAttempt<60000;
        const old=available&&Date.now()-Date.parse(snapshot.date+'T00:00:00Z')>4*24*hour;
        $('currency-rate-status').textContent=refresh?text.loading:!available?text.unavailable:failed?text.cached:old?text.old:text.ready;
        $('currency-rate-status').classList.toggle('error',!refresh&&(!available||failed||old));
        $('currency-date').textContent=available?`${text.date}: ${new Intl.DateTimeFormat(locale,{dateStyle:'medium',timeZone:'UTC'}).format(new Date(snapshot.date+'T00:00:00Z'))}`:'';
        $('currency-checked').textContent=available?`${text.checked}: ${new Intl.DateTimeFormat(locale,{dateStyle:'short',timeStyle:'short'}).format(snapshot.checkedAt)}`:'';
    }
    function table(){
        const body=$('currency-rates');body.replaceChildren();const search=$('currency-search').value.trim().toLocaleLowerCase(locale);
        if(snapshot&&valid(snapshot))for(const code of Object.keys(snapshot.rates).sort()){
            const name=names.of(code)||code;if(search&&!`${code} ${name}`.toLocaleLowerCase(locale).includes(search))continue;
            const row=document.createElement('tr');for(const value of [code,name,number.format(Number(snapshot.rates[code]))]){const cell=document.createElement('td');cell.textContent=value;row.append(cell);}body.append(row);
        }
        $('currency-empty').hidden=body.childElementCount>0;$('currency-empty').textContent=text.empty;
    }
    function select(){
        const a=from.value||saved.from||'EUR',b=to.value||saved.to||'USD';
        for(const el of [from,to]){el.replaceChildren();for(const code of Object.keys(snapshot.rates).sort())el.add(new Option(`${code} · ${names.of(code)||code}`,code));}
        from.value=Object.hasOwn(snapshot.rates,a)?a:'EUR';to.value=Object.hasOwn(snapshot.rates,b)?b:from.value;
        table();sync();
    }
    async function calculate(){
        invalidate();const id=generation;save();
        if(!snapshot||!valid(snapshot)){sync();message(text.unavailable,true);return;}
        const amount=input.value.trim().replace(',','.');
        if(!/^[+-]?(?:\d{1,200}(?:\.\d{0,100})?|\.\d{1,100})$/.test(amount)||amount.length>256){message(text.invalid,true);return;}
        const a=from.value,b=to.value;if(!Object.hasOwn(snapshot.rates,a)||!Object.hasOwn(snapshot.rates,b)){message(text.unavailable,true);return;}
        const digits=places.value==='auto'?new Intl.NumberFormat(locale,{style:'currency',currency:b}).resolvedOptions().maximumFractionDigits:Number(places.value);
        const controller=new AbortController();conversion=controller;const timeout=setTimeout(()=>controller.abort(),12000);$('currency-form').setAttribute('aria-busy','true');message(text.busy);
        try{
            const response=await fetch(`/api/currency/convert?places=${digits}`,{method:'POST',body:[amount,snapshot.rates[a],snapshot.rates[b]].join('\n'),signal:controller.signal});
            const body=await response.json();if(disposed||id!==generation||!active)return;
            if(!response.ok||!body.ok||typeof body.result!=='string'||body.result.length>1024||!/^[-+]?\d+(?:\.\d+)?$/.test(body.result))throw Error('Invalid conversion');
            window.numforgeDisplay.render(result,body.result,'plain');$('currency-result-code').textContent=b;copy=body.result;$('currency-copy').disabled=false;
            message(text.approx);$('currency-used').textContent=[...new Set([a,b])].filter(code=>code!=='EUR').map(code=>`1 EUR = ${snapshot.rates[code]} ${code}`).join(' · ');
        }catch(error){if(!disposed&&id===generation&&active)message(text.failed,true);}
        finally{clearTimeout(timeout);if(conversion===controller){conversion=null;$('currency-form').removeAttribute('aria-busy');}}
    }
    function scheduleRefresh(delay=hour){clearTimeout(timer);if(active&&!disposed)timer=setTimeout(()=>{if(!document.hidden)load();},delay);}
    async function load(force=false){
        if(refresh||disposed||!active)return;
        if(!force&&snapshot&&valid(snapshot)&&Date.now()-snapshot.checkedAt<hour){select();scheduleRefresh(hour-(Date.now()-snapshot.checkedAt));return;}
        if(Date.now()-lastAttempt<60000){sync();scheduleRefresh(60000);return;}
        lastAttempt=Date.now();clearTimeout(cooldown);cooldown=setTimeout(()=>{if(!disposed)sync();},60001);const controller=new AbortController();refresh=controller;sync();const timeout=setTimeout(()=>controller.abort(),8000);
        try{
            const response=await fetch(endpoint,{signal:controller.signal,credentials:'omit',referrerPolicy:'no-referrer'});
            if(!response.ok)throw Error('Rate provider unavailable');
            const raw=await response.text();if(raw.length>64000)throw Error('Rate table too large');const data=decode(JSON.parse(raw));
            if(disposed||refresh!==controller)return;
            snapshot=data;failed=false;try{sessionStorage.setItem(cacheKey,JSON.stringify(data));}catch(_){}
            invalidate();select();if(active)calculate();
        }catch(error){if(!disposed&&refresh===controller){failed=true;if(snapshot&&!valid(snapshot)){snapshot=null;invalidate();}if(snapshot){select();if(active)calculate();}else message(text.unavailable,true);}}
        finally{clearTimeout(timeout);if(refresh===controller){refresh=null;sync();scheduleRefresh(failed?5*60000:hour);} }
    }
    function show(currency,focus=false){
        active=currency;root.classList.toggle('is-currency',currency);unitPanels.forEach(el=>el.hidden=currency);panel.hidden=!currency;
        tabs.forEach((tab,i)=>{tab.setAttribute('aria-selected',String((i===1)===currency));tab.tabIndex=(i===1)===currency?0:-1;});
        try{sessionStorage.setItem('numforge-converter-mode',currency?'currency':'units');}catch(_){}
        if(focus)tabs[currency?1:0].focus();
        if(currency){if(snapshot&&valid(snapshot)){select();calculate();}load();}else{clearTimeout(timer);invalidate();}
    }
    tabs.forEach((tab,i)=>{tab.disabled=false;tab.addEventListener('click',()=>show(i===1),{signal:life.signal});tab.addEventListener('keydown',event=>{if(['ArrowLeft','ArrowRight','Home','End'].includes(event.key)){event.preventDefault();show(event.key==='Home'?false:event.key==='End'?true:!active,true);}},{signal:life.signal});});
    $('currency-form').addEventListener('submit',event=>{event.preventDefault();calculate();},{signal:life.signal});
    for(const el of [input,from,to,places])el.addEventListener(el===input?'input':'change',()=>{invalidate();save();$('currency-used').textContent='';message(english?'Choose Convert or press Enter.':'Zvoľ Previesť alebo stlač Enter.');},{signal:life.signal});
    $('currency-swap').addEventListener('click',()=>{const old=from.value;from.value=to.value;to.value=old;calculate();},{signal:life.signal});
    $('currency-refresh').addEventListener('click',()=>load(true),{signal:life.signal});$('currency-search').addEventListener('input',table,{signal:life.signal});
    $('currency-copy').addEventListener('click',async()=>{const value=copy,id=generation;if(!value)return;try{await navigator.clipboard.writeText(value);if(!disposed&&id===generation)message(text.copied);}catch(_){if(!disposed&&id===generation)message(text.copyFailed,true);}},{signal:life.signal});
    document.addEventListener('visibilitychange',()=>{if(!document.hidden&&active)load();},{signal:life.signal});
    let initial=false;try{initial=sessionStorage.getItem('numforge-converter-mode')==='currency';}catch(_){}show(location.hash==='#currencies'||initial);
    return ()=>{save();disposed=true;life.abort();clearTimeout(timer);clearTimeout(cooldown);refresh?.abort();invalidate();};
};
