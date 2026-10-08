// Keep the shared chrome mounted while replacing the current tool or guide.
const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
let navigating = false;
let pendingNavigation = null;
let licenseDialog = null;
const navToggle = document.querySelector('.nav-toggle');
const pageHeader = document.querySelector('.page-header');
const pageFooter = document.querySelector('.page-footer');
const routes = new Set(['/', '/api', '/graph', '/solve', '/units', '/login', '/register']);

function setMenuOpen(open)
{
    pageHeader?.classList.toggle('menu-open', open);
    navToggle?.setAttribute('aria-expanded', String(open));
}

pageHeader?.addEventListener('click', event => {
    if (event.target.closest('.nav-toggle'))
        setMenuOpen(navToggle.getAttribute('aria-expanded') !== 'true');
});
document.addEventListener('click', event => {
    if (pageHeader && !pageHeader.contains(event.target)) setMenuOpen(false);
});
document.addEventListener('keydown', event => {
    if (event.key === 'Escape' && navToggle?.getAttribute('aria-expanded') === 'true')
    {
        setMenuOpen(false);
        navToggle.focus();
    }
});
window.matchMedia('(min-width: 981px)').addEventListener('change', event => {
    if (event.matches) setMenuOpen(false);
});

function setupUpcoming()
{
    const upcomingPage = document.querySelector('[data-upcoming]');
    if (!upcomingPage) return;
    const page = window.location.pathname.slice(1);
    const language = document.documentElement.lang;
    const messages = {
        sk: {
            graph: ['Grafická kalkulačka', 'Grafy funkcií ti umožnia preskúmať výpočty aj vizuálne.',
                'Čo plánujeme', 'Zadanie funkcie, prehľadný graf a ovládanie rozsahu. Presné správanie ešte navrhneme.', '∿'],
            solve: ['Riešenie rovníc', 'Samostatný nástroj na hľadanie riešení rovníc je v príprave.',
                'Čo plánujeme', 'Najprv určíme podporované typy rovníc, presnosť a spôsob zobrazenia riešení.', 'x='],
            login: ['Prihlásenie', 'Prihlasovanie zatiaľ nie je dostupné.',
                'Čo bude ďalej', 'Po zavedení účtov sa tu bude dať bezpečne prihlásiť a pracovať s vlastným priestorom.', '→', 'Registrácia', '/register?lang=sk'],
            register: ['Vytvoriť účet', 'Registráciu pripravujeme spolu s vlastným priestorom používateľa.',
                'Čo bude ďalej', 'Pred spustením určíme, aké údaje účet potrebuje a ako sa budú bezpečne uchovávať.', '+', 'Prihlásiť sa', '/login?lang=sk']
        },
        en: {
            graph: ['Graphing calculator', 'Function graphs will let you explore calculations visually.',
                'What we plan', 'Function input, a clear plot and range controls. We still need to design the exact behavior.', '∿'],
            solve: ['Equation solver', 'A dedicated tool for finding solutions to equations is in development.',
                'What we plan', 'First we will define supported equation types, precision and how solutions are shown.', 'x='],
            login: ['Sign in', 'Sign-in is not available yet.',
                'What comes next', 'Once accounts are available, you will be able to sign in securely and use your own workspace.', '→', 'Sign up', '/register?lang=en'],
            register: ['Create an account', 'Registration is planned alongside a personal workspace.',
                'What comes next', 'Before launch, we will decide what information an account needs and how it is stored securely.', '+', 'Sign in', '/login?lang=en']
        }
    };
    const content = messages[language]?.[page];
    if (content)
    {
        document.querySelector('#upcoming-title').textContent = content[0];
        document.querySelector('#upcoming-lead').textContent = content[1];
        document.querySelector('#upcoming-detail-title').textContent = content[2];
        document.querySelector('#upcoming-detail').textContent = content[3];
        document.querySelector('#upcoming-symbol').textContent = content[4];
        if (content[5])
        {
            const secondary = document.querySelector('#upcoming-secondary');
            secondary.textContent = content[5];
            secondary.href = content[6];
        }
        document.querySelector(`[data-nav="${page}"]`)?.setAttribute('aria-current', 'page');
        document.querySelectorAll('[data-page-language]').forEach(link => {
            link.href = `/${page}?lang=${link.lang}`;
        });
        document.title = `${content[0]} · NumForge`;
    }
}
setupUpcoming();

