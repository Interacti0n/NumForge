const {test, expect} = require('@playwright/test');
const net = require('node:net');

test('unit HTTP catalogue is complete, localized and has provenance', async ({request}) => {
    const response = await request.get('/api/units');
    expect(response.status()).toBe(200);
    const body = await response.json();
    expect(body.ok).toBe(true);
    expect(body.units).toHaveLength(233);
    expect(new Set(body.units.map(u=>u.id)).size).toBe(233);
    for (const u of body.units) {
        expect(u.name_en.length).toBeGreaterThan(0);
        expect(u.name_sk.length).toBeGreaterThan(0);
        expect(u.source_url).toMatch(/^https:\/\//);
    }
    expect(body.units.find(u=>u.id==='um')).toMatchObject({symbol:'µm',name_sk:'mikrometer'});
    expect(body.units.find(u=>u.id==='gon')).toMatchObject({quantity:'angle',pi_power:1});
    expect(body.units.find(u=>u.id==='B').quantity).toBe('information');
});

test('conversion preserves exact inputs, supports angles, options and errors', async ({request}) => {
    const convert = async (query,input) => {
        const r=await request.post('/api/convert?'+query,{data:input});
        return {status:r.status(),body:await r.json()};
    };
    expect(await convert('from=km&to=m','1/3')).toMatchObject({status:200,body:{ok:true,result:'1000/3',unit:'m',input_approximate:false,factor_approximate:false}});
    expect(await convert('from=deg&to=rad&precision=30&places=10','180')).toMatchObject({body:{result:'3.1415926536',factor_approximate:true}});
    expect(await convert('from=degC&to=degF','100')).toMatchObject({body:{result:'212',unit:'degF',symbol:'°F'}});
    expect(await convert('from=MiB&to=B','2^10')).toMatchObject({body:{result:'1073741824'}});
    expect(await convert('from=m&to=cm','sqrt(2)')).toMatchObject({body:{input_approximate:true,factor_approximate:false}});
    expect(await convert('from=km%2Fh&to=m%2Fs&places=2&rounding=floor&notation=plain','1')).toMatchObject({body:{result:'0.27'}});
    expect(await convert('from=kg&to=m','1')).toMatchObject({status:400,body:{code:'incompatible_units'}});
    expect(await convert('from=bad&to=m','1')).toMatchObject({status:400,body:{code:'unknown_unit'}});
    for(const query of ['from=m&to=m&precision=0','from=m&to=m&places=10001',
        'from=m&to=m&action=commit','from=m&to=m&from=km','from=m%00&to=m',
        'from=m&to=m&rounding=bad'])
        expect(await convert(query,'1')).toMatchObject({status:400,body:{code:'invalid_options'}});
    expect(await convert('from=m&to=cm','x=5')).toMatchObject({status:400,body:{code:'assignment_not_allowed',column:2}});
    expect(await convert('from=m&to=cm','1+rand()')).toMatchObject({status:400,body:{code:'random_not_allowed',column:3}});
    expect(await convert('from=m&to=cm','1/0')).toMatchObject({status:400,body:{code:'expression_error',status:'division by zero'}});
    expect(await convert('from=m&to=cm','1'.repeat(4097))).toMatchObject({status:413,body:{status:'value too large'}});
    expect(await convert('from=m&to=cm','1E1000000')).toMatchObject({status:400,body:{code:'value_too_large'}});
});

test('conversion reads isolated session variables and ans without changing previews or revisions', async ({request}) => {
    const client='9'.repeat(32), other='8'.repeat(32);
    const send=async(id,revision,action,input='')=> (await request.post('/api/evaluate?precision=full&angle=rad&client='+id+'&revision='+revision+'&action='+action,{data:input})).json();
    const convert=async(id,input)=> (await request.post('/api/convert?from=km&to=m&client='+id,{data:input})).json();
    expect((await convert(client,'1')).code).toBe('session_expired');
    expect((await send(client,1,'start')).ok).toBe(true);
    expect((await send(client,2,'commit','x=2/3')).ok).toBe(true);
    const preview=await send(client,3,'preview','rand()');
    expect((await convert(client,'x+ans')).result).toBe('4000/3');
    expect((await convert(client,'rand()')).code).toBe('random_not_allowed');
    expect((await convert(client,'x=5')).code).toBe('assignment_not_allowed');
    expect((await send(client,4,'commit','rand()')).result).toBe(preview.result);
    expect((await convert(client,'x')).result).toBe('2000/3');
    expect((await send(other,1,'start')).ok).toBe(true);
    expect((await convert(other,'x')).status).toBe('variable is undefined');
    expect((await convert(other,'ans')).status).toBe('ans is undefined');
    expect((await send(client,5,'preview','x')).result).toBe('2/3');
});

test('conversion uses existing origin and Content-Length protections', async ({request,baseURL}) => {
    const forbidden=await request.post('/api/convert?from=m&to=cm',{data:'1',headers:{Origin:'https://example.org'}});
    expect(forbidden.status()).toBe(403);
    const port=Number(new URL(baseURL).port);
    const raw=async text=>new Promise((resolve,reject)=>{
        const socket=net.createConnection({host:'127.0.0.1',port});
        let response=''; socket.setTimeout(5000,()=>{socket.destroy();reject(Error('timeout'));});
        socket.on('connect',()=>socket.write(text)); socket.on('data',x=>response+=x.toString());
        socket.on('end',()=>resolve(response));socket.on('error',reject);
    });
    expect(await raw('POST /api/convert?from=m&to=cm HTTP/1.1\r\nHost: localhost\r\n\r\n')).toContain('411 Length Required');
    expect(await raw('POST /api/convert?from=m&to=cm HTTP/1.1\r\nHost: localhost\r\nContent-Length: 3\r\n\r\n1\x002')).toContain('invalid_body');
});
