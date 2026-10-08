const {test, expect} = require('@playwright/test');

test('quantity arithmetic is available through the expression API', async ({request}) => {
    const send=async(input)=> (await request.post('/api/evaluate?precision=full&angle=rad',{data:input})).json();
    expect(await send('qty(5;"m")*qty(5;"m")')).toMatchObject({ok:true,result:'25 m²'});
    expect(await send('qty(1;"km")+qty(500;"m")')).toMatchObject({ok:true,result:'1.5 km'});
    expect(await send('qty(36;"km/h")*qty(10;"s")')).toMatchObject({ok:true,result:'100 m'});
    expect(await send('qty(20;"degC")-qty(10;"degC")')).toMatchObject({ok:true,result:'10 Δ°C'});
    expect(await send('qty(1;"m")+qty(1;"s")')).toMatchObject({ok:false,status:'invalid quantity operation',column:11});
    expect(await send('qty(1;"bad")')).toMatchObject({ok:false,status:'unknown unit',column:7});
    expect(await send('convert(qty(1;"km");"m";"cm")')).toMatchObject({ok:true,result:'100000'});
    const r=await request.post('/api/convert?from=m&to=cm',{data:'qty(1;"m")'});
    expect(await r.json()).toMatchObject({ok:false,code:'quantity_not_allowed'});
});

for (const lang of ['sk','en']) {
    test(`calculator preserves quantity variables and ans (${lang})`, async ({page}) => {
        await page.goto('/?lang='+lang);
        const input=page.locator('#expression'), result=page.locator('#result');
        await input.fill('x=qty(3;"m")');
        await expect(result).toHaveText('3 m');
        await input.press('Enter');
        await input.fill('x*x');
        await expect(result).toHaveText('9 m²');
        await input.press('Enter');
        await input.fill('ans/qty(3;"m")');
        await expect(result).toHaveText('3 m');
        await input.fill('x+qty(1;"s")');
        await expect(result).toContainText(lang==='sk'?'neplatná operácia s veličinami':'invalid quantity operation');
        await input.fill('x+qty(200;"cm")');
        await expect(result).toHaveText('5 m');
    });
}
