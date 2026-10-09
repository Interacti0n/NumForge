const {test, expect}=require('@playwright/test');
for(const lang of ['sk','en']) for(const width of [390,1280]) {
    test(`complex values, forms and copying ${lang} ${width}`,async({page},testInfo)=>{
        await page.setViewportSize({width,height:844});await page.goto('/?lang='+lang);
        const input=page.locator('#expression'),result=page.locator('#result');
        await input.fill('z=complex(-1;0)');
        await expect(result).toHaveText('-1');
        await page.locator('.primary-button').click();
        await expect(page.locator('#variable-list')).toContainText('z');
        await input.fill('z');await expect(result).toHaveText('-1');
        await page.locator('#complex-form').selectOption('exp');
        await expect(result).toHaveText('e^(i*(π))');
        await page.locator('#copy-result').click();
        await expect.poll(()=>page.evaluate(()=>navigator.clipboard.readText())).toBe('complex(-1;0)');
        await page.locator('#complex-form').selectOption('trig');
        await expect(result).toHaveText('cos(π) + i*sin(π)');
        await page.locator('#complex-form').selectOption('cartesian');
        await input.fill('complex(1/3;1/3)^2');
        await expect(result).toHaveText('(2/9)*i');
        await page.locator('.primary-button').click();
        await input.fill('ans*9');await expect(result).toHaveText('2*i');
        for (const [expression,expected] of [
            ['i^2','-1'],['2+3i','2 + 3*i'],['re(1/3+2i)','1/3'],
            ['im(1+2/3*i)','2/3'],['conj(2+3i)','2 - 3*i'],
            ['abs(3/5+4/5*i)','1'],['arg(i)','1.5707963268'],
            ['re(e^(π*i))','-1'],['sin(re(i))','0'],
            ['sqrt(3+4i)','2 + i'],['sqrt(-1+0i)','i'],['sqrt(-3-4i)','1 - 2*i'],
            ['sqrt((1/3+i/7)^2)','1/3 + (1/7)*i'],['sqrt(i)','0.7071067812 + 0.7071067812*i'],
            ['ln(-1+0i)','3.1415926536*i'],['ln(i)','1.5707963268*i'],
            ['ln(1+i)','0.3465735903 + 0.7853981634*i'],['exp(ln(2+3i))','2 + 3*i']
        ]) {await input.fill(expression);await expect(result).toHaveText(expected);}
        const search=page.locator('#function-search');
        await search.fill(lang==='sk'?'imaginárna jednotka':'imaginary unit');
        await input.fill('');await page.locator('.function-groups [data-insert="i"]').click();
        await expect(input).toHaveValue('i');await expect(result).toHaveText('i');
        await search.fill('');await page.locator('#function-tab-6').click();
        await input.fill('conj(2+3i)');await expect(result).toHaveText('2 - 3*i');
        await page.screenshot({path:testInfo.outputPath('complex-library.png'),fullPage:true});
        const dimensions=await page.evaluate(()=>({body:document.documentElement.scrollWidth,width:innerWidth}));
        expect(dimensions.body).toBeLessThanOrEqual(dimensions.width);
    });
}
test('HTTP forms reject invalid options and retain exact snapshots',async({request})=>{
    const client='abababababababababababababababab';
    const base='/api/evaluate?precision=10&angle=rad&notation=auto';
    const start=await request.post(base+'&client='+client+'&revision=1&action=start',{data:''});expect(start.ok()).toBeTruthy();
    const response=await request.post(base+'&form=exp&client='+client+'&revision=2&action=commit',{data:'z=complex(-1;0)'});
    const data=await response.json();expect(data.result).toBe('e^(i*(π))');expect(data.copy).toBe('complex(-1;0)');
    const snapshot=await (await request.get('/api/session/variables?client='+client+'&full=1')).json();
    expect(snapshot.items[0].value.schema_version).toBe(2);
    expect(snapshot.items[0].value.components).toEqual({real:'-1',imaginary:'0'});
    expect(snapshot.items[0].value.kind).toBe('complex_rational');
    await request.post(base+'&form=cartesian&client='+client+'&revision=3&action=commit',{data:'complex(-1;0)'});
    await request.post(base+'&form=cartesian&client='+client+'&revision=4&action=preview',{data:'complex(2;3)'});
    const cached=await (await request.post(base+'&form=exp&client='+client+'&revision=5&action=preview',{data:'complex(-1;0)'})).json();
    expect(cached.result).toBe('e^(i*(π))');expect(cached.copy).toBe('complex(-1;0)');
    await request.post(base+'&form=cartesian&client='+client+'&revision=6&action=commit',{data:'w=complex(π;1/3)'});
    const approximate=await (await request.get('/api/session/value?client='+client+'&name=w')).json();
    expect(approximate.value.kind).toBe('complex_decimal_approximation');expect(approximate.value.approximate).toBe(true);
    expect(approximate.value.components.imaginary).toMatch(/^3\.333.*E-1$/);
    const invalid=await request.post(base+'&form=invalid',{data:'2+2'});expect(invalid.status()).toBe(400);
});

