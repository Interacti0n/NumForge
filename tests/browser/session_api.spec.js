const {test,expect}=require('@playwright/test');
const {randomBytes}=require('node:crypto');
const fresh=()=>randomBytes(16).toString('hex');
async function start(request,client=fresh()) {
    expect((await request.post(`/api/evaluate?precision=full&angle=rad&client=${client}&revision=1&action=start`,{data:''})).ok()).toBeTruthy();
    return client;
}
async function evaluate(request,client,revision,input,action='commit') {
    return (await request.post(`/api/evaluate?precision=full&angle=rad&client=${client}&revision=${revision}&action=${action}`,{data:input})).json();
}
async function read(request,path,client,query='') {
    const response=await request.get(`${path}?client=${client}${query}`);
    return {response,data:await response.json()};
}
async function mutate(request,client,revision,action) {
    const response=await request.post(`/api/session?client=${client}&revision=${revision}&action=${action}`,{data:''});
    return {response,data:await response.json()};
}

test('authoritative typed variable/history snapshots, pagination and isolation',async({request})=>{
    const client=await start(request), other=await start(request);
    await evaluate(request,client,1,'a=1/3');
    await evaluate(request,client,2,'b=qty(5;"m")*qty(5;"m")');
    await evaluate(request,client,3,'c=sqrt(2)');
    const before=(await read(request,'/api/session',client)).data;
    expect(before).toMatchObject({revision:'3',variable_count:3,variable_capacity:32,history_count:3,ans_available:true});
    const first=(await read(request,'/api/session/variables',client,'&limit=1&full=1&at=3')).data;
    expect(first).toMatchObject({total:3,next_offset:1,items:[{name:'a',value:{schema_version:1,kind:'rational',text:'1/3',approximate:false}}]});
    const second=(await read(request,'/api/session/variables',client,'&offset=1&limit=1&full=1&at=3')).data.items[0];
    expect(second.value).toMatchObject({quantity:true,dimensions:[2,0,0,0,0,0],temperature_point:false,text:'25'});
    const approximation=(await read(request,'/api/session/value',client,'&name=c')).data.value;
    expect(approximation.kind).toBe('decimal_approximation');expect(approximation.approximate).toBe(true);
    expect(approximation.text).toMatch(/^1\.414.*E\+0$/);
    const saved=(await read(request,'/api/session/history',client,'&id=2&full=1')).data.items[0];
    expect(saved).toMatchObject({id:'2',expression:'b=qty(5;"m")*qty(5;"m")',value:{text:'25'}});
    expect((await read(request,'/api/session',client)).data).toEqual(before);
    expect((await read(request,'/api/session/variables',other)).data.items).toEqual([]);
    expect((await evaluate(request,client,4,'a','preview')).result).toBe('1/3');
    expect((await read(request,'/api/session/variables',client,'&at=3')).response.status()).toBe(409);
});

test('reads preserve preview/random replay and history clear retains exact ans',async({request})=>{
    const client=await start(request);
    const preview=await evaluate(request,client,1,'rand()','preview');
    await read(request,'/api/session',client);
    await read(request,'/api/session/history',client);
    expect((await evaluate(request,client,2,'rand()')).result).toBe(preview.result);
    await evaluate(request,client,3,'x=qty(3;"m")');
    const cleared=await mutate(request,client,4,'clear-history');
    expect(cleared.data).toMatchObject({ok:true,history_count:0,ans_available:true,variable_count:1});
    expect((await mutate(request,client,4,'clear-history')).data.ok).toBe(true);
    expect((await mutate(request,client,4,'reset')).response.status()).toBe(409);
    expect((await evaluate(request,client,5,'ans+x')).result).toBe('6 m');
});

test('reset/release are ordered, retryable and cannot revive released clients',async({request})=>{
    const client=await start(request);
    await evaluate(request,client,1,'x=7');
    expect((await mutate(request,client,2,'reset')).data).toMatchObject({revision:'2',variable_count:0,history_count:0,ans_available:false});
    expect((await mutate(request,client,2,'reset')).data.ok).toBe(true);
    expect((await evaluate(request,client,1,'x=7')).ok).toBe(false);
    expect((await mutate(request,client,3,'release')).data.ok).toBe(true);
    expect((await mutate(request,client,3,'release')).data.ok).toBe(true);
    expect((await read(request,'/api/session',client)).data.ok).toBe(false);
    const revive=await request.post(`/api/evaluate?precision=10&angle=rad&client=${client}&revision=4&action=start`,{data:''});
    expect(revive.ok()).toBe(false);
    for(let i=0;i<8;i++)await start(request);
    expect((await mutate(request,client,3,'release')).data.ok).toBe(false); // bounded tombstone expired
});

test('function registry exposes canonical aliases, implemented arities and strict schemas',async({request})=>{
    const data=await (await request.get('/api/functions')).json();
    expect(data.schema_version).toBe(1);
    expect(data.functions.find(f=>f.name==='arcsin')).toMatchObject({canonical:'asin',minimum_arguments:1,maximum_arguments:1,implemented:true});
    expect(data.functions.find(f=>f.name==='min')).toMatchObject({minimum_arguments:2,maximum_arguments:256,variadic:true});
    expect(data.functions.find(f=>f.name==='qty')).toMatchObject({minimum_arguments:2,maximum_arguments:2});
    const client=await start(request);
    for(const query of ['&client='+client,'&offset=-1','&limit=0','&limit=33','&full=2','&at=18446744073709551616','&extra=1','&limit=%00']) {
        expect((await read(request,'/api/session/variables',client,query)).data.ok).toBe(false);
    }
    expect((await request.get('/api/functions?lang=sk')).ok()).toBe(false);
    await mutate(request,client,'18446744073709551615','reset');
    expect((await read(request,'/api/session',client)).data.revision).toBe('18446744073709551615');
});

