// Account defaults stay in memory. Guest defaults are a separate, explicit import.
(() => {
    const key = 'numforge-guest-preferences-v1';
    const defaults = () => ({language:document.documentElement.lang === 'en' ? 'en' : 'sk',
        precision_mode:'auto', precision:10, notation:'auto', angle:'rad', rounding:'half_even', complex_form:'cartesian'});
    const valid = v => v && Object.keys(v).length === 7 && ['sk','en'].includes(v.language) &&
        ['auto','full','custom'].includes(v.precision_mode) && Number.isInteger(v.precision) && v.precision >= 0 && v.precision <= 10000 &&
        ['auto','plain','scientific','math','fraction'].includes(v.notation) && ['rad','deg'].includes(v.angle) &&
        v.rounding === 'half_even' && ['cartesian','trig','exp'].includes(v.complex_form);
    let guest = defaults();
    try {
        const saved = JSON.parse(localStorage.getItem(key));
        if (valid(saved)) guest = saved;
        else if (localStorage.getItem('numforge-angle-unit') === 'deg') guest.angle = 'deg';
    } catch (_) {}
    let state = {user:null, values:null, revision:0, csrf_token:'', known:location.protocol !== 'https:'};
    let sequence = 0, refreshPromise = null, tool = null, edited = false, interacted = false;
    const listeners = new Set();
    const channel = typeof BroadcastChannel === 'function' ? new BroadcastChannel('numforge-account') : null;
    function storeGuest(values) {
        guest = {...values};
        try { localStorage.setItem(key, JSON.stringify(guest)); } catch (_) {}
    }
    const publish = () => listeners.forEach(listener => listener(state));
    const applyTool = () => { if (tool && !edited) tool.apply(state.values || guest); };
    function update(next) {
        const changed = (state.user?.id ?? null) !== (next.user?.id ?? null);
        if (!changed && !state.known && next.known && !next.user && edited && tool) {
            const values = tool.read();
            if (valid(values)) storeGuest(values);
        }
        state = next;
        if (changed) { edited = false; tool?.apply(guest); }
        applyTool(); publish();
        const language = (state.values || guest).language;
        if (state.known && !interacted && !new URL(location.href).searchParams.has('lang') &&
            language !== document.documentElement.lang) {
            const destination = new URL(location.href); destination.searchParams.set('lang', language);
            location.replace(destination);
        }
    }
    async function request(route, data, token, signal) {
        const response = await fetch('/api/account/' + route, {credentials:'same-origin', cache:'no-store',
            method:data ? 'POST' : 'GET', headers:data ? {'Content-Type':'application/json','X-CSRFToken':token} : {},
            body:data ? JSON.stringify(data) : undefined, signal:signal || AbortSignal.timeout(15000)});
        const body = await response.json();
        if (!response.ok) throw Object.assign(new Error(body.error), {status:response.status, body});
        return body;
    }
    function refresh(force = false) {
        if (refreshPromise && !force) return refreshPromise;
        if (location.protocol !== 'https:') return Promise.resolve(state);
        const current = ++sequence;
        const pending = (async () => {
            try {
                const session = await request('session');
                // Clear the previous owner's draft before waiting for the new defaults.
                if (current !== sequence) return state;
                if ((state.user?.id ?? null) !== (session.user?.id ?? null))
                    update({...session, values:null, revision:0, known:false});
                const data = session.user ? await request('preferences') : {...session, values:null, revision:0};
                if (current !== sequence) return state;
                if ((data.user?.id ?? null) !== (session.user?.id ?? null)) throw Error('account_changed');
                if (data.user && (!valid(data.values) || !Number.isSafeInteger(data.revision))) throw Error('invalid_preferences');
                update({...session, ...data, known:true});
                return state;
            } catch (error) {
                if (current === sequence) update({user:null, values:null, revision:0, csrf_token:'', known:false});
                throw error;
            }
        })();
        refreshPromise = pending;
        pending.finally(() => {if (refreshPromise === pending) refreshPromise = null;}).catch(() => {});
        return pending;
    }
    async function save(values, expectedRevision) {
        if (!state.known || !state.user) throw Object.assign(Error('authentication_required'), {status:401});
        const owner = state.user.id, current = sequence;
        const data = await request('preferences', {user_id:owner, expected_revision:expectedRevision, values}, state.csrf_token);
        if (current !== sequence || state.user?.id !== owner || data.user.id !== owner)
            throw Error('account_changed');
        update({...state, ...data, known:true});
        channel?.postMessage('preferences');
        return data;
    }
    window.numforgePreferences = {
        get user() { return state.user; },
        guest: () => ({...guest}), refresh, save, announce: () => channel?.postMessage('changed'),
        async workspace(route = '', data) {
            if (!state.known || !state.user) throw Object.assign(Error('authentication_required'), {status:401});
            const owner = state.user.id, current = sequence;
            const reply = await request('workspaces' + route, data ? {...data,user_id:owner} : undefined, state.csrf_token);
            if (current !== sequence || state.user?.id !== owner || reply.user_id !== owner) throw Error('account_changed');
            return reply;
        },
        protectDraft() { edited = true; },
        subscribe(listener) { listeners.add(listener); listener(state); return () => listeners.delete(listener); },
        mount(next) {
            tool = next; edited = Boolean(next.preserveGuest && !state.user);
            if (edited && state.known && valid(tool.read())) storeGuest(tool.read());
            applyTool();
            return () => {if (tool === next) tool = null;};
        }
    };
    function remember() {
        if (!tool) return;
        edited = true;
        if (state.known && !state.user) {
            const values = tool.read();
            if (valid(values)) {
                storeGuest(values);
            }
        }
    }
    document.addEventListener('input', event => {
        interacted = true;
        if (tool && event.target.closest('#calculator')) edited = true;
        if (event.target.closest('#settings-controls')) remember();
    });
    document.addEventListener('change', event => {if (event.target.closest('#settings-controls')) remember();});
    document.addEventListener('click', event => {
        if (event.target.closest('[data-angle]')) remember();
        if (tool && event.target.closest('#calculator')) edited = true;
        const language = event.target.closest('.language-switch a[lang]')?.lang;
        if (language && state.known && !state.user) {
            storeGuest({...guest,language});
        }
    });
    channel?.addEventListener('message', () => refresh(true).catch(() => {}));
    document.addEventListener('visibilitychange', () => {if (!document.hidden) refresh().catch(() => {});});
    window.addEventListener('pageshow', event => {if (event.persisted) refresh(true).catch(() => {});});
    if (location.protocol === 'https:') refresh().catch(() => {});
})();
