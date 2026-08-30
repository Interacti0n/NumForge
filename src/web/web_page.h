#ifndef NUMFORGE_WEB_PAGE_H
#define NUMFORGE_WEB_PAGE_H

#include <stddef.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Small calculator and API-guide pages embedded in the server executable.
    Keeping them here makes numforge_web self-contained: no Node.js, assets,
    or working-directory setup is required to use the local calculator.
------------------------------------------------------------------------------------------------------------------------------
*/
static const char NUMFORGE_WEB_PAGE_START[] =
    "<!doctype html>\n"
    "<html lang=\"sk\">\n"
    "<head>\n"
    "  <meta charset=\"utf-8\">\n"
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
    "  <title>NumForge</title>\n"
    "  <style>\n"
    "    :root { color-scheme: dark; font-family: system-ui, sans-serif; }\n"
    "    body { max-width: 720px; margin: 0 auto; padding: 48px 20px; background: #111318; color: #edf0f5; }\n"
    "    .page-header { display: flex; flex-wrap: wrap; align-items: center; justify-content: space-between; gap: 14px; }\n"
    "    h1 { margin: 0; color: #ff9d36; }\n"
    "    .language-switch { display: flex; gap: 6px; }\n"
    "    .language-switch a { padding: 6px 9px; border: 1px solid #3b4352; border-radius: 6px; color: #adb5c3; font-size: .9rem; text-decoration: none; }\n"
    "    .language-switch a[aria-current=\"true\"] { border-color: #ff9d36; color: #ffb35e; }\n"
    "    p { color: #adb5c3; line-height: 1.55; }\n"
    "    a { color: #ffb35e; }\n"
    "    form { display: flex; gap: 10px; margin-top: 28px; }\n"
    "    input { min-width: 0; flex: 1; padding: 13px; border: 1px solid #3b4352; border-radius: 8px; background: #1c2028; color: inherit; font: 1rem ui-monospace, monospace; }\n"
    "    button { padding: 12px 18px; border: 0; border-radius: 8px; background: #ff9d36; color: #17110a; font-weight: 700; cursor: pointer; }\n"
    "    .precision { display: flex; flex-wrap: wrap; align-items: center; gap: 10px 16px; margin-top: 14px; color: #adb5c3; font-size: .92rem; }\n"
    "    .precision label { display: flex; align-items: center; gap: 7px; }\n"
    "    .precision input[type=number] { width: 6.5rem; flex: none; padding: 8px; }\n"
    "    .precision input[type=checkbox] { width: auto; flex: none; }\n"
    "    .keypad { display: grid; grid-template-columns: repeat(5, minmax(0, 1fr)); gap: 8px; margin-top: 14px; }\n"
    "    .keypad button { padding: 12px 6px; background: #282e39; color: #edf0f5; }\n"
    "    .keypad button.operator, .keypad button.equals { background: #ff9d36; color: #17110a; }\n"
    "    .keypad button.action { background: #414b5c; }\n"
    "    .keypad button.future { color: #7d8798; background: #1a1e26; cursor: not-allowed; }\n"
    "    .keypad.constants { grid-template-columns: repeat(3, minmax(0, 1fr)); }\n"
    "    .keypad-label { margin: 24px 0 8px; color: #adb5c3; font-size: .84rem; font-weight: 700; text-transform: uppercase; letter-spacing: .08em; }\n"
    "    .result-panel { margin-top: 22px; padding: 16px; border-radius: 8px; background: #1c2028; }\n"
    "    .result-label { display: block; margin-bottom: 7px; color: #adb5c3; font-size: .84rem; font-weight: 700; text-transform: uppercase; letter-spacing: .08em; }\n"
    "    #result { display: block; min-height: 1.5em; font: 1.2rem ui-monospace, monospace; overflow-wrap: anywhere; }\n"
    "    #result.error { color: #ff8888; }\n"
    "    .guide-link { display: inline-block; margin-top: 22px; }\n"
    "    code, pre { border-radius: 6px; background: #1c2028; font-family: ui-monospace, monospace; }\n"
    "    code { padding: 2px 5px; }\n"
    "    pre { padding: 14px; overflow-x: auto; }\n"
    "  </style>\n"
    "</head>\n"
    "<body>\n"
    "  <header class=\"page-header\">\n"
    "    <h1>NumForge</h1>\n"
    "    <nav class=\"language-switch\" aria-label=\"Výber jazyka\"><a href=\"/?lang=sk\" lang=\"sk\" aria-current=\"true\" title=\"Slovenčina\">🇸🇰 SK</a><a href=\"/?lang=en\" lang=\"en\" title=\"English\">🇬🇧 EN</a></nav>\n"
    "  </header>\n"
    "  <p>Zapíš výraz a NumForge ho vyhodnotí priamo cez C parser a presný BigDecimal.</p>\n"
    "  <form id=\"calculator\">\n"
    "    <input id=\"expression\" aria-label=\"Matematický výraz\" autocomplete=\"off\" autofocus>\n"
    "    <button type=\"submit\">Vypočítať</button>\n"
    "  </form>\n";

static const char NUMFORGE_WEB_PAGE_RESULT[] =
    "  <section class=\"result-panel\" aria-live=\"polite\">\n"
    "    <div style=\"display:flex;align-items:center;justify-content:space-between;gap:12px\">\n"
    "      <span class=\"result-label\">Výsledok</span><button id=\"copy-result\" type=\"button\" disabled title=\"Skopírovať výsledok\" aria-label=\"Skopírovať výsledok\" style=\"padding:6px 9px;font-size:.82rem\">⧉ Kopírovať</button>\n"
    "    </div>\n"
    "    <output id=\"result\"></output>\n"
    "  </section>\n";