async function openLicense()
{
    try
    {
        const response = await fetch('/LICENSE');
        if (!response.ok) throw new Error('License unavailable');
        const license = await response.text();
        if (!licenseDialog)
        {
            licenseDialog = document.createElement('dialog');
            licenseDialog.className = 'license-dialog';
            const heading = document.createElement('div');
            heading.className = 'license-heading';
            const title = document.createElement('h2');
            title.id = 'license-title';
            title.textContent = document.documentElement.lang === 'sk' ? 'MIT licencia' : 'MIT License';
            const close = document.createElement('button');
            close.type = 'button';
            close.textContent = '✕';
            close.setAttribute('aria-label', document.documentElement.lang === 'sk' ? 'Zavrieť licenciu' : 'Close license');
            close.addEventListener('click', () => licenseDialog.close());
            heading.append(title, close);
            const content = document.createElement('pre');
            content.textContent = license;
            licenseDialog.setAttribute('aria-labelledby', title.id);
            licenseDialog.append(heading, content);
            licenseDialog.addEventListener('click', event => {
                if (event.target === licenseDialog) licenseDialog.close();
            });
            document.body.append(licenseDialog);
        }
        licenseDialog.querySelector('#license-title').textContent =
            document.documentElement.lang === 'sk' ? 'MIT licencia' : 'MIT License';
        licenseDialog.querySelector('.license-heading button').setAttribute('aria-label',
            document.documentElement.lang === 'sk' ? 'Zavrieť licenciu' : 'Close license');
        if (!licenseDialog.open) licenseDialog.showModal();
    }
    catch (_) { window.location.assign('/LICENSE'); }
}

function pageContent(shell)
{
    return shell?.querySelector(':scope > .workspace, :scope > .guide-layout, :scope > .upcoming-main, :scope > .units-workspace');
}

function scrollToDestination(hash)
{
    if (!hash) { window.scrollTo(0, 0); return; }
    try { document.getElementById(decodeURIComponent(hash.slice(1)))?.scrollIntoView(); }
    catch (_) {}
}

function syncChrome(current, incoming)
{
    const links = [...current.querySelectorAll('a')];
    const newLinks = [...incoming.querySelectorAll('a')];
    if (links.length !== newLinks.length) throw new Error('Incompatible page chrome');
    links.forEach((link, index) => {
        const next = newLinks[index];
        for (const name of link.getAttributeNames()) link.removeAttribute(name);
        for (const name of next.getAttributeNames())
            link.setAttribute(name, next.getAttribute(name));
        if (link.classList.contains('brand'))
            link.querySelector('.brand-text small').textContent =
                next.querySelector('.brand-text small').textContent;
        else link.textContent = next.textContent;
    });
    for (const selector of ['.primary-nav', '.language-switch', '.footer-links'])
    {
        const label = current.querySelector(selector);
        if (label) label.setAttribute('aria-label', incoming.querySelector(selector).getAttribute('aria-label'));
    }
    const text = current.querySelector(':scope > span');
    if (text) text.textContent = incoming.querySelector(':scope > span').textContent;
}

