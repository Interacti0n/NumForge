// Exercise the dependency guard on isolated fixtures, never on working sources.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {checkArchitecture} = require('../scripts/check_architecture.cjs');
const root = fs.mkdtempSync(path.join(os.tmpdir(), 'numforge-architecture-'));
const write = (name, text) => {
    const file = path.join(root, name);
    fs.mkdirSync(path.dirname(file), {recursive: true}); fs.writeFileSync(file, text);
};
const layouts = {
    'src/runtime': 'runtime.c', 'src/core': 'core.c', 'src/engine': 'api/engine.c',
    'src/application': 'application.c', 'apps/cli': 'main.c',
    'apps/web/http': 'http.c', 'apps/web/server': 'server.c',
    'bindings/python': 'native.c', 'src/modules/numerical': 'numerical.c'
};
let cases = 0;
function mutation(name, changes, pattern) {
    const previous = new Map();
    try {
        for (const [file, content] of Object.entries(changes)) {
            const target = path.join(root, file);
            previous.set(file, fs.existsSync(target) ? fs.readFileSync(target) : null);
            write(file, content);
        }
        const errors = checkArchitecture(root).errors;
        if (pattern) assert.ok(errors.some(error => pattern.test(error)), `${name}: ${errors.join('\n')}`);
        else assert.deepEqual(errors, [], name);
        cases++;
    } finally {
        for (const [file, content] of previous) {
            if (content === null) fs.unlinkSync(path.join(root, file));
            else write(file, content);
        }
    }
}
try {
    for (const [directory, file] of Object.entries(layouts)) {
        write(directory + '/' + file, '');
        write(directory + '/CMakeLists.txt', `target_sources(example PRIVATE "${file}")\n`);
    }
    write('CMakeLists.txt', '');
    write('src/modules/CMakeLists.txt', 'add_subdirectory(numerical)\n');
    write('include/numforge/api.h', '');
    write('src/core/core_internal.h', '');
    write('src/modules/numerical/numerical_internal.h', '');
    mutation('valid minimal tree', {}, null);
    mutation('upward dependency', {'src/core/core.c': '#include "src/application/application.c"'}, /forbidden upward/);
    mutation('public header leak', {'include/numforge/api.h': '#include "src/core/core_internal.h"'}, /public header includes private/);
    mutation('Python private dependency', {'bindings/python/native.c': '#include "src/engine/api/engine.c"'}, /Python adapters must use public/);
    mutation('module-private dependency', {'src/application/application.c': '#include "src/modules/numerical/numerical_internal.h"'}, /module-private dependency/);
    mutation('unqualified private include', {'src/application/application.c': '#include "core_internal.h"'}, /repository-qualified private include/);
    mutation('relative include', {'src/core/core.c': '#include "..\/runtime\/runtime.c"'}, /repository-qualified private include/);
    mutation('core storage dependency', {'src/application/application.c': '#include "src/core/core_internal.h"'}, /core storage is private/);
    mutation('missing include', {'src/core/core.c': '#include "src/core/missing.h"'}, /missing src\/core\/missing.h/);
    mutation('public numeric dependency', {'src/application/application.c': '#include <numforge/api.h>'}, null);
    for (const [directory, file] of Object.entries(layouts)) {
        mutation('unregistered source: ' + directory, {[directory + '/orphan.c']: ''}, /absent from .* manifest/);
        mutation('comment cannot register: ' + directory,
            {[directory + '/orphan.c']: '', [directory + '/CMakeLists.txt']: `target_sources(example PRIVATE ${file})\n# orphan.c\n`},
            /absent from .* manifest/);
        mutation('missing registered source: ' + directory,
            {[directory + '/CMakeLists.txt']: `target_sources(example PRIVATE ${file} missing.c)\n`}, /missing registered source/);
    }
    mutation('comment cannot register a module', {'src/modules/CMakeLists.txt': '# add_subdirectory(numerical)\n'}, /absent from module index/);
    mutation('unregistered instrumentation', {'src/runtime/instrumentation/probe.c': ''}, /absent from benchmark instrumentation/);
    mutation('unowned application source', {'apps/orphan.c': ''}, /no owning source manifest/);
    mutation('unowned module source', {'src/modules/orphan.c': ''}, /no owning source manifest/);
    mutation('registered instrumentation', {'src/runtime/instrumentation/probe.c': '',
        'CMakeLists.txt': 'target_sources(example PRIVATE src/runtime/instrumentation/probe.c)'}, null);
    assert.deepEqual(checkArchitecture(path.resolve(__dirname, '..')).errors, []);
    console.log(`${cases} isolated architecture guard cases passed; repository dependencies pass.`);
} finally {
    // The generated directory is a direct child of the OS temp directory.
    assert.equal(path.dirname(path.resolve(root)), path.resolve(os.tmpdir()));
    assert.ok(path.basename(root).startsWith('numforge-architecture-'));
    fs.rmSync(root, {recursive: true});
}
