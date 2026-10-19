const { artifactPath } = require('./artifacts');
const {test,expect}=require('@playwright/test');
test('row editor and palette target only their own curve and retain custom colors',async({page})=>{
 await page.setViewportSize({width:390,height:844});await page.goto('/graph?lang=en');await page.locator('#graph-add').click();
 const rows=page.locator('.graph-curve'),second=rows.nth(1);await second.locator('.graph-edit').click();
 await expect(page.locator('#graph-library-target')).toHaveText('Function 2 · y =');await page.locator('#graph-library-expression').fill('cos(x)');await page.locator('#graph-library-close').click();
 await expect(second.locator('input:not([type=color])')).toHaveValue('cos(x)');await expect(page.locator('#graph-expression')).toHaveValue('sin(x)');
 await second.locator('.graph-color-swatch').click();await expect(page.locator('#graph-color-dialog')).toBeVisible();
 await page.locator('[data-color="#ff6b88"]').click();await expect(second.locator('input[type=color]')).toHaveValue('#ff6b88');
 await expect(page.locator('[data-color="#ff6b88"]')).toHaveAttribute('aria-pressed','true');
 await page.locator('#graph-custom-color').evaluate(e=>{e.value='#123abc';e.dispatchEvent(new Event('input',{bubbles:true}));});await page.keyboard.press('Escape');
 await expect(second.locator('.graph-color-swatch')).toBeFocused();await expect(rows.first().locator('input[type=color]')).toHaveValue('#dfafff');
 await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toHaveCount(2);
 await expect(page.locator('path.graph-line').last()).toHaveCSS('stroke','rgb(18, 58, 188)');
 await page.reload();await expect(page.locator('.graph-curve input[type=color]').last()).toHaveValue('#123abc');
 await page.screenshot({path:artifactPath('graph-mobile-polish.png'),fullPage:true});
});
test('landscape expanded canvas fills screen with dismissible controls and a direct exit',async({page})=>{
 await page.setViewportSize({width:844,height:390});await page.goto('/graph?lang=sk');await page.locator('#graph-submit').click();await expect(page.locator('path.graph-line')).toBeVisible();await page.locator('#graph-expand').click();
 await expect(page.locator('#graph-reset')).not.toBeVisible();await expect(page.locator('#graph-legend')).not.toBeVisible();await expect(page.locator('#graph-expanded-back')).toBeVisible();
 const geometry=await page.locator('#graph-plot').evaluate(e=>{const r=e.getBoundingClientRect();return {height:r.height,width:r.width,vh:innerHeight,vw:innerWidth};});expect(geometry.height).toBeGreaterThanOrEqual(geometry.vh-1);expect(geometry.width).toBeGreaterThanOrEqual(geometry.vw-1);
 await page.locator('#graph-tools-toggle').click();await expect(page.locator('#graph-reset')).toBeVisible();await expect(page.locator('#graph-legend')).toBeVisible();
 await page.locator('#graph-reset').click();await expect(page.locator('#graph-plot')).toHaveAttribute('data-xmin','-10');
 await page.locator('#graph-plot').click({position:{x:300,y:240}});await expect(page.locator('#graph-reset')).not.toBeVisible();await expect(page.locator('#graph-tools-toggle')).toHaveAttribute('aria-expanded','false');
 await page.screenshot({path:artifactPath('graph-landscape-polish.png')});await page.locator('#graph-expanded-back').click();await expect(page.locator('#graph-expanded')).not.toBeVisible();await expect(page.locator('#graph-expand')).toBeFocused();
 await page.locator('#graph-expand').click();await page.keyboard.press('Escape');await expect(page.locator('#graph-expanded')).not.toBeVisible();
});
test('informational curve notes stay quiet while invalid expressions remain visible',async({page})=>{
 await page.goto('/graph?lang=en');await page.locator('#graph-expression').fill('1000');await page.locator('#graph-submit').click();await expect(page.locator('.graph-workspace')).not.toHaveAttribute('aria-busy','true');await expect(page.locator('#graph-status')).not.toBeVisible();
 await page.locator('#graph-add').click();await page.locator('.graph-curve input:not([type=color])').last().fill('x+y');await page.locator('#graph-submit').click();await expect(page.locator('.graph-curve-status.error')).toBeVisible();await expect(page.locator('.graph-curve-status.error')).toContainText('one variable');
 await page.locator('#graph-expand').click();await expect(page.locator('#graph-status.error')).toBeVisible();
});
