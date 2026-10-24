// Presentation only: raw result strings remain available for copying and reuse.
window.numforgeDisplay = (() => {
    const group = digits => digits.replace(/\B(?=(\d{3})+(?!\d))/g, '\u202f');
    function number(text) {
        return text.replace(/^([+-]?)(\d+)(\.\d+)?$/, (_, sign, integer, fraction = '') =>
            sign + group(integer) + fraction);
    }
    function render(target, raw, notation = 'auto') {
        raw = typeof raw === 'string' ? raw : '';
        target.dataset.rawResult = raw;
        target.replaceChildren();
        target.removeAttribute('aria-label');
        const scientific = /^([+-]?(?:\d+(?:\.\d*)?|\.\d+))(?:[eE]([+-]?\d+)| × 10\^([+-]?\d+))$/.exec(raw);
        if (scientific && notation !== 'scientific') {
            const exponent = scientific[2] ?? scientific[3];
            const power = document.createElement('sup');
            power.textContent = exponent.replace(/^\+/, '');
            target.append(document.createTextNode(number(scientific[1]) + ' × 10'), power);
            // Superscripts have no textual separator when read as plain text.
            target.setAttribute('aria-label', scientific[1] + ' × 10^' + exponent);
        } else target.textContent = scientific ? raw : number(raw);
    }
    function timing(target, data, english) {
        target.replaceChildren();
        target.open = false;
        const elapsed = data?.elapsed_ms;
        target.hidden = !Number.isSafeInteger(elapsed) || elapsed <= 500;
        if (target.hidden) return;
        const locale = english ? 'en' : 'sk';
        const summary = document.createElement('summary');
        let pinned = false;
        const hovering = () => matchMedia('(hover: hover)').matches && target.matches(':hover');
        target.onpointerenter = () => { if (hovering()) target.open = true; };
        target.onpointerleave = () => { if (!pinned && !summary.matches(':focus-visible')) target.open = false; };
        summary.addEventListener('focus', () => { target.open = true; });
        summary.addEventListener('blur', () => { if (!pinned && !hovering()) target.open = false; });
        summary.addEventListener('click', event => {
            event.preventDefault(); pinned = !pinned; target.open = pinned || hovering();
        });
        summary.addEventListener('keydown', event => {
            if (event.key === 'Escape') { pinned = false; target.open = false; summary.blur(); }
        });
        summary.textContent = (english ? 'Processing time: ' : 'Čas spracovania: ') +
            (elapsed / 1000).toLocaleString(locale, {maximumFractionDigits: 3}) + ' s';
        const detail = document.createElement('div');
        detail.className = 'timing-breakdown';
        const phases = english
            ? {parse:'Parsing', evaluate:'Calculation', convert:'Number conversion', normalize:'Normalization',
                round:'Rounding', compose:'Number notation', format:'Result formatting', serialize:'Response preparation', other:'Other processing'}
            : {parse:'Parsovanie', evaluate:'Výpočet', convert:'Prevod čísla', normalize:'Normalizácia',
                round:'Zaokrúhľovanie', compose:'Zostavenie zápisu', format:'Formátovanie výsledku', serialize:'Príprava odpovede', other:'Ostatné spracovanie'};
        const times = data.timings_us;
        const valid = times && Object.keys(phases).every(key => Number.isSafeInteger(times[key]) && times[key] >= 0);
        const total = valid ? Object.keys(phases).reduce((sum, key) => sum + times[key], 0) : 0;
        if (valid && Math.abs(total / 1000 - elapsed) < 5) {
            const list = document.createElement('dl');
            for (const [key, label] of Object.entries(phases)) {
                const name = document.createElement('dt'); name.textContent = label;
                const value = document.createElement('dd');
                value.textContent = (times[key] / 1000).toLocaleString(locale, {maximumFractionDigits: 3}) + ' ms';
                list.append(name, value);
            }
            detail.append(list);
        } else {
            const unavailable = document.createElement('p');
            unavailable.textContent = english ? 'Phase timings are unavailable.' : 'Rozpis fáz nie je dostupný.';
            detail.append(unavailable);
        }
        const note = document.createElement('p');
        note.textContent = english
            ? 'Server processing, including number preparation. Phases do not overlap. Network transfer, the request queue, preview delay and browser rendering are excluded.'
            : 'Spracovanie na serveri vrátane prípravy čísla. Fázy sa neprekrývajú. Prenos cez sieť, front požiadaviek, čakanie na náhľad a vykreslenie sa nezapočítavajú.';
        detail.append(note);
        target.append(summary, detail);
    }
    return {render, timing};
})();
