// Smooth only full-page navigation inside the local NumForge web app.
const pageShell = document.querySelector('.calculator-shell, .guide-shell');
const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
let leaving = false;
let licenseDialog = null;

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
        if (!licenseDialog.open) licenseDialog.showModal();
    }
    catch (_) { window.location.assign('/LICENSE'); }
}

document.addEventListener('click', event => {
    if (leaving || event.defaultPrevented || event.button !== 0 ||
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
    const current = window.location;
    if (destination.origin !== current.origin ||
        !['http:', 'https:'].includes(destination.protocol)) return;

    // Section links should keep their normal scroll and history behavior.
    if (destination.pathname === current.pathname &&
        destination.search === current.search && destination.hash) return;

    window.dispatchEvent(new Event('numforge:navigate'));
    if (!pageShell || reducedMotion.matches) return;
    event.preventDefault();
    leaving = true;
    pageShell.classList.add('page-leaving');
    window.setTimeout(() => window.location.assign(destination.href), 170);
});

// Back-forward cache can restore a page before its delayed navigation completes.
window.addEventListener('pageshow', () => {
    leaving = false;
    pageShell?.classList.remove('page-leaving');
});
