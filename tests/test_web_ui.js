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
    const array = source.match(new RegExp(`static const char \\*const ${name}\\[\\]\\s*=\\s*\\{([\\s\\S]*?)\\};`))[1];
    const names = array.match(/NUMFORGE_WEB_PAGE_SCRIPT_\w+/g);
    let script = '';
    for (const part of names) {
        const block = source.match(new RegExp(`static const char ${part}\\[\\] =([\\s\\S]*?);\\r?\\n\\r?\\n`))[1];
        script += block.match(/"(?:\\.|[^"\\])*"/g).map(literal => JSON.parse(literal)).join('');
    }
    assert.ok(script.includes('</script>'), `${name} must contain the complete script`);
    return script.split('<script>')[1].split('</script>')[0];
}

async function createUI(english) {
    const elements = new Map();
    function element(id) {
        if (!elements.has(id)) elements.set(id, {
            value: '', textContent: '', disabled: false, checked: false, className: '',
            selectionStart: 0, selectionEnd: 0, listeners: {}, dataset: {}, style: {}, scrollHeight: 150, clientHeight: 150,
            classList: {toggle() {}},
            addEventListener(event, handler) { this.listeners[event] = handler; },
            setAttribute() {},
            children: [],
            append(child) { this.children.push(child); },
            replaceChildren() { this.children = []; },
            requestSubmit() { return this.listeners.submit({preventDefault() {}}); },
            focus() {},
            setRangeText(text, start, end) { this.value = this.value.slice(0, start) + text + this.value.slice(end); }
        });
        return elements.get(id);
    }
    const pending = [], timers = [], copied = [];
    const clear = element('clear'); clear.dataset.action = 'clear';
    const insert = element('insert'); insert.dataset.insert = '1';
    const angleRad = element('angle-rad'); angleRad.dataset.angle = 'rad';
    const angleDeg = element('angle-deg'); angleDeg.dataset.angle = 'deg';
    element('#precision').value = '10';
    element('#precision-mode').value = 'custom';
    const runtime = {
        document: {
            documentElement: {lang: english ? 'en' : 'sk'},
            querySelector: element,
            createElement: name => element(Symbol(name)),
            querySelectorAll: selector => selector === '[data-action]' ? [clear]
                : selector === '[data-angle]' ? [angleRad, angleDeg]
                : selector === '[data-insert]' ? [insert] : []
        },
        navigator: {clipboard: {writeText: async text => copied.push(text)}},
        window: {isSecureContext: true, location: {reload() {}}}, TextEncoder, AbortController,
        crypto: {getRandomValues: bytes => bytes.fill(1)},
        TypeError, SyntaxError,
        setTimeout: callback => (timers.push(callback), timers.length),
        clearTimeout() {},
        fetch: (url, options) => url.endsWith('&action=start')
            ? Promise.resolve({ok: true, headers: {get: () => 'application/json'}, json: async () => ({ok: true, result: ''})})
            : new Promise((resolve, reject) => pending.push({url, options, resolve, reject}))
    };
    vm.runInNewContext(scriptFor(english), runtime);
    await runtime.ensureSession();
    const preview = value => {
        element('#expression').value = value;
        return runtime.calculate(false);
    };
    const confirm = value => {
        element('#expression').value = value;
        return element('#calculator').listeners.submit({preventDefault() {}});
    };
    const respond = (index, result) => pending[index].resolve({
        ok: true, headers: {get: () => 'application/json'}, json: async () => ({ok: true, result})
    });
    return {element, pending, preview, confirm, respond, clear, insert, timers, copied};
}

