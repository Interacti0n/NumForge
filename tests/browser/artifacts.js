const fs = require('node:fs');
const path = require('node:path');
const directory = path.resolve(__dirname, '../../build/browser-artifacts');

// Resolve diagnostics independently of the checkout location and test runner cwd.
exports.artifactPath = name => {
    fs.mkdirSync(directory, {recursive: true});
    return path.join(directory, name);
};