static const char NUMFORGE_WEB_PAGE_KEYPAD[] =
    "  <section class=\"precision\" aria-label=\"Nastavenie výstupnej presnosti\">\n"
    "    <label>Desatinné miesta <input id=\"precision\" type=\"number\" min=\"0\" step=\"1\" value=\"10\" inputmode=\"numeric\"></label>\n"
    "    <label><input id=\"full-precision\" type=\"checkbox\"> Plný výstup</label>\n"
    "  </section>\n"
    "  <section class=\"keypad\" aria-label=\"Kalkulačná klávesnica\">\n"
    "    <button type=\"button\" class=\"operator\" data-insert=\"(\">(</button><button type=\"button\" class=\"operator\" data-insert=\")\">)</button><button type=\"button\" data-insert=\".\">.</button><button type=\"button\" class=\"action\" data-action=\"clear\">C</button><button type=\"button\" class=\"action\" data-action=\"backspace\" aria-label=\"Vymazať posledný znak\">⌫</button>\n"
    "  </section>\n"
    "  <section class=\"keypad\" aria-label=\"Číselná klávesnica\">\n"
    "    <button type=\"button\" data-insert=\"7\">7</button><button type=\"button\" data-insert=\"8\">8</button><button type=\"button\" data-insert=\"9\">9</button><button type=\"button\" class=\"operator\" data-insert=\"/\">÷</button><button type=\"button\" class=\"operator\" data-insert=\"*\">×</button>\n"
    "    <button type=\"button\" data-insert=\"4\">4</button><button type=\"button\" data-insert=\"5\">5</button><button type=\"button\" data-insert=\"6\">6</button><button type=\"button\" class=\"operator\" data-insert=\"-\">−</button><button type=\"button\" class=\"operator\" data-insert=\"+\">+</button>\n"
    "    <button type=\"button\" data-insert=\"1\">1</button><button type=\"button\" data-insert=\"2\">2</button><button type=\"button\" data-insert=\"3\">3</button><button type=\"button\" data-insert=\"0\">0</button><button type=\"button\" class=\"equals\" data-action=\"evaluate\">=</button>\n"
    "  </section>\n"
    "  <p class=\"keypad-label\">Konštanty</p>\n"
    "  <section class=\"keypad constants\" aria-label=\"Matematické konštanty\">\n"
    "    <button type=\"button\" data-insert=\"&#960;\" title=\"π\">π</button><button type=\"button\" data-insert=\"e\" title=\"Eulerovo číslo\">e</button><button type=\"button\" data-insert=\"&#966;\" title=\"φ\">φ</button>\n"
    "  </section>\n";

static const char NUMFORGE_WEB_PAGE_EN_START[] =
    "<!doctype html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "  <meta charset=\"utf-8\">\n"
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
    "  <title>NumForge</title>\n"
    "  <style>\n"
    "    :root { color-scheme: dark; font-family: system-ui, sans-serif; } body { max-width: 720px; margin: 0 auto; padding: 48px 20px; background: #111318; color: #edf0f5; }\n"
    "    .page-header { display: flex; flex-wrap: wrap; align-items: center; justify-content: space-between; gap: 14px; } h1 { margin: 0; color: #ff9d36; } p { color: #adb5c3; line-height: 1.55; } a { color: #ffb35e; }\n"
    "    .language-switch { display: flex; gap: 6px; } .language-switch a { padding: 6px 9px; border: 1px solid #3b4352; border-radius: 6px; color: #adb5c3; font-size: .9rem; text-decoration: none; } .language-switch a[aria-current=\"true\"] { border-color: #ff9d36; color: #ffb35e; }\n"
    "    form { display: flex; gap: 10px; margin-top: 28px; } input { min-width: 0; flex: 1; padding: 13px; border: 1px solid #3b4352; border-radius: 8px; background: #1c2028; color: inherit; font: 1rem ui-monospace, monospace; } button { padding: 12px 18px; border: 0; border-radius: 8px; background: #ff9d36; color: #17110a; font-weight: 700; cursor: pointer; }\n"
    "    .precision { display: flex; flex-wrap: wrap; align-items: center; gap: 10px 16px; margin-top: 14px; color: #adb5c3; font-size: .92rem; } .precision label { display: flex; align-items: center; gap: 7px; } .precision input[type=number] { width: 6.5rem; flex: none; padding: 8px; } .precision input[type=checkbox] { width: auto; flex: none; }\n"
    "    .keypad { display: grid; grid-template-columns: repeat(5, minmax(0, 1fr)); gap: 8px; margin-top: 14px; } .keypad button { padding: 12px 6px; background: #282e39; color: #edf0f5; } .keypad button.operator, .keypad button.equals { background: #ff9d36; color: #17110a; } .keypad button.action { background: #414b5c; } .keypad button.future { color: #7d8798; background: #1a1e26; cursor: not-allowed; } .keypad.constants { grid-template-columns: repeat(3, minmax(0, 1fr)); }\n"
    "    .keypad-label { margin: 24px 0 8px; color: #adb5c3; font-size: .84rem; font-weight: 700; text-transform: uppercase; letter-spacing: .08em; } .result-panel { margin-top: 22px; padding: 16px; border-radius: 8px; background: #1c2028; } .result-label { display: block; margin-bottom: 7px; color: #adb5c3; font-size: .84rem; font-weight: 700; text-transform: uppercase; letter-spacing: .08em; } #result { display: block; min-height: 1.5em; font: 1.2rem ui-monospace, monospace; overflow-wrap: anywhere; } #result.error { color: #ff8888; } .guide-link { display: inline-block; margin-top: 22px; }\n"
    "  </style>\n"
    "</head>\n"
    "<body>\n"
    "  <header class=\"page-header\">\n"
    "    <h1>NumForge</h1>\n"
    "    <nav class=\"language-switch\" aria-label=\"Language selection\"><a href=\"/?lang=sk\" lang=\"sk\" title=\"Slovenčina\">🇸🇰 SK</a><a href=\"/?lang=en\" lang=\"en\" aria-current=\"true\" title=\"English\">🇬🇧 EN</a></nav>\n"
    "  </header>\n"
    "  <p>Enter an expression and NumForge evaluates it directly with its C parser and exact BigDecimal.</p>\n"
    "  <form id=\"calculator\">\n"
    "    <input id=\"expression\" aria-label=\"Mathematical expression\" autocomplete=\"off\" autofocus>\n"
    "    <button type=\"submit\">Calculate</button>\n"
    "  </form>\n";