let activePageKey = window.location.pathname + window.location.search;
async function navigate(destination, addHistory = true)
{
    if (navigating) return;
    navigating = true;
    let stagedStyle = null;
    try
    {
        // Save session UI only after an in-flight deletion has settled.
        await window.numforgePendingVariableDeletion;
        const response = await fetch(destination.href, {headers: {Accept: 'text/html'}});
        if (!response.ok || !response.headers.get('content-type')?.includes('text/html'))
            throw new Error('Page unavailable');
        const incoming = new DOMParser().parseFromString(await response.text(), 'text/html');
        const incomingShell = incoming.querySelector('.calculator-shell, .guide-shell, .units-shell');
        const incomingContent = pageContent(incomingShell);
        const incomingHeader = incomingShell?.querySelector(':scope > .page-header');
        const incomingFooter = incomingShell?.querySelector(':scope > .page-footer');
        const shell = document.querySelector('.calculator-shell, .guide-shell, .units-shell');
        const currentContent = pageContent(shell);
        const currentStyle = document.querySelector('link[rel="stylesheet"]:not([href$="chrome.css"])');
        const incomingStyle = incoming.querySelector('link[rel="stylesheet"]:not([href$="chrome.css"])');
        if (!incomingContent || !incomingHeader || !incomingFooter || !currentContent ||
            !currentStyle || !incomingStyle) throw new Error('Invalid page: ' +
                JSON.stringify({incomingContent: !!incomingContent, incomingHeader: !!incomingHeader,
                    incomingFooter: !!incomingFooter, currentContent: !!currentContent,
                    currentStyle: !!currentStyle, incomingStyle: !!incomingStyle}));

        if (currentStyle.getAttribute('href') !== incomingStyle.getAttribute('href'))
        {
            stagedStyle = document.createElement('link');
            stagedStyle.rel = 'stylesheet';
            stagedStyle.href = incomingStyle.getAttribute('href');
            stagedStyle.media = 'not all';
            document.querySelector('link[href$="chrome.css"]').before(stagedStyle);
            await new Promise((resolve, reject) => {
                stagedStyle.onload = resolve;
                stagedStyle.onerror = reject;
            });
        }

        if (!reducedMotion.matches)
        {
            currentContent.classList.add('content-leaving');
            await new Promise(resolve => window.setTimeout(resolve, 130));
        }
        window.dispatchEvent(new Event('numforge:navigate'));
        window.numforgeDisposeCalculator?.();
        window.numforgeDisposeCalculator = null;
        window.numforgeDisposeUnits?.();
        window.numforgeDisposeUnits = null;

        const replacement = document.importNode(incomingContent, true);
        if (stagedStyle)
        {
            stagedStyle.media = 'all';
            currentStyle.remove();
        }
        shell.className = incomingShell.className;
        currentContent.replaceWith(replacement);
        syncChrome(pageHeader, incomingHeader);
        syncChrome(pageFooter, incomingFooter);
        setMenuOpen(false);
        document.documentElement.lang = incoming.documentElement.lang;
        document.title = incoming.title;
        if (addHistory) history.pushState(null, '', destination.href);
        activePageKey = destination.pathname + destination.search;
        setupUpcoming();
        if (replacement.querySelector('#unit-converter'))
        {
            if (window.numforgeInitUnits)
                window.numforgeDisposeUnits = window.numforgeInitUnits();
            else await new Promise((resolve, reject) => {
                const script = document.createElement('script');
                script.src = '/assets/units.js';
                script.onload = resolve;
                script.onerror = reject;
                document.body.append(script);
            });
        }
        if (replacement.querySelector('#calculator'))
        {
            if (window.numforgeInitCalculator)
                window.numforgeDisposeCalculator = window.numforgeInitCalculator();
            else
            {
                await new Promise((resolve, reject) => {
                    const script = document.createElement('script');
                    script.src = '/assets/calculator.js';
                    script.onload = resolve;
                    script.onerror = reject;
                    document.body.append(script);
                });
            }
        }
        scrollToDestination(destination.hash);
        if (!reducedMotion.matches)
        {
            replacement.classList.add('content-entering');
            replacement.addEventListener('animationend', () =>
                replacement.classList.remove('content-entering'), {once: true});
        }
    }
    catch (error)
    {
        console.error('NumForge navigation failed:', error);
        stagedStyle?.remove();
        pendingNavigation = null;
        window.location.assign(destination.href);
    }
    finally
    {
        navigating = false;
        if (pendingNavigation)
        {
            const next = pendingNavigation;
            pendingNavigation = null;
            navigate(next.destination, next.addHistory);
        }
    }
}

document.addEventListener('click', event => {
    if (event.defaultPrevented || event.button !== 0 ||
        event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return;
    const link = event.target.closest?.('a[href]');
    if (!link || link.hasAttribute('download') ||
        (link.target && link.target !== '_self')) return;
    if (link.matches('[data-license]'))
    {
        event.preventDefault();
        openLicense();
        return;
    }
    const destination = new URL(link.href, window.location.href);
    if (destination.origin !== window.location.origin || !routes.has(destination.pathname)) return;
    if (navigating)
    {
        event.preventDefault();
        pendingNavigation = {destination, addHistory: true};
        return;
    }
    if (destination.pathname + destination.search === activePageKey)
    {
        if (destination.hash) return;
        event.preventDefault();
        return;
    }
    event.preventDefault();
    navigate(destination);
});

window.addEventListener('popstate', () => {
    const destination = new URL(window.location.href);
    if (navigating)
    {
        pendingNavigation = {destination, addHistory: false};
        return;
    }
    if (destination.pathname + destination.search === activePageKey)
        scrollToDestination(destination.hash);
    else if (routes.has(destination.pathname)) navigate(destination, false);
});