async function test(english) {
    const ui = await createUI(english);
    const old = ui.preview('1');
    const current = ui.preview('2');
    assert.equal(ui.pending[0].options.signal.aborted, true);
    ui.respond(1, '2'); await current;
    ui.respond(0, '1'); await old;
    assert.equal(ui.element('#result').textContent, '2');

    const cleared = ui.preview('3');
    ui.clear.listeners.click();
    ui.respond(2, '3'); await cleared;
    assert.equal(ui.element('#result').textContent, '');
    assert.equal(ui.element('#copy-result').disabled, true);

    const changed = ui.preview('4');
    ui.element('#expression').value = '5';
    ui.element('#expression').listeners.input();
    ui.respond(3, '4'); await changed;
    assert.equal(ui.element('#result').textContent, '');

    await ui.preview('π'.repeat(2049));
    assert.equal(ui.pending.length, 4);
    assert.ok(ui.element('#result').textContent.includes('4096'));
    ui.element('#precision').value = '10001';
    await ui.preview('1');
    assert.equal(ui.pending.length, 4);
    assert.ok(ui.element('#result').textContent.includes('10000'));
    ui.element('#precision').value = '10';

    const invalid = ui.preview('6');
    ui.pending[4].resolve({ok: false, headers: {get: () => 'text/plain'}});
    await invalid;
    assert.ok(ui.element('#result').textContent.includes(english ? 'Unexpected server response' : 'Neočakávaná odpoveď servera'));
    const network = ui.preview('7');
    ui.pending[5].reject(new TypeError('Failed to fetch'));
    await network;
    assert.ok(!ui.element('#result').textContent.includes('fetch'));
    assert.ok(ui.element('#result').textContent.includes(english ? 'Cannot contact the server' : 'Nepodarilo sa spojiť'));
    assert.ok(ui.element('#result').textContent.includes('Enter'));

    const success = ui.preview('8'); ui.respond(6, '8'); await success;
    await ui.element('#copy-result').listeners.click();
    assert.deepEqual(ui.copied, ['8']);
    const next = ui.preview('9');
    ui.timers.forEach(callback => callback());
    assert.equal(ui.element('#copy-result').disabled, true);
    ui.respond(7, '9'); await next;
    assert.equal(ui.element('#result').textContent, '9');
    ui.insert.listeners.click();
    assert.equal(ui.element('#result').textContent, '');
}

async function testAutomaticCalculation(english) {
    const ui = await createUI(english);
    const input = ui.element('#expression');
    input.value = '1'; input.listeners.input();
    input.value = '1/8'; input.listeners.input();
    assert.equal(ui.pending.length, 0, 'typing is debounced');
    ui.timers[0]();
    assert.equal(ui.pending.length, 0, 'superseded timer cannot submit');
    const calculation = ui.timers[1]();
    assert.equal(ui.pending.length, 1);
    ui.respond(0, '0.125'); await calculation;

    ui.element('#precision').value = '2';
    ui.element('#precision').listeners.input();
    const rounded = ui.timers.at(-1)();
    assert.ok(ui.pending[1].url.includes('precision=2&angle=rad'));
    ui.respond(1, '0.12'); await rounded;
    ui.element('#precision-mode').value = 'full';
    ui.element('#precision-mode').listeners.change();
    assert.equal(ui.element('#precision').disabled, true);
    const full = ui.timers.at(-1)();
    assert.ok(ui.pending[2].url.includes('precision=full&angle=rad'));
    ui.respond(2, '0.125'); await full;

    input.value = '9'; input.listeners.input();
    const cancelled = ui.timers.at(-1);
    ui.clear.listeners.click(); cancelled();
    assert.equal(ui.pending.length, 3, 'Clear cancels scheduled work');
    input.value = ' '; input.listeners.input();
    await ui.preview('');
    assert.equal(ui.pending.length, 3, 'empty input does not calculate');
    assert.equal(ui.element('#result').textContent, '');
}

