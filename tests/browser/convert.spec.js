const {test, expect} = require('@playwright/test');

for (const lang of ['sk', 'en']) {
    test(`convert expressions and localized errors (${lang})`, async ({page}) => {
        await page.goto('/?lang='+lang);
        const input=page.locator('#expression');
        await input.fill('convert(90;"km/h";"m/s")');
        await expect(page.locator('#result')).toHaveText('25');
        await input.press('Enter');
        await input.fill('ans+1');
        await expect(page.locator('#result')).toHaveText('26');
        await input.fill('convert(1;"m";"s")');
        await expect(page.locator('#result')).toContainText(lang==='sk'?'nekompatibilné jednotky':'incompatible units');
        await input.fill('convert(1;"mb";"B")');
        await expect(page.locator('#result')).toContainText(lang==='sk'?'neznáma jednotka':'unknown unit');
        await page.goto('/units?lang='+lang);
        await page.locator('#unit-input').fill('convert(1;"m";"s")');
        await expect(page.locator('#unit-status')).toContainText(lang==='sk'?'Tieto jednotky nie sú kompatibilné':'These units are not compatible');
        await expect(page.locator('#unit-status')).toContainText('15');
    });
}

test('convert works through both HTTP expression routes and retains numeric session semantics', async ({request}) => {
    const evaluate=async(input)=> (await request.post('/api/evaluate?precision=full&angle=rad',{data:input})).json();
    expect(await evaluate('convert(1/3;"km";"m")')).toMatchObject({ok:true,result:'1000/3'});
    expect(await evaluate('convert(1;"m";"s")')).toMatchObject({ok:false,status:'incompatible units',column:15});
    expect(await evaluate('convert(1;"bad";"m")')).toMatchObject({ok:false,status:'unknown unit',column:11});
    expect(await evaluate('convert(180;"deg";"rad")')).toMatchObject({ok:true,result:'3.141592653589793238462643383279503'});
    const response=await request.post('/api/convert?from=m&to=km',{data:'convert(1/3;"km";"m")'});
    expect(await response.json()).toMatchObject({ok:true,result:'1/3',input_approximate:false});
    const denied=await request.post('/api/convert?from=m&to=km',{data:'convert(rand();"km";"m")'});
    expect(await denied.json()).toMatchObject({ok:false,code:'random_not_allowed'});
});