static const char NUMFORGE_WEB_PAGE_EN_RESULT[] =
    "  <section class=\"result-panel\" aria-live=\"polite\">\n"
    "    <div style=\"display:flex;align-items:center;justify-content:space-between;gap:12px\">\n"
    "      <span class=\"result-label\">Result</span><button id=\"copy-result\" type=\"button\" disabled title=\"Copy result\" aria-label=\"Copy result\" style=\"padding:6px 9px;font-size:.82rem\">⧉ Copy</button>\n"
    "    </div>\n"
    "    <output id=\"result\"></output>\n"
    "  </section>\n";

static const char NUMFORGE_WEB_PAGE_EN_KEYPAD[] =
    "  <section class=\"precision\" aria-label=\"Output precision settings\">\n"
    "    <label>Decimal places <input id=\"precision\" type=\"number\" min=\"0\" step=\"1\" value=\"10\" inputmode=\"numeric\"></label>\n"
    "    <label><input id=\"full-precision\" type=\"checkbox\"> Full output</label>\n"
    "  </section>\n"
    "  <section class=\"keypad\" aria-label=\"Calculator keypad\">\n"
    "    <button type=\"button\" class=\"operator\" data-insert=\"(\">(</button><button type=\"button\" class=\"operator\" data-insert=\")\">)</button><button type=\"button\" data-insert=\".\">.</button><button type=\"button\" class=\"action\" data-action=\"clear\">C</button><button type=\"button\" class=\"action\" data-action=\"backspace\" aria-label=\"Delete last character\">⌫</button>\n"
    "  </section>\n"
    "  <section class=\"keypad\" aria-label=\"Number keypad\">\n"
    "    <button type=\"button\" data-insert=\"7\">7</button><button type=\"button\" data-insert=\"8\">8</button><button type=\"button\" data-insert=\"9\">9</button><button type=\"button\" class=\"operator\" data-insert=\"/\">÷</button><button type=\"button\" class=\"operator\" data-insert=\"*\">×</button>\n"
    "    <button type=\"button\" data-insert=\"4\">4</button><button type=\"button\" data-insert=\"5\">5</button><button type=\"button\" data-insert=\"6\">6</button><button type=\"button\" class=\"operator\" data-insert=\"-\">−</button><button type=\"button\" class=\"operator\" data-insert=\"+\">+</button>\n"
    "    <button type=\"button\" data-insert=\"1\">1</button><button type=\"button\" data-insert=\"2\">2</button><button type=\"button\" data-insert=\"3\">3</button><button type=\"button\" data-insert=\"0\">0</button><button type=\"button\" class=\"equals\" data-action=\"evaluate\">=</button>\n"
    "  </section>\n"
    "  <p class=\"keypad-label\">Constants</p>\n"
    "  <section class=\"keypad constants\" aria-label=\"Mathematical constants\">\n"
    "    <button type=\"button\" data-insert=\"&#960;\" title=\"π\">π</button><button type=\"button\" data-insert=\"e\" title=\"Euler's number\">e</button><button type=\"button\" data-insert=\"&#966;\" title=\"φ\">φ</button>\n"
    "  </section>\n";

static const char NUMFORGE_WEB_PAGE_EN_FUTURE[] =
    "  <p class=\"keypad-label\">Functions and operations</p>\n"
    "  <section class=\"keypad\" aria-label=\"Functions and operations\">\n"
    "    <button type=\"button\" data-insert=\"^\" title=\"Power: exponent must be a non-negative integer\">xʸ</button><button type=\"button\" data-insert=\"&#178;\" title=\"Square\">x²</button><button type=\"button\" data-insert=\"&#179;\" title=\"Cube\">x³</button><button type=\"button\" data-insert=\"!\" title=\"Factorial\">n!</button>\n"
    "    <button type=\"button\" class=\"future\" disabled title=\"Coming soon\">√x</button><button type=\"button\" class=\"future\" disabled title=\"Coming soon\">|x|</button><button type=\"button\" class=\"future\" disabled title=\"Coming soon\">sin</button><button type=\"button\" class=\"future\" disabled title=\"Coming soon\">cos</button><button type=\"button\" class=\"future\" disabled title=\"Coming soon\">tan</button>\n"
    "    <button type=\"button\" class=\"future\" disabled title=\"Coming soon\">ln</button><button type=\"button\" class=\"future\" disabled title=\"Coming soon\">log</button><button type=\"button\" class=\"future\" disabled title=\"Coming soon\">eˣ</button>\n"
    "  </section>\n"
    "  <a class=\"guide-link\" href=\"/api?lang=en\">How calculation and the API work →</a>\n";

