const {chromium,expect}=require('../browser/node_modules/@playwright/test');
const fs=require('node:fs');
(async()=>{
    const baseURL=process.env.NF_UI_BASE_URL;
    if(!baseURL?.startsWith('https://localhost:'))throw Error('Disposable local HTTPS gateway required');
    const browser=await chromium.launch(),contexts=[];let release;
    const fresh=async()=>{
        const context=await browser.newContext({baseURL,ignoreHTTPSErrors:true,
            extraHTTPHeaders:{'Cf-Connecting-IP':'192.0.2.47'},viewport:{width:1440,height:900}});
        contexts.push(context);return context.newPage();
    };
    const login=async(page,user)=>{
        await page.goto('/login?lang=en');await page.getByLabel('Username',{exact:true}).fill(user);
        await page.getByLabel('Password',{exact:true}).fill('test-only');
        await page.getByRole('button',{name:'Sign in',exact:true}).click();await expect(page.locator('#account-name')).toHaveText(user);
    };
    const dialog=page=>page.locator('.account-workspaces-dialog');
    const action=(page,what)=>dialog(page).locator(`[data-action="${what}"]`).click();
    const status=page=>page.locator('#account-workspaces-status');
    const close=page=>dialog(page).locator('[data-close]').click();
    const open=async(page)=>{await page.locator('#account-workspaces').click();await expect(status(page)).toContainText(/Select a saved|No saved/);};
    const choose=async(page,name)=>page.locator('#account-workspaces-list').selectOption({label:name});
    const named=async(page,name)=>{await page.locator('#account-workspace-name').fill(name);await action(page,'create');await expect(status(page)).toHaveText('Saved.');};
    try{
        const first=await fresh(),second=await fresh(),other=await fresh();
        await login(first,'container-check');await login(second,'container-check');await login(other,'container-other');
        await first.evaluate(async()=>{
            const session=await (await fetch('/api/account/session')).json();
            const listing=await (await fetch('/api/account/workspaces')).json();
            for(const row of listing.items.filter(r=>/^(Saved |Renamed |Copy |Offline )/.test(r.name)))
                await fetch('/api/account/workspaces/'+row.id,{method:'POST',headers:{'Content-Type':'application/json','X-CSRFToken':session.csrf_token},
                    body:JSON.stringify({user_id:session.user.id,action:'delete',expected_revision:row.revision})});
        });
        const checks=[
            {tool:'calculator',path:'/',selector:'#expression',value:'123456789012345678901234567890+1'},
            {tool:'graph',path:'/graph',selector:'#graph-expression',value:'cos(x)+2'},
            {tool:'solve',path:'/solve',selector:'#equation-expression',value:'x^2=9'},
            {tool:'matrices',path:'/matrices',selector:'#matrix-a input',value:'12345678901234567890'}
        ];
        const calls=[];
        second.on('request',r=>{if(/^\/api\/(evaluate|graph|equations|matrix)/.test(new URL(r.url()).pathname))calls.push(r.url());});
        for(const check of checks){
            await first.goto(check.path+'?lang=en');
            if(check.tool==='solve')await first.locator('#equation-mode').selectOption('expression');
            await first.locator(check.selector).first().fill(check.value);
            if(check.tool==='calculator'){
                await first.locator('#precision-mode').selectOption('custom');await first.locator('#precision').fill('42');
                await first.locator('[data-angle="deg"]').click();
            }
            if(check.tool==='calculator'){
                await first.locator('#precision').fill('');await open(first);
                await first.locator('#account-workspace-name').fill('Invalid precision');await action(first,'create');
                await expect(status(first)).toContainText('Inputs are unsupported');await close(first);
                await expect(first.locator('#precision')).toHaveValue('');await first.locator('#precision').fill('42');
            }
            await open(first);await named(first,'Saved '+check.tool);await close(first);
            await second.goto(check.path+'?lang=en');await open(second);await choose(second,'Saved '+check.tool);
            const before=calls.length;await action(second,'load');await expect(status(second)).toHaveText('Opened without calculating.');
            await expect(second.locator(check.selector).first()).toHaveValue(check.value);
            await second.waitForTimeout(2300);expect(calls.length).toBe(before);
            if(check.tool==='calculator'){
                await expect(second.locator('#precision')).toHaveValue('42');
                await expect(second.locator('[data-angle="deg"]')).toHaveAttribute('aria-pressed','true');
            }
            await close(second);
            if(check.tool==='calculator'){
                const beforeNavigation=calls.length;
                await second.locator('.primary-nav a[href="/graph?lang=en"]').click();
                await second.locator('.primary-nav a[href="/?lang=en"]').click();
                await expect(second.locator('#expression')).toHaveValue(check.value);
                await expect(second.locator('#precision')).toHaveValue('42');
                await second.waitForTimeout(2300);expect(calls.length).toBe(beforeNavigation);
            }
            // Another account sees neither the title nor the input payload.
            await other.goto(check.path+'?lang=en');await open(other);
            await expect(other.locator('#account-workspaces-list option')).toHaveCount(0);await close(other);
        }
        // Explicit update, rename, duplicate, delete; stale revisions never replace the draft.
        await first.goto('/solve?lang=en');await second.goto('/solve?lang=en');
        await open(first);await choose(first,'Saved solve');await close(first);
        await second.locator('#equation-expression').fill('x=7');await open(second);await choose(second,'Saved solve');
        second.once('dialog',d=>d.accept());await action(second,'update');await expect(status(second)).toHaveText('Saved.');
        await first.locator('#account-workspaces').click();await expect(status(first)).toContainText('Select a saved');
        await choose(first,'Saved solve'); // Snapshot revision 2 in first browser.
        await second.locator('#account-workspace-name').fill('Renamed solve');await action(second,'rename');await expect(status(second)).toHaveText('Saved.');
        first.once('dialog',d=>d.accept());await action(first,'update');await expect(status(first)).toContainText('saved version changed');
        await expect(first.locator('#equation-expression')).toHaveValue('x^2=9');
        await action(first,'refresh');await choose(first,'Renamed solve');await action(first,'load');
        await expect(first.locator('#equation-expression')).toHaveValue('x=7');
        await first.locator('#account-workspace-name').fill('Copy solve');await action(first,'duplicate');await expect(status(first)).toHaveText('Saved.');
        first.once('dialog',d=>d.accept());await action(first,'delete');await expect(status(first)).toHaveText('Saved.');
        await expect(first.locator('#account-workspaces-list option')).toHaveCount(1);
        await close(second);await close(first);
        // Failed saves preserve input/name, and retry is explicit.
        await first.locator('#equation-expression').fill('x=11');await open(first);
        await first.locator('#account-workspace-name').fill('Offline draft');
        await first.route('**/api/account/workspaces',r=>r.request().method()==='POST'?r.abort():r.continue());
        await action(first,'create');await expect(status(first)).toContainText('Could not connect');
        await expect(first.locator('#account-workspace-name')).toHaveValue('Offline draft');
        await expect(first.locator('#equation-expression')).toHaveValue('x=11');
        await first.unroute('**/api/account/workspaces');await action(first,'create');await expect(status(first)).toHaveText('Saved.');
        // Delayed loads cannot replace input edited while the server was responding.
        let fetched=false;const gate=new Promise(resolve=>release=resolve);
        await first.route('**/api/account/workspaces/*',async route=>{
            if(route.request().method()!=='GET')return route.continue();
            const response=await route.fetch();fetched=true;await gate;await route.fulfill({response}).catch(()=>{});
        });
        await choose(first,'Renamed solve');await action(first,'load');await expect.poll(()=>fetched).toBe(true);
        await first.evaluate(()=>{const field=document.querySelector('#equation-expression');field.value='x=99';field.dispatchEvent(new Event('input',{bubbles:true}));});
        release();await expect(status(first)).toContainText('Inputs changed while loading');
        await expect(first.locator('#equation-expression')).toHaveValue('x=99');await first.unrouteAll({behavior:'wait'});
        // A held response after switching accounts must not expose the old owner's title or inputs.
        fetched=false;const held=new Promise(resolve=>release=resolve);
        await first.route('**/api/account/workspaces/*',async route=>{const response=await route.fetch();fetched=true;await held;await route.fulfill({response}).catch(()=>{});});
        await action(first,'load');await expect.poll(()=>fetched).toBe(true);
        const tab=await first.context().newPage();await tab.goto('/login?lang=en');await tab.getByRole('button',{name:'Sign out',exact:true}).click();
        await expect(tab.locator('#account-form')).toBeVisible();
        await tab.getByLabel('Username',{exact:true}).fill('container-other');await tab.getByLabel('Password',{exact:true}).fill('test-only');
        await tab.getByRole('button',{name:'Sign in',exact:true}).click();await expect(tab.locator('#account-name')).toHaveText('container-other');
        await expect(first.locator('#account-workspaces-list option')).toHaveCount(0);release();await first.unrouteAll({behavior:'wait'});
        await expect(first.locator('#equation-expression')).toHaveValue('x=99');await expect(first.locator('#account-workspace-name')).toHaveValue('');
        await close(first);
        // SK labels and mobile modal remain reachable without horizontal page overflow.
        await second.goto('/matrices?lang=sk');await second.setViewportSize({width:390,height:844});
        await second.locator('#account-workspaces').click();
        await expect(dialog(second)).toBeVisible();await expect(dialog(second).locator('h2')).toHaveText('Pracovné priestory');
        expect(await second.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
        fs.mkdirSync('build/browser-artifacts/workspaces',{recursive:true});await second.screenshot({path:'build/browser-artifacts/workspaces/mobile.png',fullPage:true});
        await close(second);await second.setViewportSize({width:1440,height:900});await second.goto('/matrices?lang=en');await open(second);
        await second.screenshot({path:'build/browser-artifacts/workspaces/desktop.png',fullPage:true});await close(second);
        // SPA navigation disposes the old dialog and mounts exactly one adapter.
        await second.locator('.primary-nav a[href="/graph?lang=en"]').click();await expect(second.locator('#graph-form')).toBeVisible();
        await expect(second.locator('#account-workspaces')).toHaveCount(1);await expect(second.locator('.account-workspaces-dialog')).toHaveCount(1);
        await open(second);await choose(second,'Saved graph');await action(second,'load');await expect(status(second)).toHaveText('Opened without calculating.');
        console.log('Workspaces UI: four tools/second browser, manual CRUD, owner isolation, CAS, offline retry, edited draft/delayed account switch, SK/mobile and SPA passed.');
    }finally{release?.();for(const context of contexts)await context.close();await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
