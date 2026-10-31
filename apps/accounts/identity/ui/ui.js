(() => {
    const copy = JSON.parse(document.getElementById('account-copy').textContent);
    const form = document.getElementById('account-form');
    const signed = document.getElementById('account-signed');
    const status = document.getElementById('account-status');
    const retry = document.getElementById('account-retry');
    const password = document.getElementById('account-password');
    const username = document.getElementById('account-username');
    const preferences = window.numforgePreferences;
    let token = '', pending = false, userId = null;
    function message(text, error = false) { status.hidden = !text; status.textContent = text; status.dataset.error = String(error); }
    function apply(data) {
        if ((data.user?.id || null) !== userId) {
            username.value = ''; password.value = ''; document.getElementById('account-remember').checked = false;
        }
        userId = data.user?.id || null;
        token = data.csrf_token;
        form.hidden = Boolean(data.user); signed.hidden = !data.user; retry.hidden = true;
        document.getElementById('account-name').textContent = data.user?.username || '';
        message(data.user ? '' : copy.lead);
    }
    async function request(path, data) {
        const response = await fetch('/api/account/' + path, {method:data ? 'POST' : 'GET', credentials:'same-origin', cache:'no-store',
            headers:data ? {'Content-Type':'application/json', 'X-CSRFToken':token} : {},
            body:data ? JSON.stringify(data) : undefined, signal:AbortSignal.timeout(15000)});
        const body = await response.json();
        if (!response.ok) throw Object.assign(new Error(), {status:response.status});
        return body;
    }
    async function refresh() {
        if (pending) return;
        if (location.protocol !== 'https:') { message(copy.secure, true); return; }
        pending = true; retry.hidden = true; form.hidden = signed.hidden = true;
        message(copy.checking);
        try { apply(await preferences.refresh()); }
        catch { message(copy.offline, true); retry.hidden = false; }
        finally { pending = false; }
    }
    async function mutate(path, data) {
        if (pending) return;
        pending = true;
        document.querySelectorAll('.account-card button').forEach(button => button.disabled = true);
        message(copy.pending);
        try {
            apply(await request(path, data));
            await preferences.refresh(true); preferences.announce();
        }
        catch (error) {
            if (error.status === 401) message(copy.invalid, true);
            else if (error.status === 429) message(copy.limited, true);
            else if (error.status === 403) { message(copy.csrf, true); form.hidden = signed.hidden = true; retry.hidden = false; }
            else { message(copy.offline, true); form.hidden = signed.hidden = true; retry.hidden = false; }
        }
        finally {
            password.value = ''; password.type = 'password'; document.getElementById('account-show').checked = false;
            pending = false; document.querySelectorAll('.account-card button').forEach(button => button.disabled = false);
        }
    }
    form.addEventListener('submit', event => { event.preventDefault(); mutate('login', {
        username:username.value, password:password.value, remember:document.getElementById('account-remember').checked}); });
    document.getElementById('account-show').addEventListener('change', event => password.type = event.target.checked ? 'text' : 'password');
    document.getElementById('account-logout').addEventListener('click', () => mutate('logout', {}));
    retry.addEventListener('click', refresh);
    document.addEventListener('visibilitychange', () => { if (!document.hidden) refresh(); });
    const settings = document.getElementById('preferences-form');
    const settingsStatus = document.getElementById('preferences-status');
    const reload = document.getElementById('preferences-reload');
    let owner = null, revision = 0, dirty = false, saving = false;
    const fields = ['language','precision_mode','precision','notation','angle','complex_form'];
    const field = name => settings.elements.namedItem(name);
    function settingsMessage(text, error = false) {
        settingsStatus.textContent = text; settingsStatus.dataset.error = String(error);
    }
    function precisionMode() {
        const custom = field('precision_mode').value === 'custom';
        document.getElementById('preferences-places').hidden = !custom;
        field('precision').disabled = !custom;
    }
    function fill(values) { fields.forEach(name => field(name).value = String(values[name])); precisionMode(); }
    function conflict() { settingsMessage(copy.conflict, true); reload.hidden = false; }
    preferences.subscribe(data => {
        if (!pending) {
            if (data.known) apply(data);
            else { form.hidden = signed.hidden = true; retry.hidden = false; }
        }
        const next = data.user?.id ?? null;
        if (next !== owner && (data.known || data.user)) {
            settings.reset(); dirty = false; owner = next; revision = 0;
            settingsMessage(''); reload.hidden = true;
        }
        settings.hidden = !data.known || !data.user || !data.values;
        if (settings.hidden || saving) return;
        if (dirty) { if (data.revision !== revision) conflict(); return; }
        fill(data.values); revision = data.revision; reload.hidden = true;
    });
    settings.addEventListener('input', () => { dirty = true; settingsMessage(copy.unsaved); });
    settings.addEventListener('change', () => { dirty = true; precisionMode(); settingsMessage(copy.unsaved); });
    document.getElementById('preferences-import').addEventListener('click', () => {
        fill(preferences.guest()); dirty = true; settingsMessage(copy.import_note);
    });
    reload.addEventListener('click', async () => {
        dirty = false;
        try { await preferences.refresh(true); settingsMessage(copy.saved); }
        catch { settingsMessage(copy.offline, true); }
    });
    settings.addEventListener('submit', async event => {
        event.preventDefault();
        if (saving || owner === null || !settings.reportValidity()) return;
        const values = Object.fromEntries(fields.map(name => [name, field(name).value]));
        values.precision = Number(values.precision); values.rounding = 'half_even';
        const originalOwner = owner;
        saving = true; settingsMessage(copy.saving);
        settings.querySelectorAll('input,select,button').forEach(control => control.disabled = true);
        try {
            const saved = await preferences.save(values, revision);
            if (owner !== originalOwner) return;
            dirty = false; revision = saved.revision; fill(saved.values);
            reload.hidden = true; settingsMessage(copy.saved);
        } catch (error) {
            if (owner !== originalOwner) return;
            if (error.body?.error === 'revision_conflict') conflict();
            else if ([401,403].includes(error.status) || error.body?.error === 'account_changed') {
                await preferences.refresh(true).catch(() => {});
            } else settingsMessage(error.status === 400 ? copy.invalid_settings : copy.offline, true);
        } finally {
            saving = false; settings.querySelectorAll('input,select,button').forEach(control => control.disabled = false);
            precisionMode();
            if (owner !== originalOwner) preferences.refresh().catch(() => {});
        }
    });
    refresh();
})();
