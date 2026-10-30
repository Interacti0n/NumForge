// Provision inside Kuma; credentials arrive via stdin, never command arguments.
const fs = require('node:fs');
const { io } = require('/app/node_modules/socket.io-client');
const config = JSON.parse(fs.readFileSync(0, 'utf8'));
const socket = io('http://127.0.0.1:3001', { transports: ['websocket'], reconnection: false });
const timer = setTimeout(() => { console.error('Monitor setup timed out'); process.exit(1); }, 60000);
const call = (name, ...args) => new Promise((resolve, reject) => {
    socket.timeout(15000).emit(name, ...args, (error, result) => {
        if (error || result?.ok === false) reject(error || new Error(result.msg));
        else resolve(result);
    });
});
socket.once('connect', async () => {
    try {
        if (await call('needSetup')) await call('setup', config.username, config.password);
        await call('login', { username: config.username, password: config.password });
        const listed = new Promise(resolve => socket.once('monitorList', resolve));
        await call('getMonitorList');
        const monitors = Object.values(await listed);
        const defaults = { active: true, interval: 60, retryInterval: 20, maxretries: 2,
            timeout: 15, method: 'GET', accepted_statuscodes: ['200-299'],
            notificationIDList: {}, parent: null, maxredirects: 3, conditions: [],
            kafkaProducerBrokers: [], kafkaProducerSaslOptions: {}, rabbitmqNodes: [] };
        for (const monitor of config.monitors) {
            const existing = monitors.find(item => item.name === monitor.name);
            await call(existing ? 'editMonitor' : 'add', { ...defaults, ...existing, ...monitor });
        }
        console.log(`Configured ${config.monitors.length} local monitors.`);
    } catch (error) {
        console.error(error.message);
        process.exitCode = 1;
    } finally {
        clearTimeout(timer);
        socket.disconnect();
    }
});
socket.once('connect_error', error => { console.error(error.message); clearTimeout(timer); socket.disconnect(); process.exitCode = 1; });
