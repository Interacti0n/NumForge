#ifndef NUMFORGE_WEB_PAGE_H
#define NUMFORGE_WEB_PAGE_H

/*
------------------------------------------------------------------------------------------------------------------------------
    Single-page local interface embedded in the server executable. Keeping the
    page here makes numforge_web self-contained: no Node.js, assets, or working
    directory setup is required to use the local calculator.
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
    "    p { color: #adb5c3; }\n"
    "    form { display: flex; gap: 10px; margin-top: 28px; }\n"
    "    input { min-width: 0; flex: 1; padding: 13px; border: 1px solid #3b4352; border-radius: 8px; background: #1c2028; color: inherit; font: 1rem ui-monospace, monospace; }\n"
    "    button { padding: 12px 18px; border: 0; border-radius: 8px; background: #ff9d36; color: #17110a; font-weight: 700; cursor: pointer; }\n"
    "    #result { min-height: 1.5em; margin-top: 22px; padding: 16px; border-radius: 8px; background: #1c2028; font: 1.1rem ui-monospace, monospace; overflow-wrap: anywhere; }\n"
    "    #result.error { color: #ff8888; }\n"
    "    small { display: block; margin-top: 22px; color: #747d8d; }\n"
    "  </style>\n"
    "</head>\n"
    "<body>\n"
    "  <h1>NumForge</h1>\n"
    "  <p>Lokálna kalkulačka poháňaná priamo C jadrom a BigDecimal.</p>\n"
    "  <form id=\"calculator\">\n"
    "    <input id=\"expression\" aria-label=\"Matematický výraz\" value=\"0.1 + 0.2\" autocomplete=\"off\" autofocus>\n"
    "    <button type=\"submit\">Vypočítať</button>\n"
    "  </form>\n"
    "  <output id=\"result\" aria-live=\"polite\">Pripravené.</output>\n"
    "  <small>Podporované: +, -, *, /, zátvorky a desatinné čísla. Server beží len na tvojom počítači.</small>\n"
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
    "        result.textContent = '= ' + data.result;\n"
    "      } catch (error) {\n"
    "        result.className = 'error'; result.textContent = 'Chyba: ' + error.message;\n"
    "      }\n"
    "    });\n"
    "  </script>\n"
    "</body>\n"
    "</html>\n";

#endif
