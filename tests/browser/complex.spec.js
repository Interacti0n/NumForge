const {test, expect}=require('@playwright/test');
for(const lang of ['sk','en']) for(const width of [390,1280]) {
    test(`complex inverse trigonometry uses radians ${lang} ${width}`,async({page})=>{
        await page.addInitScript(()=>localStorage.setItem('numforge-angle-unit','deg'));
        await page.setViewportSize({width,height:844});await page.goto('/?lang='+lang);
        const input=page.locator('#expression'),result=page.locator('#result');
        for(const [expression,expected] of [
            ['asin(i)','0.881373587*i'],['asin(1+i)','0.6662394325 + 1.0612750619*i'],
            ['acos(1+i)','0.9045568943 - 1.0612750619*i'],
            ['atan(1+i)','1.0172219679 + 0.4023594781*i'],
            ['asin(0.1+1E-100i)','0.1001674212 + 1.0050378153E-100*i'],
            ['acos(1-1E-60+0i)','1.4142135624E-30'],
            ['asin(2+0i)','1.5707963268 - 1.3169578969*i'],
            ['arcsin(0.5+0i)','0.5235987756'],['asin(0.5)','30']
        ]) {await input.fill(expression);await expect(result).toHaveText(expected);}
    });
}
test('HTTP complex inverse trigonometry preserves domains and failed commits',async({request})=>{
    const client='a4a4a4a4a4a4a4a4a4a4a4a4a4a4a4a4';
    const base='/api/evaluate?precision=10&angle=deg&notation=auto&form=cartesian&client='+client;
    expect((await (await request.post(base+'&revision=1&action=start',{data:''})).json()).ok).toBe(true);
    let revision=2;
    for(const [expression,expected] of [
        ['z=asin(i)','0.881373587*i'],['acos(2+0i)','1.3169578969*i'],
        ['atan(2i)','1.5707963268 + 0.5493061443*i'],
        ['atan(-2i)','-1.5707963268 - 0.5493061443*i'],
        ['arcuscos(1+i)','0.9045568943 - 1.0612750619*i'],
        ['asin(0.5)','30']
    ]) {
        const data=await (await request.post(base+'&revision='+revision+++'&action=preview',{data:expression})).json();
        expect(data.ok).toBe(true);expect(data.result).toBe(expected);
    }
    const saved=await (await request.post(base+'&revision='+revision+++'&action=commit',{data:'z=asin(i)'})).json();
    expect(saved.ok).toBe(true);
    const before=await (await request.get('/api/session/value?client='+client+'&name=z')).json();
    expect(before.value.kind).toBe('complex_decimal_approximation');expect(before.value.approximate).toBe(true);
    for(const expression of ['z=atan(i)','z=atan(-i)','z=asin(2)','z=acos(2)']) {
        const data=await (await request.post(base+'&revision='+revision+++'&action=commit',{data:expression})).json();
        expect(data.ok).toBe(false);
    }
    const after=await (await request.get('/api/session/value?client='+client+'&name=z')).json();
    expect(after.value).toEqual(before.value);
    const answer=await (await request.get('/api/session/value?client='+client+'&name=ans')).json();
    expect(answer.value).toEqual(before.value);
});
test('HTTP inverse branches agree with independent real-component identities',async({request})=>{
    test.setTimeout(60000);
    const client='a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5';
    const base='/api/evaluate?precision=10&client='+client;let revision=1;
    expect((await (await request.post(base+'&revision='+revision+++'&action=start',{data:''})).json()).ok).toBe(true);
    for(const x of [-2,-0.25,0.25,2]) for(const y of [-1.5,-0.1,0.1,1.5]) {
        const radius=(Math.hypot(x+1,y)+Math.hypot(x-1,y))/2;
        const asin=[Math.asin(x/radius),Math.sign(y)*Math.acosh(radius)];
        const references={asin,acos:[Math.PI/2-asin[0],-asin[1]],
            atan:[Math.atan2(2*x,1-x*x-y*y)/2,Math.log((x*x+(y+1)**2)/(x*x+(y-1)**2))/4]};
        for(const [fn,[re,im]] of Object.entries(references)) {
            const expression=`z=${fn}(complex(${x};${y}))`;
            const data=await (await request.post(base+'&revision='+revision+++'&action=commit',{data:expression})).json();
            expect(data.ok,expression).toBe(true);
            const snapshot=await (await request.get('/api/session/value?client='+client+'&name=z')).json();
            expect(Number(snapshot.value.components.real),expression).toBeCloseTo(re,9);
            expect(Number(snapshot.value.components.imaginary),expression).toBeCloseTo(im,9);
        }
    }
    for(const [expression,re,im] of [
        ['asin(2+1E-20i)',Math.PI/2,Math.acosh(2)],
        ['asin(2-1E-20i)',Math.PI/2,-Math.acosh(2)],
        ['atan(1E-20+2i)',Math.PI/2,Math.log(3)/2],
        ['atan(-1E-20+2i)',-Math.PI/2,Math.log(3)/2]
    ]) {
        expect((await (await request.post(base+'&revision='+revision+++'&action=commit',{data:'z='+expression})).json()).ok).toBe(true);
        const snapshot=await (await request.get('/api/session/value?client='+client+'&name=z')).json();
        expect(Number(snapshot.value.components.real)).toBeCloseTo(re,9);
        expect(Number(snapshot.value.components.imaginary)).toBeCloseTo(im,9);
    }
});
for(const lang of ['sk','en']) for(const width of [390,1280]) {
    test(`complex hyperbolic functions use radians ${lang} ${width}`,async({page})=>{
        await page.addInitScript(()=>localStorage.setItem('numforge-angle-unit','deg'));
        await page.setViewportSize({width,height:844});await page.goto('/?lang='+lang);
        const input=page.locator('#expression'),result=page.locator('#result');
        for(const [expression,expected] of [
            ['sinh(i)','0.8414709848*i'],['cosh(i)','0.5403023059'],['tanh(i)','1.5574077247*i'],
            ['sinh(1+i)','0.6349639148 + 1.2984575814*i'],
            ['cosh(1+i)','0.8337300251 + 0.9888977058*i'],
            ['tanh(1+i)','1.0839233273 + 0.2717525853*i']
        ]) {await input.fill(expression);await expect(result).toHaveText(expected);}
    });
}
for(const lang of ['sk','en']) for(const width of [390,1280]) {
    test(`complex trigonometry uses radians ${lang} ${width}`,async({page})=>{
        await page.addInitScript(()=>localStorage.setItem('numforge-angle-unit','deg'));
        await page.setViewportSize({width,height:844});await page.goto('/?lang='+lang);
        const input=page.locator('#expression'),result=page.locator('#result');
        for(const [expression,expected] of [
            ['sin(i)','1.1752011936*i'],['cos(i)','1.5430806348'],['tan(i)','0.761594156*i'],
            ['sin(1+i)','1.2984575814 + 0.6349639148*i'],
            ['cos(1+i)','0.8337300251 - 0.9888977058*i'],
            ['tan(1+i)','0.2717525853 + 1.0839233273*i'],['sin(90)','1']
        ]) {await input.fill(expression);await expect(result).toHaveText(expected);}
    });
}
for(const lang of ['sk','en']) for(const width of [390,1280]) {
    test(`complex values, forms and copying ${lang} ${width}`,async({page},testInfo)=>{
        test.setTimeout(30000); // Exercises the growing complex-function catalogue.
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
            ['sqrt(-1)','i'],['sqrt(-4/9)','(2/3)*i'],['sqrt(-2)','1.4142135624*i'],['sqrt(3+4i)','2 + i'],['sqrt(-1+0i)','i'],['sqrt(-3-4i)','1 - 2*i'],
            ['sqrt((1/3+i/7)^2)','1/3 + (1/7)*i'],['sqrt(i)','0.7071067812 + 0.7071067812*i'],
            ['ln(-1+0i)','3.1415926536*i'],['ln(i)','1.5707963268*i'],
            ['ln(1+i)','0.3465735903 + 0.7853981634*i'],['exp(ln(2+3i))','2 + 3*i'],
            ['i^i','0.2078795764'],['2^i','0.7692389014 + 0.6389612763*i'],
            ['(-1+0i)^0.3','0.5877852523 + 0.8090169944*i'],['pow(2;i)','0.7692389014 + 0.6389612763*i'],
            ['log(i)','0.6821881769*i'],['log(-1+0i;i)','2'],['log(-i;i)','-1'],
            ['log(1+i;2+i)','0.7455202636 + 0.5464509967*i']
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
    expect((await send('sqrt(-1)')).result).toBe('i');
    expect((await send('ans')).result).toBe('i');
    expect((await send('x=-4/9')).result).toBe('-4/9');
    expect((await send('w=sqrt(x)')).result).toBe('(2/3)*i');
    const promoted=await (await request.get('/api/session/value?client='+client+'&name=w')).json();
    expect(promoted.value.components).toEqual({real:'0',imaginary:'2/3'});
    expect((await send('root(-1;2)')).ok).toBe(false);
    expect((await send('ans')).result).toBe('(2/3)*i');
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

test('HTTP principal powers preserve branches, exact integers and failed state',async({request})=>{
    const client='dededededededededededededededede',base='/api/evaluate?precision=10&angle=deg&client='+client;
    let revision=1;
    const send=async(data,action='commit')=>(await request.post(base+'&revision='+(revision++)+'&action='+action,{data})).json();
    expect((await send('','start')).ok).toBe(true);
    expect((await send('z=i^i')).result).toBe('0.2078795764');
    let snapshot=await (await request.get('/api/session/value?client='+client+'&name=z')).json();
    expect(snapshot.value.kind).toBe('complex_decimal_approximation');
    expect(snapshot.value.approximate).toBe(true);
    expect(Number(snapshot.value.components.real)).toBeCloseTo(Math.exp(-Math.PI/2),12);
    expect((await send('z=(0*i)^i')).ok).toBe(false);
    expect((await send('z=(0*i)^(-1/2)')).ok).toBe(false);
    expect((await send('ans')).result).toBe('0.2078795764');
    expect((await send('2^0.5')).ok).toBe(false);
    expect((await send('pow(2;i)')).result).toBe('0.7692389014 + 0.6389612763*i');
    expect((await send('(-1+0i)^0.3')).result).toBe('0.5877852523 + 0.8090169944*i');
    expect((await send('(-1-1E-20i)^0.3')).result).toBe('0.5877852523 - 0.8090169944*i');
    expect((await send('(0*i)^(1/2)')).result).toBe('0');
    expect((await send('(0*i)^0')).result).toBe('1');
    expect((await send('(1+i)^3')).result).toBe('-2 + 2*i');
    snapshot=await (await request.get('/api/session/value?client='+client+'&name=ans')).json();
    expect(snapshot.value.approximate).toBe(false);
});

test('HTTP complex base logarithms preserve domains and failed state',async({request})=>{
    const client='f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0',base='/api/evaluate?precision=10&angle=deg&client='+client;
    let revision=1;
    const send=async(data,action='commit')=>(await request.post(base+'&revision='+(revision++)+'&action='+action,{data})).json();
    expect((await send('','start')).ok).toBe(true);
    expect(await send('z=log(i;i)')).toMatchObject({ok:true,result:'1'});
    const snapshot=await (await request.get('/api/session/value?client='+client+'&name=z')).json();
    expect(snapshot.value.kind).toBe('complex_decimal_approximation');expect(snapshot.value.approximate).toBe(true);
    for(const input of ['z=log(i;1)','z=log(i;0)','z=log(0*i)','log(-1)','log(2;-1)',
        'log(qty(2;"m");i)','log(i;qty(2;"m"))']) expect((await send(input)).ok).toBe(false);
    expect((await send('ans')).result).toBe('1');
    expect((await send('log(i)')).result).toBe('0.6821881769*i');
    expect((await send('log(-1+0i;i)')).result).toBe('2');
    expect((await send('log(-1-1E-20i;i)')).ok).toBe(true);
    const cut=await (await request.get('/api/session/value?client='+client+'&name=ans')).json();
    expect(Number(cut.value.components.real)).toBeCloseTo(-2,12);
    const cutImaginary=Number(cut.value.components.imaginary);
    expect(cutImaginary).toBeLessThan(0);expect(Math.abs(cutImaginary)).toBeLessThan(1e-30);
    expect((await send('log(-i;i)')).result).toBe('-1');
    expect((await send('log(i;-1)')).result).toBe('0.5');
    expect((await send('log(1+i;2+i)')).result).toBe('0.7455202636 + 0.5464509967*i');
    const stored=await (await request.get('/api/session/value?client='+client+'&name=z')).json();
    expect(stored.value.components).toEqual(snapshot.value.components);
});

test('HTTP complex trigonometry preserves radians, near-pole values and failed state',async({request})=>{
    const client='a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2',base='/api/evaluate?precision=10&angle=deg&client='+client;
    let revision=1;
    const send=async(data,action='commit')=>(await request.post(base+'&revision='+(revision++)+'&action='+action,{data})).json();
    expect((await send('','start')).ok).toBe(true);
    expect((await send('z=cos(i)')).result).toBe('1.5430806348');
    let snapshot=await (await request.get('/api/session/value?client='+client+'&name=z')).json();
    expect(snapshot.value.kind).toBe('complex_decimal_approximation');expect(snapshot.value.approximate).toBe(true);
    expect(Number(snapshot.value.components.imaginary)).toBe(0);
    expect((await send('z=tan(90)')).ok).toBe(false);
    expect((await send('ans')).result).toBe('1.5430806348');
    expect((await send('asinh(i)')).ok).toBe(false);
    expect((await send('sin(1+i)')).result).toBe('1.2984575814 + 0.6349639148*i');
    expect((await send('cos(1-i)')).result).toBe('0.8337300251 + 0.9888977058*i');
    expect((await send('tan(-i)')).result).toBe('-0.761594156*i');
    expect((await send('tan(π/2+1E-8i)')).ok).toBe(true);
    snapshot=await (await request.get('/api/session/value?client='+client+'&name=ans')).json();
    expect(Number(snapshot.value.components.imaginary)/1e8).toBeCloseTo(1,10);
    expect(Math.abs(Number(snapshot.value.components.real))).toBeLessThan(1e-10);
    expect((await send('tan(20i)')).result).toBe('i');
    expect((await send('tan(1E-40i)')).result).toBe('1E-40*i');
    expect((await send('sin(90+0i)')).result).toBe('0.8939966636');
    const stored=await (await request.get('/api/session/value?client='+client+'&name=z')).json();
    expect(Number(stored.value.components.real)).toBeCloseTo(Math.cosh(1),12);
});

test('HTTP complex hyperbolic functions preserve tiny tails, poles and session state',async({request})=>{
    const client='a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3',base='/api/evaluate?precision=10&angle=deg&client='+client;
    let revision=1;
    const send=async(data,action='commit')=>(await request.post(base+'&revision='+(revision++)+'&action='+action,{data})).json();
    expect((await send('','start')).ok).toBe(true);
    expect((await send('z=cosh(i)')).result).toBe('0.5403023059');
    let snapshot=await (await request.get('/api/session/value?client='+client+'&name=z')).json();
    expect(snapshot.value.kind).toBe('complex_decimal_approximation');expect(snapshot.value.approximate).toBe(true);
    expect((await send('z=atanh(i)')).ok).toBe(false);
    expect((await send('ans')).result).toBe('0.5403023059');
    expect((await send('tanh(-1+i)')).result).toBe('-1.0839233273 + 0.2717525853*i');
    expect((await send('tanh(0.25+0.5i)')).result).toBe('0.3124206925 + 0.5045007027*i');
    expect((await send('tanh(0.5+0.5i)')).result).toBe('0.5640831413 + 0.4038964553*i');
    expect((await send('tanh(0.500000000001+0.5i)')).result).toBe('0.5640831413 + 0.4038964553*i');
    expect((await send('tanh(1E-40+0i)')).result).toBe('1E-40');
    expect((await send('tanh(1E100+0i)')).result).toBe('1');
    expect((await send('tanh(1000+i)')).ok).toBe(true);
    snapshot=await (await request.get('/api/session/value?client='+client+'&name=ans')).json();
    expect(Number(snapshot.value.components.real)).toBe(1);expect(snapshot.value.components.imaginary).not.toBe('0');
    expect((await send('tanh(1E-8+π/2*i)')).ok).toBe(true);
    snapshot=await (await request.get('/api/session/value?client='+client+'&name=ans')).json();
    expect(Number(snapshot.value.components.real)/1e8).toBeCloseTo(1,10);
    expect(Math.abs(Number(snapshot.value.components.imaginary))).toBeLessThan(1e-10);
    expect((await send('cosh(1+i)^2-sinh(1+i)^2')).ok).toBe(true);
    snapshot=await (await request.get('/api/session/value?client='+client+'&name=ans')).json();
    expect(Number(snapshot.value.components.real)).toBeCloseTo(1,12);
    expect(Math.abs(Number(snapshot.value.components.imaginary))).toBeLessThan(1e-30);
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
    expect((await send('(0*i)^i')).ok).toBe(false);
    const exponential=await send('e^(π*i)');expect(exponential.ok).toBe(true);
    expect(exponential.result).toMatch(/^-1/);
    const snapshot=await (await request.get('/api/session/value?client='+client+'&name=ans')).json();
    expect(snapshot.value.kind).toBe('complex_decimal_approximation');
    expect(Math.abs(Number(snapshot.value.components.real)+1)).toBeLessThan(1e-10);
    expect(Math.abs(Number(snapshot.value.components.imaginary))).toBeLessThan(1e-30);
    const registry=await (await request.get('/api/functions')).json();
    for(const name of ['re','im','conj','arg']) expect(JSON.stringify(registry)).toContain('"'+name+'"');
});