async function testErrorContext(english) {
    const cases = [
        ['π/0', 'division by zero', 2, 'π⟦/⟧0'],
        ['sqrt(-1)', 'invalid argument', 1, '⟦sqrt⟧(-1)'],
        ['round(1;1.5)', 'invalid argument', 1, '⟦round⟧(1;1.5)'],
        ['2+', 'syntax error', 3, english ? 'at the end of the expression' : 'na konci výrazu'],
        ['😀+?', 'invalid token', 3, '😀+⟦?⟧'],
    ];
    for (const [input, status, column, excerpt] of cases) {
        const ui = await createUI(english);
        const request = ui.preview(input);
        ui.pending[0].resolve({ok: false, headers: {get: () => 'application/json'},
            json: async () => ({ok: false, status, column})});
        await request;
        assert.ok(ui.element('#result').textContent.includes(excerpt));
        assert.ok(ui.element('#result').textContent.includes(`${english ? 'position' : 'pozícia'} ${column}`));
        if (input === 'sqrt(-1)') assert.ok(ui.element('#result').textContent.includes('x ≥ 0'));
        if (input === 'round(1;1.5)') assert.ok(ui.element('#result').textContent.includes('round(x;n)'));
    }
}

async function testConfirmation(english) {
    const ui = await createUI(english);
    const preview = ui.preview('ans+1');
    assert.ok(ui.pending[0].url.endsWith('&action=preview'));
    ui.respond(0, '6'); await preview;
    assert.equal(ui.element('#history-list').children.length, 0);

    const commit = ui.confirm('ans+1');
    const url = ui.pending[1].url;
    assert.ok(url.endsWith('&action=commit'));
    assert.equal(ui.pending[1].options.signal, undefined, 'confirmation cannot be aborted by editing');
    await ui.confirm('ans+1');
    assert.equal(ui.pending.length, 2, 'only one confirmation is in flight');
    ui.pending[1].reject(new TypeError('response lost'));
    await commit;
    assert.match(ui.element('#result').textContent, english ? /uncertain/ : /neisté/);
    await ui.preview('999');
    assert.equal(ui.pending.length, 2, 'uncertain confirmation blocks previews');
    const retry = ui.confirm('999');
    assert.equal(ui.pending[2].url, url);
    assert.equal(ui.pending[2].options.body, 'ans+1');
    assert.equal(ui.element('#expression').value, 'ans+1', 'retry restores the captured expression');
    ui.respond(2, '6'); await retry;
    assert.equal(ui.element('#history-list').children.length, 1);
    const item = ui.element('#history-list').children[0].children[0];
    assert.equal(item.textContent, 'ans+1 → 6');
    item.listeners.click();
    assert.equal(ui.element('#expression').value, 'ans+1');
    assert.equal(ui.pending.length, 3, 'history restores input without confirming');

    const next = ui.confirm('ans+1');
    ui.element('#expression').value = '2+2';
    ui.element('#expression').listeners.input();
    ui.respond(3, '7'); await next;
    assert.equal(ui.element('#history-list').children.length, 2, 'late confirmation still enters history');
    assert.equal(ui.element('#result').textContent, '', 'late result does not replace newer input preview');
}

(async () => {
    for (const english of [false, true]) {
        for (const data of [null, {ok: true}, {ok: true, result: 42}, {ok: 'true'}]) {
            const ui = await createUI(english);
            const request = ui.preview('1');
            ui.pending[0].resolve({ok: true, headers: {get: () => 'application/json'}, json: async () => data});
            await request;
            assert.ok(ui.element('#result').textContent.includes(english ? 'Unexpected' : 'Neočakávaná'));
            assert.equal(ui.element('#copy-result').disabled, true);
        }
        const ui = await createUI(english);
        ui.element('angle-deg').listeners.click();
        const request = ui.preview('1');
        assert.ok(ui.pending[0].url.includes('angle=deg'));
        ui.pending[0].resolve({ok: true, headers: {get: () => 'application/json'}, json: async () => { throw new SyntaxError(); }});
        await request;
        assert.ok(ui.element('#result').textContent.includes(english ? 'Unexpected' : 'Neočakávaná'));
    }
    await test(false);
    await test(true);
    await testAutomaticCalculation(false);
    await testAutomaticCalculation(true);
    await testErrorContext(false);
    await testErrorContext(true);
    await testConfirmation(false);
    await testConfirmation(true);
    console.log('SK/EN web UI regressions passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