static const char NUMFORGE_WEB_PAGE_FUTURE[] =
    "  <p class=\"keypad-label\">Funkcie a operácie</p>\n"
    "  <section class=\"keypad\" aria-label=\"Funkcie a operácie\">\n"
    "    <button type=\"button\" data-insert=\"^\" title=\"Mocnina: exponent musí byť nezáporné celé číslo\">xʸ</button><button type=\"button\" data-insert=\"&#178;\" title=\"Druhá mocnina\">x²</button><button type=\"button\" data-insert=\"&#179;\" title=\"Tretia mocnina\">x³</button><button type=\"button\" data-insert=\"!\" title=\"Faktoriál\">n!</button>\n"
    "    <button type=\"button\" class=\"future\" disabled title=\"Pripravované\">√x</button><button type=\"button\" class=\"future\" disabled title=\"Pripravované\">|x|</button><button type=\"button\" class=\"future\" disabled title=\"Pripravované\">sin</button><button type=\"button\" class=\"future\" disabled title=\"Pripravované\">cos</button><button type=\"button\" class=\"future\" disabled title=\"Pripravované\">tan</button>\n"
    "    <button type=\"button\" class=\"future\" disabled title=\"Pripravované\">ln</button><button type=\"button\" class=\"future\" disabled title=\"Pripravované\">log</button><button type=\"button\" class=\"future\" disabled title=\"Pripravované\">eˣ</button>\n"
    "  </section>\n"
    "  <a class=\"guide-link\" href=\"/api\">Ako funguje výpočet a API →</a>\n";

static const char NUMFORGE_WEB_PAGE_SCRIPT[] =
    "  <script>\n"
    "    const form = document.querySelector('#calculator');\n"
    "    const expression = document.querySelector('#expression');\n"
    "    const result = document.querySelector('#result');\n"
    "    const copyResult = document.querySelector('#copy-result');\n"
    "    const precision = document.querySelector('#precision');\n"
    "    const fullPrecision = document.querySelector('#full-precision');\n"
    "    const english = document.documentElement.lang === 'en';\n"
    "    const text = english ? { calculating: 'Calculating…', precision: 'Enter a non-negative whole number of decimal places.', failure: 'Calculation failed.', error: 'Error: ', copy: '⧉ Copy', copied: '✓ Copied' } : { calculating: 'Počítam…', precision: 'Zadaj nezáporný celý počet desatinných miest.', failure: 'Výpočet zlyhal.', error: 'Chyba: ', copy: '⧉ Kopírovať', copied: '✓ Skopírované' };\n"
    "    function insertText(text) {\n"
    "      const start = expression.selectionStart ?? expression.value.length;\n"
    "      const end = expression.selectionEnd ?? start;\n"
    "      expression.setRangeText(text, start, end, 'end'); expression.focus();\n"
    "    }\n"
    "    function eraseText() {\n"
    "      const start = expression.selectionStart ?? expression.value.length;\n"
    "      const end = expression.selectionEnd ?? start;\n"
    "      if (start !== end) expression.setRangeText('', start, end, 'end');\n"
    "      else if (start > 0) expression.setRangeText('', start - 1, start, 'end');\n"
    "      expression.focus();\n"
    "    }\n"
    "    async function copyText(value) {\n"
    "      if (navigator.clipboard && window.isSecureContext) { await navigator.clipboard.writeText(value); return; }\n"
    "      const temporary = document.createElement('textarea'); temporary.value = value; temporary.style.position = 'fixed'; temporary.style.opacity = '0'; document.body.append(temporary); temporary.select();\n"
    "      const copied = document.execCommand('copy'); temporary.remove(); if (!copied) throw new Error('copy failed');\n"
    "    }\n"
    "    document.querySelectorAll('[data-insert]').forEach((button) => button.addEventListener('click', () => insertText(button.dataset.insert)));\n"
    "    document.querySelectorAll('[data-action]').forEach((button) => button.addEventListener('click', () => {\n"
    "      if (button.dataset.action === 'clear') { expression.value = ''; expression.focus(); }\n"
    "      else if (button.dataset.action === 'backspace') eraseText();\n"
    "      else form.requestSubmit();\n"
    "    }));\n"
    "    copyResult.addEventListener('click', async () => {\n"
    "      if (!result.textContent) return;\n"
    "      try { await copyText(result.textContent); copyResult.textContent = text.copied; copyResult.disabled = true; setTimeout(() => { copyResult.textContent = text.copy; copyResult.disabled = !result.textContent; }, 1400); } catch (_) { copyResult.textContent = text.copy; }\n"
    "    });\n"
    "    fullPrecision.addEventListener('change', () => { precision.disabled = fullPrecision.checked; });\n"
    "    form.addEventListener('submit', async (event) => {\n"
    "      event.preventDefault();\n"
    "      result.className = ''; result.textContent = text.calculating; copyResult.disabled = true; copyResult.textContent = text.copy;\n"
    "      try {\n"
    "        const requestedPrecision = fullPrecision.checked ? 'full' : precision.value;\n"
    "        if (!fullPrecision.checked && (!/^[0-9]+$/.test(requestedPrecision))) throw new Error(text.precision);\n"
    "        const response = await fetch('/api/evaluate?precision=' + encodeURIComponent(requestedPrecision), { method: 'POST', headers: { 'Content-Type': 'text/plain; charset=utf-8' }, body: expression.value });\n"
    "        const data = await response.json();\n"
    "        if (!response.ok || !data.ok) throw new Error(data.error || text.failure);\n"
    "        result.textContent = data.result; copyResult.disabled = false;\n"
    "      } catch (error) {\n"
    "        result.className = 'error'; result.textContent = text.error + error.message; copyResult.disabled = true;\n"
    "      }\n"
    "    });\n"
    "  </script>\n"
    "</body>\n"
    "</html>\n";

