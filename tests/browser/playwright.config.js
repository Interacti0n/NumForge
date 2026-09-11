const { defineConfig } = require('@playwright/test');
const path = require('node:path');
if (!process.env.NUMFORGE_WEB_EXECUTABLE) throw Error('Set NUMFORGE_WEB_EXECUTABLE to the built server');
const executable = path.resolve(process.env.NUMFORGE_WEB_EXECUTABLE);
const port = Number(process.env.NUMFORGE_TEST_PORT || 18765);
if (!Number.isInteger(port) || port < 1024 || port > 65535) throw Error('Invalid test port');
module.exports = defineConfig({
    testDir: '.', testMatch: '*.spec.js', workers: 1, retries: 0, timeout: 15000,
    reporter: [['list'], ['html', { open: 'never' }]],
    use: { baseURL: `http://127.0.0.1:${port}`, browserName: 'chromium',
        trace: 'retain-on-failure', screenshot: 'only-on-failure',
        permissions: ['clipboard-read', 'clipboard-write'] },
    webServer: { command: `"${executable}" --no-browser --port ${port}`,
        url: `http://127.0.0.1:${port}`, reuseExistingServer: false, timeout: 15000 },
});
