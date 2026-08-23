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
static const char NUMFORGE_WEB_PAGE[] =
    "<!doctype html>\n"
    "<html lang=\"sk\">\n"
    "<head>\n"
    "  <meta charset=\"utf-8\">\n"
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
    "  <title>NumForge</title>\n"
    "  <style>\n"
    "    :root { color-scheme: dark; font-family: system-ui, sans-serif; }\n"
    "    body { max-width: 720px; margin: 0 auto; padding: 48px 20px; background: #111318; color: #edf0f5; }\n"
    "    h1 { margin: 0; color: #ff9d36; }\n"
    "    p { color: #adb5c3; line-height: 1.55; }\n"
    "    a { color: #ffb35e; }\n"
    "    form { display: flex; gap: 10px; margin-top: 28px; }\n"
    "    input { min-width: 0; flex: 1; padding: 13px; border: 1px solid #3b4352; border-radius: 8px; background: #1c2028; color: inherit; font: 1rem ui-monospace, monospace; }\n"
    "    button { padding: 12px 18px; border: 0; border-radius: 8px; background: #ff9d36; color: #17110a; font-weight: 700; cursor: pointer; }\n"
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
    "  <h1>NumForge</h1>\n"
    "  <p>Zapíš výraz a NumForge ho vyhodnotí priamo cez C parser a presný BigDecimal.</p>\n"
    "  <form id=\"calculator\">\n"
    "    <input id=\"expression\" aria-label=\"Matematický výraz\" value=\"0.1 + 0.2\" autocomplete=\"off\" autofocus>\n"
    "    <button type=\"submit\">Vypočítať</button>\n"
    "  </form>\n"
    "  <section class=\"result-panel\" aria-live=\"polite\">\n"
    "    <span class=\"result-label\">Výsledok</span>\n"
    "    <output id=\"result\">Pripravené</output>\n"
    "  </section>\n"
    "  <a class=\"guide-link\" href=\"/api\">Ako funguje výpočet a API →</a>\n"
    "  <script>\n"
    "    const form = document.querySelector('#calculator');\n"
    "    const expression = document.querySelector('#expression');\n"
    "    const result = document.querySelector('#result');\n"
    "    form.addEventListener('submit', async (event) => {\n"
    "      event.preventDefault();\n"
    "      result.className = ''; result.textContent = 'Počítam…';\n"
    "      try {\n"
    "        const response = await fetch('/api/evaluate', { method: 'POST', headers: { 'Content-Type': 'text/plain; charset=utf-8' }, body: expression.value });\n"
    "        const data = await response.json();\n"
    "        if (!response.ok || !data.ok) throw new Error(data.error || 'Výpočet zlyhal.');\n"
    "        result.textContent = data.result;\n"
    "      } catch (error) {\n"
    "        result.className = 'error'; result.textContent = 'Chyba: ' + error.message;\n"
    "      }\n"
    "    });\n"
    "  </script>\n"
    "</body>\n"
    "</html>\n";

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
    "<body>\n"
    "  <a href=\"/\">← Späť na kalkulačku</a>\n"
    "  <h1>NumForge: použitie a API</h1>\n"
    "  <p>Kalkulačka neposiela výpočet JavaScriptu. Výraz ide priamo do C aplikácie: tokenizer → parser → BigDecimal → textový výsledok.</p>\n"
    "  <p class=\"notice\">Sčítanie, odčítanie a násobenie desatinných čísel sú presné. Delenie má predvolených 34 desatinných miest a zaokrúhľuje pravidlom half-even.</p>\n"
    "  <h2>Čo môžeš zadať do kalkulačky</h2>\n"
    "  <table><tr><th>Prvok</th><th>Príklady</th></tr>\n"
    "  <tr><td>Celé a desatinné čísla</td><td><code>42</code>, <code>-1.5</code>, <code>.25</code>, <code>1.</code></td></tr>\n"
    "  <tr><td>Vedecký zápis</td><td><code>1.25e-3</code>, <code>6E4</code></td></tr>\n"
    "  <tr><td>Operátory</td><td><code>+</code>, <code>-</code>, <code>*</code>, <code>/</code>; násobenie a delenie majú vyššiu prioritu</td></tr>\n"
    "  <tr><td>Zátvorky a znamienka</td><td><code>(2 + 3) * 4</code>, <code>-(2.5e-1) * 8</code></td></tr></table>\n"
    "  <p>Momentálne nie sú podporované <code>^</code>, <code>%</code>, premenné, funkcie, konštanty ani implicitné násobenie ako <code>2(3 + 4)</code>.</p>\n"
    "  <h2>Lokálne HTTP rozhranie</h2>\n"
    "  <p>Vlastný lokálny klient môže poslať výraz ako obyčajný UTF-8 text na <code>POST /api/evaluate</code>. Vstup má limit 4096 bajtov.</p>\n"
    "  <pre>POST /api/evaluate\nContent-Type: text/plain; charset=utf-8\n\n0.1 + 0.2\n\nHTTP 200\n{\"ok\":true,\"result\":\"0.3\"}</pre>\n"
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
    NUMFORGE_API_PAGE_C_LIBRARY,
    NULL
};

#endif