static const char *const NUMFORGE_WEB_PAGE[] = {
    NUMFORGE_WEB_PAGE_START,
    NUMFORGE_WEB_PAGE_RESULT,
    NUMFORGE_WEB_PAGE_KEYPAD,
    NUMFORGE_WEB_PAGE_FUTURE,
    NUMFORGE_WEB_PAGE_SCRIPT,
    NULL
};

static const char *const NUMFORGE_WEB_PAGE_EN[] = {
    NUMFORGE_WEB_PAGE_EN_START,
    NUMFORGE_WEB_PAGE_EN_RESULT,
    NUMFORGE_WEB_PAGE_EN_KEYPAD,
    NUMFORGE_WEB_PAGE_EN_FUTURE,
    NUMFORGE_WEB_PAGE_SCRIPT,
    NULL
};

static const char NUMFORGE_API_PAGE_START[] =
    "<!doctype html>\n"
    "<html lang=\"sk\">\n"
    "<head>\n"
    "  <meta charset=\"utf-8\">\n"
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
    "  <title>NumForge API</title>\n"
    "  <style>\n"
    "    :root { color-scheme: dark; font-family: system-ui, sans-serif; }\n"
    "    body { max-width: 720px; margin: 0 auto; padding: 48px 20px; background: #111318; color: #edf0f5; }\n"
    "    h1, h2, h3 { color: #ff9d36; }\n"
    "    p, li { color: #adb5c3; line-height: 1.55; }\n"
    "    a { color: #ffb35e; }\n"
    "    table { width: 100%; border-collapse: collapse; margin: 14px 0 24px; }\n"
    "    th, td { padding: 10px; border-bottom: 1px solid #343b49; text-align: left; vertical-align: top; }\n"
    "    th { color: #edf0f5; }\n"
    "    code, pre { border-radius: 6px; background: #1c2028; font-family: ui-monospace, monospace; }\n"
    "    code { padding: 2px 5px; }\n"
    "    pre { padding: 14px; overflow-x: auto; }\n"
    "    .notice { padding: 14px; border-left: 3px solid #ff9d36; border-radius: 4px; background: #1c2028; }\n"
    "  </style>\n"
    "</head>\n"
    "<body>\n";

static const char NUMFORGE_API_PAGE_SK_CONTENT[] =
    "  <header style=\"display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:14px\">\n"
    "    <a href=\"/?lang=sk\">← Späť na kalkulačku</a>\n"
    "    <nav aria-label=\"Výber jazyka\" style=\"display:flex;gap:6px\"><a href=\"/api?lang=sk\" aria-current=\"true\" title=\"Slovenčina\" style=\"padding:6px 9px;border:1px solid #ff9d36;border-radius:6px;color:#ffb35e;font-size:.9rem;text-decoration:none\">🇸🇰 SK</a><a href=\"/api?lang=en\" title=\"English\" style=\"padding:6px 9px;border:1px solid #3b4352;border-radius:6px;color:#adb5c3;font-size:.9rem;text-decoration:none\">🇬🇧 EN</a></nav>\n"
    "  </header>\n"
    "  <h1>NumForge: použitie a API</h1>\n"
    "  <p>Kalkulačka neposiela výpočet JavaScriptu. Výraz ide priamo do C aplikácie: tokenizer → parser → BigDecimal → textový výsledok.</p>\n"
    "  <p class=\"notice\">Sčítanie, odčítanie a násobenie desatinných čísel sú presné. Delenie má predvolených 34 desatinných miest a zaokrúhľuje pravidlom half-even.</p>\n"
    "  <h2>Čo môžeš zadať do kalkulačky</h2>\n"
    "  <table><tr><th>Prvok</th><th>Príklady</th></tr>\n"
    "  <tr><td>Celé a desatinné čísla</td><td><code>42</code>, <code>-1.5</code>, <code>1,5</code>, <code>.25</code>, <code>1.</code></td></tr>\n"
    "  <tr><td>Vedecký zápis</td><td><code>1.25E-3</code>, <code>6E4</code>; veľké <code>E</code> je povinné</td></tr>\n"
    "  <tr><td>Operátory</td><td><code>+</code>, <code>-</code>, <code>*</code>, <code>/</code>, <code>^</code>; mocnina má najvyššiu prioritu a exponent musí byť nezáporné celé číslo</td></tr>\n"
    "  <tr><td>Zátvorky a znamienka</td><td><code>(2 + 3) * 4</code>, <code>-(2.5E-1) * 8</code></td></tr>\n"
    "  <tr><td>Postfixové operácie</td><td><code>12²</code>, <code>2³</code>, <code>5!</code>; faktorál vyžaduje nezáporné celé číslo najviac 5000</td></tr>\n"
    "  <tr><td>Konštanty</td><td><code>π</code>, <code>e</code>, <code>φ</code></td></tr>\n"
    "  <tr><td>Implicitné násobenie</td><td><code>2π</code>, <code>πe</code>, <code>2(3 + 4)</code></td></tr></table>\n"
    "  <p>Momentálne nie sú podporované <code>%</code>, premenné ani ostatné funkcie.</p>\n"
    "  <p>Konštanty majú uložených 200 desatinných miest. Malé <code>e</code> vždy znamená Eulerovo číslo, preto <code>5e</code> znamená <code>5 * e</code> a <code>1e3</code> znamená <code>1 * e * 3</code>. Vedecký zápis vždy používa veľké <code>E</code>: <code>5E-1</code> je <code>0.5</code> a <code>1E3</code> je <code>1000</code>. Tlačidlá budúcich funkcií sú zámerne neaktívne; zatiaľ nepridávajú žiadnu syntax ani výpočet.</p>\n"
    "  <h2>Výstupná presnosť</h2>\n"
    "  <p>Nastavenie <strong>Desatinné miesta</strong> určuje počet miest, na ktoré sa výsledok zaokrúhli pravidlom half-even; predvolená hodnota je 10. Voľba <strong>Plný výstup</strong> nevynucuje výstupné zaokrúhlenie. Veľmi malé a veľké nenulové výsledky sa zobrazia vo vedeckom zápise s veľkým <code>E</code>, napríklad <code>1.25E-12</code>.</p>\n"
    "  <p>Výpočet má približne päťsekundový CPU limit. Po jeho prekročení sa výpočet zastaví s chybou <code>TLE</code>; jeden už začatý extrémne veľký krok BigInt sa môže dokončiť tesne po limite.</p>\n";

