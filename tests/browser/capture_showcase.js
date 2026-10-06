// Capture the real local calculator for project documentation. No UI mocks.
const { chromium, expect } = require('@playwright/test');
const { spawn } = require('node:child_process');
const net = require('node:net');
const fs = require('node:fs');
const path = require('node:path');
const executable = process.argv[2];
if (!executable) throw Error('Usage: node tests/browser/capture_showcase.js path-to-numforge_web');
const root = path.resolve(__dirname, '../..');
const directory = path.join(root, 'docs/images');
const port = 18773;
const url = `http://127.0.0.1:${port}`;
let server;
let serverError = '';
let browser;
async function confirm(page, expression, expected) {
    await page.locator('#expression').fill(expression);
    await expect(page.locator('#result')).toHaveText(expected);
    await page.locator('#expression').press('Enter');
    await expect(page.locator('#result')).toHaveText(expected);
}
(async () => {
    try {
        // Fail before opening a browser if this port belongs to another server.
        await new Promise((resolve, reject) => {
            const probe = net.createServer();
            probe.once('error', reject);
            probe.listen(port, '127.0.0.1', () => probe.close(resolve));
        });
        server = spawn(path.resolve(executable), ['--no-browser', '--port', String(port)], {
            windowsHide: true, stdio: ['ignore', 'ignore', 'pipe']
        });
        server.stderr.on('data', chunk => { serverError += chunk.toString(); });
        server.on('error', error => { serverError += error.message; });
        const deadline = Date.now() + 15000;
        while (true) {
            if (server.exitCode !== null || serverError) throw Error(serverError || 'Server exited');
            try { if ((await fetch(url)).ok) break; } catch {}
            if (Date.now() > deadline) throw Error('Local server did not start');
            await new Promise(resolve => setTimeout(resolve, 100));
        }
        fs.mkdirSync(directory, { recursive: true });
        browser = await chromium.launch();
        for (const [name, viewport] of [
            ['calculator-desktop', { width: 1600, height: 1000 }],
            ['calculator-mobile', { width: 390, height: 844 }]
        ]) {
            const context = await browser.newContext({ viewport, deviceScaleFactor: 1,
                reducedMotion: 'reduce', isMobile: name.endsWith('mobile'), hasTouch: name.endsWith('mobile') });
            const page = await context.newPage();
            await page.goto(url + '/?lang=en');
            await expect(page.locator('body')).toHaveCSS('visibility', 'visible');
            await page.locator('#precision-mode').selectOption('full');
            await confirm(page, '0.1+0.2', '0.3');
            await confirm(page, '2^128', '3.40282366920938463463374607431768211456E+38');
            await page.locator('#notation-mode').selectOption('fraction');
            await confirm(page, 'sqrt(4/9)', '2/3');
            await confirm(page, '1/3+1/6', '1/2');
            await expect(page.locator('#history-list li')).toHaveCount(4);
            await expect(page.locator('#result-approx')).toHaveText('≈ 0.5');
            await page.locator('#expression').blur();
            await page.evaluate(() => document.fonts.ready);
            await page.screenshot({ path: path.join(directory, name + '.png'), fullPage: true });
            await context.close();
        }
        console.log('Captured verified desktop and mobile calculator screenshots.');
    } finally {
        if (browser) await browser.close();
        if (server && server.pid && server.exitCode === null) {
            const stopped = new Promise(resolve => server.once('exit', resolve));
            server.kill(); await stopped;
        }
    }
})().catch(error => { console.error(error); process.exitCode = 1; });
