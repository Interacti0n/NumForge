// Manual account saves contain inputs only; restoring never submits a calculation.
window.numforgeWorkspaces = (() => {
    const clone = value => JSON.parse(JSON.stringify(value));
    function mount(tool) {
        const accounts = window.numforgePreferences;
        let host = document.querySelector('.expression-card > .section-heading, .equation-entry > .workbench-panel-heading, .matrix-entry > .section-heading');
        const graphTitle = document.querySelector('.graph-controls > h1');
        let graphHeading = null;
        if (graphTitle) {graphHeading=document.createElement('div');graphHeading.className='account-workspaces-tool-heading';graphTitle.before(graphHeading);graphHeading.append(graphTitle);host=graphHeading;}
        if (!accounts || !host) return () => {};
        const en = document.documentElement.lang === 'en', t = (sk, english) => en ? english : sk;
        const lifecycle = new AbortController(), options = {signal:lifecycle.signal};
        const button = document.createElement('button'), dialog = document.createElement('dialog');
        button.type = 'button'; button.className = 'account-workspaces-button'; button.id = 'account-workspaces';
        button.textContent = t('Úlohy v účte', 'Account tasks'); button.setAttribute('aria-haspopup', 'dialog');
        dialog.className = 'account-workspaces-dialog'; dialog.setAttribute('aria-labelledby','account-workspaces-title');
        // All dynamic account names and titles are inserted with textContent/value.
        dialog.innerHTML = `<div class="account-workspaces-heading"><h2 id="account-workspaces-title"></h2><button type="button" data-close>×</button></div>
            <p data-note></p><a data-login></a><section data-content hidden>
            <label><span data-list-label></span><select id="account-workspaces-list" size="5"></select></label>
            <label><span data-name-label></span><input id="account-workspace-name" maxlength="80" autocomplete="off"></label>
            <div class="account-workspaces-actions"><button type="button" data-action="create"></button><button type="button" data-action="load"></button>
            <button type="button" data-action="update"></button><button type="button" data-action="rename"></button>
            <button type="button" data-action="duplicate"></button><button type="button" data-action="delete"></button><button type="button" data-action="refresh"></button></div>
            </section><p id="account-workspaces-status" role="status" aria-live="polite"></p>`;
        host.append(button);host.classList.add('account-workspaces-tool-heading');document.body.append(dialog);
        const $ = selector => dialog.querySelector(selector), list = $('#account-workspaces-list'), name = $('#account-workspace-name');
        $('#account-workspaces-title').textContent = t('Pracovné priestory','Workspaces');
        $('[data-close]').setAttribute('aria-label', t('Zavrieť', 'Close'));
        $('[data-note]').textContent = t('Uloží zadanie a nastavenia tohto nástroja do účtu. Otvorenie nič nepočíta. Výsledky, premenné ani história sedenia sa neprenášajú.',
            'Save this tool’s inputs and settings to your account. Opening does not calculate. Results, variables and session history are not transferred.');
        $('[data-list-label]').textContent = t('Uložené v tomto nástroji', 'Saved for this tool');
        $('[data-name-label]').textContent = t('Názov', 'Name');
        $('[data-login]').textContent = t('Prihlásiť sa na ukladanie do účtu', 'Sign in to save to your account');
        $('[data-login]').href = '/login?lang=' + (en ? 'en' : 'sk');
        const labels = {create:t('Uložiť ako nové', 'Save as new'),load:t('Otvoriť', 'Open'),update:t('Nahradiť zadaním', 'Replace with current inputs'),
            rename:t('Premenovať', 'Rename'),duplicate:t('Duplikovať', 'Duplicate'),delete:t('Vymazať', 'Delete'),refresh:t('Obnoviť zoznam', 'Refresh list')};
        for (const action in labels) $(`[data-action="${action}"]`).textContent = labels[action];
        let user = null, known = false, generation = 0, busy = false, selected = null, items = [], stale = false;
        const message = (value, error = false) => {$('#account-workspaces-status').textContent=value; $('#account-workspaces-status').classList.toggle('error',error);};
        function controls() {
            $('[data-content]').hidden = !known || !user; $('[data-login]').hidden = Boolean(known && user);
            list.disabled = name.disabled = busy;
            for (const b of dialog.querySelectorAll('[data-action]')) b.disabled = busy || !known || !user ||
                (!['create','refresh'].includes(b.dataset.action) && (!selected || stale));
        }
        function render() {
            list.replaceChildren();
            for (const item of items) {const option=document.createElement('option');option.value=item.id;option.textContent=item.name;list.append(option);}
            list.selectedIndex = selected ? items.findIndex(item=>item.id===selected.id) : -1;
            controls();
        }
        function failed(error) {
            if (error.body?.error === 'revision_conflict') {stale=true;controls();}
            const code = error.body?.error || error.message;
            const text = code === 'revision_conflict' ? t('Uložená verzia sa zmenila. Zadanie zostalo zachované. Obnov zoznam a otvor aktuálnu verziu alebo ulož zadanie ako nové.',
                'The saved version changed. Your inputs were preserved. Refresh the list and open the current version, or save your inputs as new.') :
                code === 'quota_exceeded' ? t('Limit účtu: 100 úloh alebo 1 MB. Odstráň nepotrebné úlohy.', 'Account limit: 100 workspaces or 1 MB. Remove unused workspaces.') :
                ['invalid_workspace','workspace_too_large','request_too_large'].includes(code) ? t('Zadanie nie je podporované alebo prekračuje limit 100 kB.', 'Inputs are unsupported or exceed the 100 kB limit.') :
                code === 'invalid_name' ? t('Zadaj názov s 1 až 80 znakmi.', 'Enter a name of 1–80 characters.') :
                code === 'calculation_pending' ? t('Najprv dokonči alebo potvrď rozpracovaný výpočet.', 'Finish or confirm the pending calculation first.') :
                ['account_changed','authentication_required'].includes(code) || error.status === 401 ? t('Účet sa zmenil. Znovu otvor pracovné priestory.', 'The account changed. Reopen workspaces.') :
                code === 'not_found' ? t('Úloha už neexistuje. Obnov zoznam.', 'This workspace no longer exists. Refresh the list.') :
                t('Spojenie sa nepodarilo. Zadanie aj názov zostali zachované. Pred opakovaním uloženia obnov zoznam.',
                    'Could not connect. Your inputs and name were preserved. Refresh the list before retrying a save.');
            message(text, true);
        }
        async function refresh(id) {
            const reply = await accounts.workspace();
            if(id!==generation || lifecycle.signal.aborted || !dialog.open)return;
            items = reply.items.filter(item=>item.tool===tool.kind); selected=null; stale=false; render();
            message(items.length ? t('Vyber uloženú úlohu.', 'Select a saved workspace.') : t('Zatiaľ tu nie sú uložené úlohy.', 'No saved workspaces yet.'));
        }
        async function run(action) {
            if (busy || !known || !user) return;
            const id=++generation, selection=selected && {...selected};
            if (action === 'delete' && !window.confirm(t('Vymazať úlohu „','Delete workspace “') + selection.name + '”?')) return;
            if (action === 'update' && !window.confirm(t('Nahradiť uložené zadanie úlohy „','Replace the saved inputs of “') + selection.name + '”?')) return;
            // Capture the current task before any network waits; no later reply may replace newer edits.
            const before=JSON.stringify(tool.read());
            busy=true;controls();message(t('Spracúvam…','Working…'));
            try {
                if (action === 'refresh') await refresh(id);
                else if (action === 'load') {
                    const row=await accounts.workspace('/'+selection.id);
                    if (id!==generation || lifecycle.signal.aborted) return;
                    if (JSON.stringify(tool.read()) !== before) throw Error('draft_changed');
                    if (row.data?.version!==1 || row.data.tool!==tool.kind || !tool.valid(row.data.inputs)) throw Error('invalid_workspace');
                    accounts.protectDraft(); tool.apply(clone(row.data.inputs)); selected=row;name.value=row.name;
                    message(t('Otvorené bez výpočtu.','Opened without calculating.'));
                } else {
                    const payload = action === 'create' ? {name:name.value,data:{version:1,tool:tool.kind,inputs:JSON.parse(before)}} :
                        {action,expected_revision:selection.revision,...(['rename','duplicate'].includes(action)?{name:name.value}:{}),
                            ...(action==='update'?{data:{version:1,tool:tool.kind,inputs:JSON.parse(before)}}:{})};
                    const row=await accounts.workspace(action==='create'?'':'/'+selection.id,payload);
                    if(id!==generation || lifecycle.signal.aborted)return;
                    if(action==='delete') {items=items.filter(item=>item.id!==selection.id);selected=null;name.value='';}
                    else {items=[row,...items.filter(item=>item.id!==row.id)];selected=row;name.value=row.name;}
                    stale=false;render();message(t('Uložené.','Saved.'));
                }
            } catch(error) {if(id===generation&&!lifecycle.signal.aborted){
                if(error.message==='draft_changed')message(t('Medzitým sa zadanie zmenilo. Otvorenie zopakuj, ak ho chceš nahradiť.','Inputs changed while loading. Open again if you want to replace them.'),true);
                else failed(error);
            }} finally {if(id===generation){busy=false;controls();}}
        }
        list.addEventListener('change',()=>{selected=items.find(item=>item.id===list.value)||null;stale=false;if(selected)name.value=selected.name;controls();},options);
        dialog.addEventListener('click',event=>{const action=event.target.closest('[data-action]')?.dataset.action;if(action)run(action);},options);
        $('[data-close]').addEventListener('click',()=>dialog.close(),options);
        dialog.addEventListener('close',()=>{++generation;busy=false;controls();},options);
        button.addEventListener('click',async()=>{
            dialog.showModal();busy=true;controls();message(t('Overujem účet…','Verifying account…'));
            try {
                await accounts.refresh();
                if(lifecycle.signal.aborted||!dialog.open)return;
                busy=false;controls();
                if(user&&known)await run('refresh');else message(t('Pre ukladanie sa prihlás.','Sign in to save workspaces.'));
            } catch {if(!lifecycle.signal.aborted&&dialog.open){busy=false;controls();message(t('Účet sa nepodarilo overiť. Skús to znova.','Could not verify the account. Try again.'),true);}}
        },options);
        const unsubscribe=accounts.subscribe(state=>{
            const changed=user?.id!==state.user?.id || (known&&!state.known);
            user=state.user;known=state.known;
            if(changed){++generation;busy=false;items=[];selected=null;stale=false;name.value='';message('');render();}
            controls();
        });
        return ()=>{++generation;lifecycle.abort();unsubscribe();dialog.close();dialog.remove();button.remove();host.classList.remove('account-workspaces-tool-heading');if(graphHeading){graphHeading.before(graphTitle);graphHeading.remove();}};
    }
    return {mount};
})();