/*
 * Keep each embedded C string below the ISO C required minimum limit of
 * 4095 characters. GCC diagnoses longer concatenated literals under -Werror.
 */
static const char NUMFORGE_API_PAGE_HTTP[] =
    "  <h2>Lokálne HTTP rozhranie</h2>\n"
    "  <p>Vlastný lokálny klient môže poslať výraz ako obyčajný UTF-8 text na <code>POST /api/evaluate?precision=10</code>. Parameter <code>precision</code> je nezáporné celé číslo alebo <code>full</code>. Vstup má limit 4096 bajtov.</p>\n"
    "  <pre>POST /api/evaluate?precision=10\nContent-Type: text/plain; charset=utf-8\n\nπ / 2\n\nHTTP 200\n{\"ok\":true,\"result\":\"1.5707963268\"}</pre>\n"
    "  <p>Neplatný výraz alebo delenie nulou vrátia HTTP 400 a JSON s <code>ok: false</code>, opisom chyby a stĺpcom chyby.</p>\n";

static const char NUMFORGE_API_PAGE_C_LIBRARY[] =
    "  <h2>Verejné C API</h2>\n"
    "  <p>Verejné sú zatiaľ typy <code>BigInt</code> a <code>BigDecimal</code>. Sú opaque: vytvor ich cez <code>*_create()</code>, uvoľni cez <code>*_destroy()</code>, a reťazce z <code>*_to_string()</code> uvoľni cez <code>free()</code>.</p>\n"
    "  <h3>BigInt</h3>\n"
    "  <p><code>#include &lt;numforge/bigint.h&gt;</code></p>\n"
    "  <ul><li>Životný cyklus a text: <code>bigint_create</code>, <code>bigint_destroy</code>, <code>bigint_copy</code>, <code>bigint_set_string</code>, <code>bigint_to_string</code></li>\n"
    "  <li>Porovnanie: <code>bigint_compare</code>, <code>bigint_is_zero</code>, <code>bigint_is_one</code>, <code>bigint_is_negative</code>, <code>bigint_is_even</code>, <code>bigint_is_odd</code></li>\n"
    "  <li>Aritmetika: <code>bigint_abs</code>, <code>bigint_negate</code>, <code>bigint_add</code>, <code>bigint_sub</code>, <code>bigint_mul</code>, <code>bigint_div</code>, <code>bigint_mod</code>, <code>bigint_div_mod</code>, <code>bigint_pow</code></li>\n"
    "  <li>Teória čísel: <code>bigint_gcd</code>, <code>bigint_lcm</code>, <code>bigint_factorial</code>, <code>bigint_is_probable_prime</code>, <code>bigint_is_perfect_square</code></li>\n"
    "  <li>Bity: <code>bigint_and</code>, <code>bigint_or</code>, <code>bigint_xor</code>, <code>bigint_not</code>, <code>bigint_shift_left</code>, <code>bigint_shift_right</code></li></ul>\n"
    "  <p>Delenie skracuje smerom k nule. <code>bigint_div_mod</code> vyžaduje rozdielne objekty pre podiel a zvyšok. Bitové AND/OR/XOR prijímajú iba nezáporné hodnoty.</p>\n"
    "  <h3>BigDecimal</h3>\n"
    "  <p><code>#include &lt;numforge/bigdecimal.h&gt;</code></p>\n"
    "  <ul><li>Životný cyklus a text: <code>bigdecimal_create</code>, <code>bigdecimal_destroy</code>, <code>bigdecimal_copy</code>, <code>bigdecimal_set_string</code>, <code>bigdecimal_to_string</code></li>\n"
    "  <li>Porovnanie: <code>bigdecimal_compare</code>, <code>bigdecimal_is_zero</code>, <code>bigdecimal_is_negative</code></li>\n"
    "  <li>Presné operácie: <code>bigdecimal_abs</code>, <code>bigdecimal_negate</code>, <code>bigdecimal_add</code>, <code>bigdecimal_sub</code>, <code>bigdecimal_mul</code></li>\n"
    "  <li>Zaokrúhľované operácie: <code>bigdecimal_rescale</code>, <code>bigdecimal_div</code>; režimy <code>TOWARD_ZERO</code>, <code>AWAY_FROM_ZERO</code>, <code>FLOOR</code>, <code>CEILING</code>, <code>HALF_UP</code>, <code>HALF_EVEN</code> s prefixom <code>BIGDECIMAL_ROUND_</code></li></ul>\n"
    "  <p>Mutujúce operácie vracajú stavový kód a pri chybe ponechajú cieľovú hodnotu nezmenenú. Úplné signatúry sú v hlavičkách a podrobnejší prehľad v <code>docs/API.md</code> projekte.</p>\n"
    "</body>\n"
    "</html>\n";

