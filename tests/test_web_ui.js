'use strict';

// Execute the actual embedded page script with a minimal DOM and controlled
// network promises. No framework or browser installation is required.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = fs.readFileSync(path.join(__dirname, '../src/web/web_page.h'), 'utf8');

function scriptFor(english) {
    const name = english ? 'NUMFORGE_WEB_PAGE_EN' : 'NUMFORGE_WEB_PAGE';
    const array = source.match(new RegExp(`static const char \\*const ${name}\\[\\] = \\{([\\s\\S]*?)\\};`))[1];
    const names = array.match(/NUMFORGE_WEB_PAGE_SCRIPT_\w+/g);
    let script = '';
    for (const part of names) {
        const block = source.match(new RegExp(`static const char ${part}\\[\\] =([\\s\\S]*?);\\r?\\n\\r?\\n`))[1];
        script += block.match(/"(?:\\.|[^"\\])*"/g).map(literal => JSON.parse(literal)).join('');
    }
    assert.ok(script.includes('</script>'), `${name} must contain the complete script`);
    return script.split('<script>')[1].split('</script>')[0];
}

function createUI(english) {
    const elements = new Map();
    function element(id) {
        if (!elements.has(id)) elements.set(id, {
            value: '', textContent: '', disabled: false, checked: false, className: '',
            selectionStart: 0, selectionEnd: 0, listeners: {}, dataset: {},
            addEventListener(event, handler) { this.listeners[event] = handler; },
            focus() {},
            setRangeText(text, start, end) { this.value = this.value.slice(0, start) + text + this.value.slice(end); }
        });
        return elements.get(id);
    }
    const pending = [], timers = [], copied = [];
    const clear = element('clear'); clear.dataset.action = 'clear';
    const insert = element('insert'); insert.dataset.insert = '1';
    element('#precision').value = '10';
    vm.runInNewContext(scriptFor(english), {
        document: {
            documentElement: {lang: english ? 'en' : 'sk'},
            querySelector: element,
            querySelectorAll: selector => selector === '[data-action]' ? [clear] : [insert]
        },
        navigator: {clipboard: {writeText: async text => copied.push(text)}},
        window: {isSecureContext: true}, TextEncoder, AbortController,
        TypeError, SyntaxError,
        setTimeout: callback => (timers.push(callback), timers.length),
        clearTimeout() {},
        fetch: (url, options) => new Promise((resolve, reject) => pending.push({url, options, resolve, reject}))
    });
    const submit = value => {
        element('#expression').value = value;
        return element('#calculator').listeners.submit({preventDefault() {}});
    };
    const respond = (index, result) => pending[index].resolve({
        ok: true, headers: {get: () => 'application/json'}, json: async () => ({ok: true, result})
    });
    return {element, pending, submit, respond, clear, insert, timers, copied};
}

async function test(english) {
    const ui = createUI(english);
    const old = ui.submit('1');
    const current = ui.submit('2');
    assert.equal(ui.pending[0].options.signal.aborted, true);
    ui.respond(1, '2'); await current;
    ui.respond(0, '1'); await old;
    assert.equal(ui.element('#result').textContent, '2');

    const cleared = ui.submit('3');
    ui.clear.listeners.click();
    ui.respond(2, '3'); await cleared;
    assert.equal(ui.element('#result').textContent, '');
    assert.equal(ui.element('#copy-result').disabled, true);

    const changed = ui.submit('4');
    ui.element('#expression').value = '5';
    ui.element('#expression').listeners.input();
    ui.respond(3, '4'); await changed;
    assert.equal(ui.element('#result').textContent, '');

    await ui.submit('π'.repeat(2049));
    assert.equal(ui.pending.length, 4);
    assert.ok(ui.element('#result').textContent.includes('4096'));
    ui.element('#precision').value = '10001';
    await ui.submit('1');
    assert.equal(ui.pending.length, 4);
    assert.ok(ui.element('#result').textContent.includes('10000'));
    ui.element('#precision').value = '10';

    const invalid = ui.submit('6');
    ui.pending[4].resolve({ok: false, headers: {get: () => 'text/plain'}});
    await invalid;
    assert.equal(ui.element('#result').textContent, english ? 'Error: Calculation failed.' : 'Chyba: Výpočet zlyhal.');
    const network = ui.submit('7');
    ui.pending[5].reject(new TypeError('Failed to fetch'));
    await network;
    assert.ok(!ui.element('#result').textContent.includes('fetch'));

    const success = ui.submit('8'); ui.respond(6, '8'); await success;
    await ui.element('#copy-result').listeners.click();
    assert.deepEqual(ui.copied, ['8']);
    const next = ui.submit('9');
    ui.timers.forEach(callback => callback());
    assert.equal(ui.element('#copy-result').disabled, true);
    ui.respond(7, '9'); await next;
    assert.equal(ui.element('#result').textContent, '9');
    ui.insert.listeners.click();
    assert.equal(ui.element('#result').textContent, '');
}

(async () => {
    await test(false);
    await test(true);
    console.log('SK/EN web UI regressions passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
