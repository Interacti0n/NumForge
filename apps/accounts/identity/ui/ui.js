(() => {
    const copy = JSON.parse(document.getElementById('account-copy').textContent);
    const form = document.getElementById('account-form');
    const signed = document.getElementById('account-signed');
    const status = document.getElementById('account-status');
    const retry = document.getElementById('account-retry');
    const password = document.getElementById('account-password');
    const username = document.getElementById('account-username');
    let token = '', pending = false, userId = null;
    const channel = typeof BroadcastChannel === 'function' ? new BroadcastChannel('numforge-account') : null;
    function message(text, error = false) { status.textContent = text; status.dataset.error = String(error); }
    function apply(data) {
        if ((data.user?.id || null) !== userId) {
            username.value = ''; password.value = ''; document.getElementById('account-remember').checked = false;
        }
        userId = data.user?.id || null;
        token = data.csrf_token;
        form.hidden = Boolean(data.user); signed.hidden = !data.user; retry.hidden = true;
        document.getElementById('account-name').textContent = data.user?.username || '';
        message(data.user ? copy.signed : copy.lead);
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
        try { apply(await request('session')); }
        catch { message(copy.offline, true); retry.hidden = false; }
        finally { pending = false; }
    }
    async function mutate(path, data) {
        if (pending) return;
        pending = true;
        document.querySelectorAll('.account-card button').forEach(button => button.disabled = true);
        message(copy.pending);
        try { apply(await request(path, data)); channel?.postMessage('changed'); }
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
    channel?.addEventListener('message', refresh);
    document.addEventListener('visibilitychange', () => { if (!document.hidden) refresh(); });
    refresh();
})();
