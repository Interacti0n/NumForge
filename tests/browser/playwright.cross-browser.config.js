const {defineConfig}=require('@playwright/test');
const base=require('./playwright.config');

// Keep the default Chromium suite unchanged. Clipboard permissions are not
// supported by every engine and are not needed by graph/guide checks.
module.exports=defineConfig({...base,
    testMatch: /(?:graph.*|guide)\.spec\.js$/,
    outputDir: 'test-results-cross-browser',
    reporter: [['list'], ['html', {open:'never', outputFolder:'playwright-report-cross-browser'}]],
    use: {...base.use, permissions:[]},
    projects: ['firefox','webkit'].map(browserName=>({name:browserName,use:{browserName}})),
});
