const {test,expect}=require('@playwright/test');
const {artifactPath}=require('./artifacts');
const appearance=locator=>locator.evaluate(e=>{
 const s=getComputedStyle(e);return Object.fromEntries(['fontFamily','fontSize','fontWeight','color','backgroundColor','border','borderRadius','padding','minHeight'].map(key=>[key,s[key]]));
});

for(const lang of ['sk','en'])test(`graph library shares calculator categories and button styles ${lang}`,async({page})=>{
 await page.emulateMedia({reducedMotion:'reduce'});
 await page.setViewportSize({width:1280,height:800});await page.goto('/?lang='+lang);
 const labels=await page.locator('.function-tabs button').allTextContents();
 const categoryStyle=await appearance(page.locator('.function-tabs button').first());
 const functionStyle=await appearance(page.locator('.function-group button[data-function=abs]'));
 await page.goto('/graph?lang='+lang);
 await expect(page.locator('#graph-library-dock #graph-functions button[data-function=abs]')).toBeVisible();
 const categories=page.locator('#graph-function-categories');
 expect(await categories.locator('button:not([data-category=""])').allTextContents()).toEqual(labels);
 expect(await appearance(categories.locator('button[data-category="0"]'))).toEqual(categoryStyle);
 expect(await appearance(page.locator('#graph-functions button[data-function=abs]'))).toEqual(functionStyle);
 await expect(page.locator('.graph-library-panel .graph-library-category')).toBeHidden();
 await expect(page.locator('.graph-library-panel .graph-library-pages')).toBeHidden();
 await categories.locator('button[data-category="7"]').click();
 await expect(page.locator('#graph-functions button[data-insert="π"]')).toBeVisible();
 await expect(page.locator('#graph-functions button[data-function=abs]')).toBeHidden();
 await page.locator('#function-search').fill('square root');
 await expect(categories).toBeHidden();await expect(page.locator('#graph-functions button[data-function=sqrt]')).toBeVisible();
 await page.locator('#graph-add').click();const input=page.locator('.graph-curve').last().locator('input:not([type=color])');
 await input.fill('');await page.locator('#graph-functions button[data-function=sqrt]').click();await input.press('4');
 await expect(input).toHaveValue('sqrt(4)');await expect(page.locator('#graph-expression')).toHaveValue('sin(x)');
 await page.locator('#function-search').fill('');await expect(categories).toBeVisible();
 await expect(categories.locator('button[data-category="0"]')).toHaveAttribute('aria-pressed','true');
 await expect(categories.locator('button[data-category="0"]')).toBeInViewport();
 await page.locator('.graph-library-panel').screenshot({path:artifactPath('graph-library-consistent-'+lang+'.png')});
 await page.locator('.graph-edit').last().click();await expect(page.locator('#graph-library-dialog #graph-function-categories')).toBeVisible();
 await page.locator('#graph-library-close').click();await expect(page.locator('#graph-library-dock #graph-function-categories')).toBeVisible();
 expect(await page.evaluate(()=>document.documentElement.scrollHeight)).toBe(800);
});
