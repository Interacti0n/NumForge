const {test,expect}=require('@playwright/test');
for(const lang of ['en','sk'])for(const width of [390,1280])for(const tool of ['graph','calculator'])test(`library insertion supplies parentheses and preserves selected expressions ${tool} ${lang} ${width}`,async({page})=>{
 await page.setViewportSize({width,height:844});await page.goto((tool==='graph'?'/graph':'/')+'?lang='+lang);
 const graph=tool==='graph',input=page.locator(graph?'#graph-library-expression':'#expression'),search=page.locator('#function-search');
 if(graph)await page.locator('#graph-library summary').click();
 const choose=async name=>{await search.fill(name);const button=page.locator(`${graph?'#graph-functions ':''}button[data-function="${name}"]`);await expect(button).toBeEnabled();if(!graph)await expect(button).toHaveAttribute('data-minimum-arguments',/\d+/);await button.click();};
 await input.fill('');await choose('sqrt');await expect(input).toHaveValue('sqrt()');if(graph)await expect(page.locator('#graph-expression')).toHaveValue('sqrt()');await expect(input).toBeFocused();await expect(input).toHaveJSProperty('selectionStart',5);await expect(input).toHaveJSProperty('selectionEnd',5);
 await input.pressSequentially(graph?'x':'9');await expect(input).toHaveValue(graph?'sqrt(x)':'sqrt(9)');
 if(graph){await page.locator('#graph-library-plot').click();await expect(page.locator('path.graph-line')).toBeVisible();await page.locator('#graph-library summary').click();}else{await input.press('Enter');await expect(page.locator('#result')).toHaveText('3');}
 // Wrapping replaces only the selected subexpression, leaving its surroundings intact.
 await input.fill('1+(x+1)');await input.evaluate(e=>e.setSelectionRange(3,6));await choose('sqrt');await expect(input).toHaveValue('1+(sqrt(x+1))');await expect(input).toHaveJSProperty('selectionStart',12);
 await input.fill('sin()');await input.evaluate(e=>e.setSelectionRange(4,4));await choose('sqrt');await expect(input).toHaveValue('sin(sqrt())');await expect(input).toHaveJSProperty('selectionStart',9);
 await input.fill('');await choose('root');await expect(input).toHaveValue('root(;)');await expect(input).toHaveJSProperty('selectionStart',5);
 await input.pressSequentially(graph?'x':'9');await input.press('ArrowRight');await input.pressSequentially('2');await expect(input).toHaveValue(graph?'root(x;2)':'root(9;2)');
 if(graph){await page.locator('#graph-library-plot').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('path.graph-line')).toBeVisible();await page.locator('#graph-library summary').click();}else{await input.press('Enter');await expect(page.locator('#result')).toHaveText('3');}
 await input.fill('x+1');await input.evaluate(e=>e.setSelectionRange(0,3));await choose('root');await expect(input).toHaveValue('root(x+1;)');await expect(input).toHaveJSProperty('selectionStart',9);await input.pressSequentially('3');await expect(input).toHaveValue('root(x+1;3)');
 if(!graph){
  if(width>=600){await input.fill('');await page.locator('#recent-buttons button').filter({hasText:/^root$/}).click();await expect(input).toHaveValue('root(;)');await expect(input).toHaveJSProperty('selectionStart',5);}
  await input.fill('');await choose('rand');await expect(input).toHaveValue('rand()');await expect(input).toHaveJSProperty('selectionStart',6);
 }
});