test('HTTP complex roots retain exact components and preserve real domains',async({request})=>{
    const client='efefefefefefefefefefefefefefefef',base='/api/evaluate?precision=10&angle=rad&notation=auto&client='+client;
    let revision=1;
    const send=async(body,action='commit')=>(await request.post(base+'&revision='+(revision++)+'&action='+action,{data:body})).json();
    expect((await send('','start')).ok).toBe(true);
    expect((await send('z=sqrt(3+4i)')).result).toBe('2 + i');
    let snapshot=await (await request.get('/api/session/value?client='+client+'&name=z')).json();
    expect(snapshot.value.kind).toBe('complex_rational');expect(snapshot.value.components).toEqual({real:'2',imaginary:'1'});
    expect((await send('z^2')).result).toBe('3 + 4*i');
    expect((await send('sqrt(-1)')).ok).toBe(false);
    expect((await send('ans')).result).toBe('3 + 4*i');
    expect((await send('sqrt(-1+0i)')).result).toBe('i');
    expect((await send('sqrt(i)')).result).toBe('0.7071067812 + 0.7071067812*i');
    snapshot=await (await request.get('/api/session/value?client='+client+'&name=ans')).json();
    expect(snapshot.value.kind).toBe('complex_decimal_approximation');expect(snapshot.value.approximate).toBe(true);
});

test('HTTP principal logarithm retains radians, typed values and failed-assignment state',async({request})=>{
    const client='bcbcbcbcbcbcbcbcbcbcbcbcbcbcbcbc',base='/api/evaluate?precision=10&angle=deg&notation=auto&client='+client;
    let revision=1;
    const send=async(body,action='commit')=>(await request.post(base+'&revision='+(revision++)+'&action='+action,{data:body})).json();
    expect((await send('','start')).ok).toBe(true);
    expect((await send('z=ln(i)')).result).toBe('1.5707963268*i');
    let snapshot=await (await request.get('/api/session/value?client='+client+'&name=z')).json();
    expect(snapshot.value.kind).toBe('complex_decimal_approximation');expect(snapshot.value.approximate).toBe(true);
    expect(Number(snapshot.value.components.real)).toBe(0);
    expect(Number(snapshot.value.components.imaginary)).toBeCloseTo(Math.PI/2,12);
    expect((await send('ln(-1)')).ok).toBe(false);
    expect((await send('z=ln(0*i)')).ok).toBe(false);
    expect((await send('ans')).result).toBe('1.5707963268*i');
    expect((await send('ln(-1+0i)')).result).toBe('3.1415926536*i');
    expect((await send('exp(ln(2+3i))')).result).toBe('2 + 3*i');
    expect((await send('ln(-3-4i)')).result).toBe('1.6094379124 - 2.2142974356*i');
    const positiveCut=await send('ln(-1+1E-40i)'),negativeCut=await send('ln(-1-1E-40i)');
    expect(positiveCut.result).toContain('+ 3.1415926536*i');
    expect(negativeCut.result).toContain('- 3.1415926536*i');
    expect(positiveCut.result).toContain('5E-81');expect(negativeCut.result).toContain('5E-81');
});

test('HTTP imaginary unit, projections and variable names preserve session semantics',async({request})=>{
    const client='cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd',base='/api/evaluate?precision=10&angle=deg&client='+client;
    let revision=1;
    const send=async(body,action='commit')=>(await request.post(base+'&revision='+(revision++)+'&action='+action,{data:body})).json();
    expect((await send('','start')).ok).toBe(true);
    expect((await send('i=7')).ok).toBe(false);
    for(const [expression,result] of [['I=7','7'],['x=2','2'],['y=3','3'],['xy=10','10'],
        ['x*y','6'],['xy','10'],['z=1/3+2/3*i','1/3 + (2/3)*i'],['re(z)','1/3'],
        ['im(z)','2/3'],['conj(z)','1/3 - (2/3)*i'],['abs(3/5+4/5*i)','1'],['arg(i)','1.5707963268']])
        expect((await send(expression)).result).toBe(result);
    const variables=await (await request.get('/api/session/variables?client='+client+'&full=1')).json();
    const names=variables.items.map(item=>item.name);
    expect(names).toEqual(['I','x','y','xy','z']);
    expect(variables.items.find(item=>item.name==='z').value.components).toEqual({real:'1/3',imaginary:'2/3'});
    expect((await send('arg(0)')).ok).toBe(false);
    expect((await send('2^i')).ok).toBe(false);
    const exponential=await send('e^(π*i)');expect(exponential.ok).toBe(true);
    expect(exponential.result).toMatch(/^-1/);
    const snapshot=await (await request.get('/api/session/value?client='+client+'&name=ans')).json();
    expect(snapshot.value.kind).toBe('complex_decimal_approximation');
    expect(Math.abs(Number(snapshot.value.components.real)+1)).toBeLessThan(1e-10);
    expect(Math.abs(Number(snapshot.value.components.imaginary))).toBeLessThan(1e-30);
    const registry=await (await request.get('/api/functions')).json();
    for(const name of ['re','im','conj','arg']) expect(JSON.stringify(registry)).toContain('"'+name+'"');
});