static const char *const NUMFORGE_API_PAGE[] = {
    NUMFORGE_API_PAGE_START,
    NUMFORGE_API_PAGE_SK_CONTENT,
    NUMFORGE_API_PAGE_HTTP,
    NUMFORGE_API_PAGE_C_LIBRARY,
    NULL
};

static const char NUMFORGE_API_PAGE_EN_START[] =
    "<!doctype html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "  <meta charset=\"utf-8\">\n"
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
    "  <title>NumForge API</title>\n"
    "  <style>\n"
    "    :root { color-scheme: dark; font-family: system-ui, sans-serif; } body { max-width: 720px; margin: 0 auto; padding: 48px 20px; background: #111318; color: #edf0f5; } .page-header { display: flex; flex-wrap: wrap; align-items: center; justify-content: space-between; gap: 14px; } h1, h2, h3 { color: #ff9d36; } h1 { margin: 0; } p, li { color: #adb5c3; line-height: 1.55; } a { color: #ffb35e; } .language-switch { display: flex; gap: 6px; } .language-switch a { padding: 6px 9px; border: 1px solid #3b4352; border-radius: 6px; color: #adb5c3; font-size: .9rem; text-decoration: none; } .language-switch a[aria-current=\"true\"] { border-color: #ff9d36; color: #ffb35e; }\n"
    "    table { width: 100%; border-collapse: collapse; margin: 14px 0 24px; } th, td { padding: 10px; border-bottom: 1px solid #343b49; text-align: left; vertical-align: top; } th { color: #edf0f5; } code, pre { border-radius: 6px; background: #1c2028; font-family: ui-monospace, monospace; } code { padding: 2px 5px; } pre { padding: 14px; overflow-x: auto; } .notice { padding: 14px; border-left: 3px solid #ff9d36; border-radius: 4px; background: #1c2028; }\n"
    "  </style>\n"
    "</head>\n"
    "<body>\n"
    "  <header class=\"page-header\">\n"
    "    <a href=\"/?lang=en\">← Back to calculator</a>\n"
    "    <nav class=\"language-switch\" aria-label=\"Language selection\"><a href=\"/api?lang=sk\" lang=\"sk\" title=\"Slovenčina\">🇸🇰 SK</a><a href=\"/api?lang=en\" lang=\"en\" aria-current=\"true\" title=\"English\">🇬🇧 EN</a></nav>\n"
    "  </header>\n"
    "  <h1>NumForge: usage and API</h1>\n"
    "  <p>The calculator does not send calculations to JavaScript. Each expression goes directly to the C application: tokenizer → parser → BigDecimal → text result.</p>\n"
    "  <p class=\"notice\">Addition, subtraction, and multiplication of decimal values are exact. Division uses 34 decimal places by default and rounds half-even.</p>\n"
    "  <h2>What you can enter</h2>\n"
    "  <table><tr><th>Element</th><th>Examples</th></tr>\n"
    "  <tr><td>Integers and decimals</td><td><code>42</code>, <code>-1.5</code>, <code>1,5</code>, <code>.25</code>, <code>1.</code></td></tr>\n"
    "  <tr><td>Scientific notation</td><td><code>1.25E-3</code>, <code>6E4</code>; uppercase <code>E</code> is required</td></tr>\n"
    "  <tr><td>Operators</td><td><code>+</code>, <code>-</code>, <code>*</code>, <code>/</code>, <code>^</code>; power has the highest precedence and its exponent must be a non-negative integer</td></tr>\n"
    "  <tr><td>Parentheses and signs</td><td><code>(2 + 3) * 4</code>, <code>-(2.5E-1) * 8</code></td></tr>\n"
    "  <tr><td>Postfix operations</td><td><code>12²</code>, <code>2³</code>, <code>5!</code>; factorial requires a non-negative integer no greater than 5000</td></tr>\n"
    "  <tr><td>Constants</td><td><code>π</code>, <code>e</code>, <code>φ</code></td></tr>\n"
    "  <tr><td>Implicit multiplication</td><td><code>2π</code>, <code>πe</code>, <code>2(3 + 4)</code></td></tr></table>\n"
    "  <p><code>%</code>, variables, and other functions are not supported yet.</p>\n"
    "  <p>Constants store 200 decimal places. Lowercase <code>e</code> always means Euler's number: <code>5e</code> means <code>5 * e</code> and <code>1e3</code> means <code>1 * e * 3</code>. Scientific notation always uses uppercase <code>E</code>: <code>5E-1</code> is <code>0.5</code> and <code>1E3</code> is <code>1000</code>. Disabled buttons do not add syntax or run calculations yet.</p>\n";

