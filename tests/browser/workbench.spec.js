const {test,expect}=require('@playwright/test');
const {artifactPath}=require('./artifacts');

for(const [width,height] of [[1280,720],[1024,768],[1280,800],[1440,900]])test(`calculator session aligns with keypad and scrolls independently ${width}x${height}`,async({page})=>{
 await page.setViewportSize({width,height});await page.goto('/?lang=sk');
 const session=page.locator('.session-panel'),keyboard=page.locator('.keypad-card');
 const bounds=async()=>({session:await session.boundingBox(),keyboard:await keyboard.boundingBox()});
 const initial=await bounds();
 expect(Math.abs(initial.session.y-initial.keyboard.y)).toBeLessThan(2);
 expect(Math.abs(initial.session.height-initial.keyboard.height)).toBeLessThan(2);
 for(let i=0;i<18;i++){
  await page.locator('#expression').fill(String(i));await page.locator('#expression').press('Enter');
  await expect(page.locator('#result')).toHaveAttribute('data-raw-result',String(i));
 }
 await expect(page.locator('#history-list li')).toHaveCount(16);
 expect(await bounds()).toEqual(initial);
 await page.locator('#history-list').evaluate(e=>e.scrollTop=e.scrollHeight);
 expect(await page.locator('#history-list').evaluate(e=>e.scrollTop)).toBeGreaterThan(0);
 expect(await page.locator('.sidebar').evaluate(e=>e.scrollTop)).toBe(0);
 await page.locator('#function-search').fill('no-such-function');expect(await bounds()).toEqual(initial);
 expect(await page.evaluate(()=>document.documentElement.scrollHeight)).toBe(height);
 await page.screenshot({path:artifactPath(`calculator-workbench-${width}.png`)});
});

test('desktop graph panels share rows and the inline library edits the active curve',async({page})=>{
 await page.setViewportSize({width:1280,height:800});await page.goto('/graph?lang=sk');
 const control=await page.locator('.graph-controls').boundingBox(),plot=await page.locator('.graph-result').boundingBox();
 const history=await page.locator('.graph-history').boundingBox(),library=await page.locator('.graph-library-panel').boundingBox();
 expect(Math.abs(control.y-plot.y)).toBeLessThan(2);expect(Math.abs(control.height-plot.height)).toBeLessThan(2);
 expect(Math.abs(history.y-library.y)).toBeLessThan(2);expect(Math.abs(history.height-library.height)).toBeLessThan(2);
 expect(library.x).toBe(plot.x);expect(library.width).toBe(plot.width);
 await expect(page.locator('#graph-library-dock #graph-functions button').first()).toBeVisible();
 await page.locator('#graph-add').click();const second=page.locator('.graph-curve').last().locator('input:not([type=color])');
 await second.fill('');await page.locator('#function-search').fill('sine');
 await page.locator('#graph-functions button[data-function=sin]').click();await second.press('x');
 await expect(second).toHaveValue('sin(x)');await expect(page.locator('#graph-expression')).toHaveValue('sin(x)');
 await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toHaveCount(2);
 await expect(page.locator('#graph-history-list button')).toHaveCount(1);
 expect(await page.evaluate(()=>document.documentElement.scrollHeight)).toBe(800);
 await page.screenshot({path:artifactPath('graph-workbench-desktop.png')});
 await page.locator('.graph-edit').last().click();await expect(page.locator('#graph-library-dialog')).toBeVisible();
 await expect(page.locator('#graph-library-dialog #function-search')).toBeVisible();
 await page.locator('#graph-library-close').click();await expect(page.locator('#graph-library-dock #function-search')).toBeVisible();
});

for(const width of [390,1280])test(`graph history restores function sets and survives navigation ${width}`,async({page})=>{
 await page.setViewportSize({width,height:844});await page.goto('/graph?lang=en');
 await page.locator('#graph-expression').fill('x');await page.locator('#graph-add').click();
 await page.locator('.graph-curve').last().locator('input:not([type=color])').fill('2*x');
 await page.locator('#graph-submit').click();await expect(page.locator('#graph-history-list button')).toHaveCount(1);
 const originalColor=await page.locator('#graph-color').inputValue();
 await page.locator('#graph-expression').fill('x^2');await page.locator('#graph-submit').click();
 await expect(page.locator('#graph-history-list button')).toHaveCount(2);
 await page.locator('#graph-history-list button').last().click();await expect(page.locator('#graph-expression')).toHaveValue('x');
 await expect(page.locator('path.graph-line')).toHaveCount(2);await expect(page.locator('#graph-color')).toHaveValue(originalColor);
 await expect(page.locator('#graph-history-list button')).toHaveCount(2);
 await page.reload();await expect(page.locator('#graph-history-list button')).toHaveCount(2);
 await page.locator('#graph-expression').fill('unknown(x)');await page.locator('#graph-submit').click();
 await expect(page.locator('#graph-status')).toHaveClass(/error/);await expect(page.locator('#graph-history-list button')).toHaveCount(2);
 await page.locator('#graph-history-clear').click();await expect(page.locator('#graph-history-list button')).toHaveCount(0);
 await expect(page.locator('#graph-history-empty')).toBeVisible();
});

test('graph history is bounded and restores hidden curves, colors and camera',async({page})=>{
 await page.goto('/graph?lang=en');await page.locator('#graph-add').click();
 const rows=page.locator('.graph-curve');await rows.last().locator('input:not([type=color])').fill('2*x');
 await rows.first().locator('.graph-visibility').click();
 await rows.last().locator('input[type=color]').evaluate(e=>{e.value='#ff1234';e.dispatchEvent(new Event('input',{bubbles:true}));});
 await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toHaveCount(1);
 const svg=page.locator('#graph-plot');await svg.press('+');
 await page.locator('#graph-submit').click();await expect(page.locator('#graph-history-list button')).toHaveCount(2);
 const camera=await svg.getAttribute('data-xmin');
 await page.locator('#graph-history-list button').first().click();
 await expect(rows.first().locator('.graph-visibility')).toHaveAttribute('aria-pressed','false');
 await expect(rows.last().locator('input[type=color]')).toHaveValue('#ff1234');
 await expect(svg).toHaveAttribute('data-xmin',camera);await expect(page.locator('#graph-history-list button')).toHaveCount(2);
 for(let i=0;i<9;i++){
  await rows.last().locator('input:not([type=color])').fill('x+'+i);await page.locator('#graph-submit').click();
  await expect(page.locator('#graph-history-list strong').first()).toContainText('x+'+i);
 }
 await expect(page.locator('#graph-history-list button')).toHaveCount(8);
 await page.reload();await expect(page.locator('#graph-history-list button')).toHaveCount(8);
});