test('conversion history stores authoritative values and retry never reevaluates variables',async({request})=>{
    const client=await start(request);
    await evaluate(request,client,1,'x=1/3');
    const endpoint=`/api/conversions?client=${client}&action=commit&revision=1&from=km&to=m&places=2&notation=plain`;
    const confirmed=await (await request.post(endpoint,{data:'x'})).json();
    expect(confirmed.entry).toMatchObject({expression:'x',result:'333.33',value:{text:'1000/3',unit:'m',dimensions:[1,0,0,0,0,0]}});
    await evaluate(request,client,2,'x=999');
    const replay=await (await request.post(endpoint,{data:'x'})).json();
    expect(replay.entry).toEqual(confirmed.entry);
    expect((await request.post(endpoint,{data:'x+1'})).status()).toBe(409);
    const before=(await read(request,'/api/session',client)).data;
    await request.post(`/api/convert?from=km&to=m&client=${client}`,{data:'x'});
    expect((await read(request,'/api/session',client)).data).toEqual(before);
    const stored=(await read(request,'/api/conversions',client,'&id=1&full=1')).data.items[0];
    expect(stored).toEqual(confirmed.entry);
    const clear=`/api/conversions?client=${client}&action=clear&revision=2`;
    expect((await (await request.post(clear,{data:''})).json()).ok).toBe(true);
    expect((await (await request.post(clear,{data:''})).json()).ok).toBe(true);
    expect((await read(request,'/api/conversions',client)).data.items).toEqual([]);
    expect((await read(request,'/api/session/value',client,'&name=ans')).data.value.text).toBe('999');
    expect((await request.post(endpoint,{data:'x'})).status()).toBe(409);
});

test('conversion errors preserve state and retain original expression columns',async({request})=>{
    const client=await start(request);
    const before=(await read(request,'/api/session',client)).data;
    const url=`/api/conversions?client=${client}&action=commit&revision=1&from=m&to=km`;
    const error=await (await request.post(url,{data:'1 + rand()'})).json();
    expect(error).toMatchObject({ok:false,code:'random_not_allowed',column:5});
    expect((await read(request,'/api/session',client)).data).toEqual(before);
    const incompatible=await (await request.post(url.replace('to=km','to=kg'),{data:'5'})).json();
    expect(incompatible.code).toBe('incompatible_units');
    expect((await read(request,'/api/session',client)).data).toEqual(before);
    const forbidden=await request.post(`/api/session?client=${client}&action=reset&revision=1`,{data:'',headers:{Origin:'https://example.com'}});
    expect(forbidden.status()).toBe(403);
});

test('large lists fail explicitly and smaller pages preserve full integers',async({request})=>{
    const client=await start(request);
    for(let i=0;i<5;i++)expect((await evaluate(request,client,i+1,'v'+String.fromCharCode(65+i)+'=2^100000')).ok).toBe(true);
    const before=(await read(request,'/api/session',client)).data;
    expect((await read(request,'/api/session/variables',client,'&limit=5')).data.status).toBe('value too large');
    const page=(await read(request,'/api/session/variables',client,'&limit=1&full=1')).data;
    expect(page.items[0].value.text).toBe((2n**100000n).toString());
    expect((await read(request,'/api/session',client)).data).toEqual(before);
});

test('browser lists follow server state instead of browser history',async({page})=>{
    await page.goto('/?lang=en');
    const input=page.locator('#expression');
    await input.fill('x=1');await input.press('Enter');
    await expect(page.locator('#variable-count')).toHaveText('1 / 32');
    const client=await page.evaluate(()=>sessionStorage.getItem('numforge-client-id'));
    const state=(await read(page.request,'/api/session',client)).data;
    await evaluate(page.request,client,BigInt(state.revision)+1n,'external=qty(7;"m")');
    await page.reload();
    await page.locator('#session-variables-tab').click();
    await expect(page.locator('#variable-list li')).toHaveCount(2);
    await expect(page.locator('#variable-list')).toContainText('external');
    await expect(page.locator('#history-list li')).toHaveCount(2);
    await page.locator('#session-history-tab').click();
    await page.locator('#clear-history').click();
    await expect(page.locator('#history-list li')).toHaveCount(0);
    await expect(page.locator('#variable-count')).toHaveText('2 / 32');
    expect((await read(page.request,'/api/session/value',client,'&name=ans')).data.value.quantity).toBe(true);
    await page.locator('#reset-session').click();
    await expect(page.locator('#variable-count')).toHaveText('0 / 32');
});

test('lost history-clear response retries the same lifecycle mutation',async({page})=>{
    await page.goto('/?lang=en');
    await page.locator('#expression').fill('x=qty(5;"m")');
    await page.locator('#expression').press('Enter');
    await expect(page.locator('#history-list li')).toHaveCount(1);
    let lose=true;
    await page.route('**/api/session?**action=clear-history',async route=>{
        if(lose){lose=false;await route.fetch();await route.abort('failed');}
        else await route.continue();
    });
    await page.locator('#clear-history').click();
    await expect(page.locator('#variable-status')).toContainText('retry');
    await page.locator('#clear-history').click();
    await expect(page.locator('#history-list li')).toHaveCount(0);
    await expect(page.locator('#variable-count')).toHaveText('1 / 32');
    const client=await page.evaluate(()=>sessionStorage.getItem('numforge-client-id'));
    expect((await read(page.request,'/api/session/value',client,'&name=ans')).data.value.quantity).toBe(true);
});