static const char NUMFORGE_API_PAGE_EN_DETAILS[] =
    "  <h2>Output precision</h2>\n"
    "  <p><strong>Decimal places</strong> sets the number of places to which the result is rounded half-even; the default is 10. <strong>Full output</strong> does not force output rounding. Very small and very large non-zero values use scientific notation with uppercase <code>E</code>, for example <code>1.25E-12</code>.</p>\n"
    "  <p>Calculation has an approximate five-second CPU limit. If exceeded, it stops with a <code>TLE</code> error; one already-started, extremely large BigInt step may finish just after the limit.</p>\n"
    "  <h2>Local HTTP interface</h2>\n"
    "  <p>A local client can send an expression as plain UTF-8 text to <code>POST /api/evaluate?precision=10</code>. The <code>precision</code> parameter is a non-negative integer or <code>full</code>. Input is limited to 4096 bytes.</p>\n"
    "  <pre>POST /api/evaluate?precision=10\nContent-Type: text/plain; charset=utf-8\n\nπ / 2\n\nHTTP 200\n{\"ok\":true,\"result\":\"1.5707963268\"}</pre>\n"
    "  <p>An invalid expression or division by zero returns HTTP 400 and JSON with <code>ok: false</code>, an error description, and the error column.</p>\n";

static const char NUMFORGE_API_PAGE_EN_C_LIBRARY[] =
    "  <h2>Public C API</h2>\n"
    "  <p>The public types are currently <code>BigInt</code> and <code>BigDecimal</code>. They are opaque: create them with <code>*_create()</code>, release them with <code>*_destroy()</code>, and release strings from <code>*_to_string()</code> with <code>free()</code>.</p>\n"
    "  <h3>BigInt</h3>\n"
    "  <p><code>#include &lt;numforge/bigint.h&gt;</code></p>\n"
    "  <ul><li>Lifecycle and text: <code>bigint_create</code>, <code>bigint_destroy</code>, <code>bigint_copy</code>, <code>bigint_set_string</code>, <code>bigint_to_string</code></li>\n"
    "  <li>Comparison: <code>bigint_compare</code>, <code>bigint_is_zero</code>, <code>bigint_is_one</code>, <code>bigint_is_negative</code>, <code>bigint_is_even</code>, <code>bigint_is_odd</code></li>\n"
    "  <li>Arithmetic: <code>bigint_abs</code>, <code>bigint_negate</code>, <code>bigint_add</code>, <code>bigint_sub</code>, <code>bigint_mul</code>, <code>bigint_div</code>, <code>bigint_mod</code>, <code>bigint_div_mod</code>, <code>bigint_pow</code></li>\n"
    "  <li>Number theory: <code>bigint_gcd</code>, <code>bigint_lcm</code>, <code>bigint_factorial</code>, <code>bigint_is_probable_prime</code>, <code>bigint_is_perfect_square</code></li>\n"
    "  <li>Bits: <code>bigint_and</code>, <code>bigint_or</code>, <code>bigint_xor</code>, <code>bigint_not</code>, <code>bigint_shift_left</code>, <code>bigint_shift_right</code></li></ul>\n"
    "  <p>Division truncates toward zero. <code>bigint_div_mod</code> requires distinct objects for quotient and remainder. Bitwise AND/OR/XOR accept non-negative values only.</p>\n"
    "  <h3>BigDecimal</h3>\n"
    "  <p><code>#include &lt;numforge/bigdecimal.h&gt;</code></p>\n"
    "  <ul><li>Lifecycle and text: <code>bigdecimal_create</code>, <code>bigdecimal_destroy</code>, <code>bigdecimal_copy</code>, <code>bigdecimal_set_string</code>, <code>bigdecimal_to_string</code></li>\n"
    "  <li>Comparison: <code>bigdecimal_compare</code>, <code>bigdecimal_is_zero</code>, <code>bigdecimal_is_negative</code></li>\n"
    "  <li>Exact operations: <code>bigdecimal_abs</code>, <code>bigdecimal_negate</code>, <code>bigdecimal_add</code>, <code>bigdecimal_sub</code>, <code>bigdecimal_mul</code></li>\n"
    "  <li>Rounded operations: <code>bigdecimal_rescale</code>, <code>bigdecimal_div</code>; modes <code>TOWARD_ZERO</code>, <code>AWAY_FROM_ZERO</code>, <code>FLOOR</code>, <code>CEILING</code>, <code>HALF_UP</code>, <code>HALF_EVEN</code> with the <code>BIGDECIMAL_ROUND_</code> prefix</li></ul>\n"
    "  <p>Mutating operations return a status code and leave the destination value unchanged on failure. Full signatures are in the headers and a more detailed overview is in <code>docs/API.md</code>.</p>\n"
    "</body>\n"
    "</html>\n";

static const char *const NUMFORGE_API_PAGE_EN[] = {
    NUMFORGE_API_PAGE_EN_START,
    NUMFORGE_API_PAGE_EN_DETAILS,
    NUMFORGE_API_PAGE_EN_C_LIBRARY,
    NULL
};

#endif
