FROM louislam/uptime-kuma:2.5.9-slim@sha256:c41c46667c116066ad252b4887e1c4c66cca8891ca17f3161c1d16540645d1cc
COPY deploy/monitor-patches.json /opt/numforge/monitor-patches.json
COPY deploy/patch_monitor.cjs /tmp/patch_monitor.cjs
RUN node /tmp/patch_monitor.cjs prepare /opt/numforge/monitor-patches.json /tmp/monitor-before.json \
    && npm install --omit=dev --ignore-scripts --no-audit --no-fund \
    && node /tmp/patch_monitor.cjs verify /opt/numforge/monitor-patches.json /tmp/monitor-before.json \
    && rm -f /tmp/patch_monitor.cjs /tmp/monitor-before.json \
    && rm -rf /root/.npm
RUN apt-get update && apt-get upgrade -y --no-install-recommends \
    && rm -rf /var/lib/apt/lists/* \
    && rm -f /app/extra/healthcheck /usr/bin/cloudflared /usr/local/bin/npm /usr/local/bin/npx \
    && rm -rf /usr/local/lib/node_modules/npm
USER node
# These NumForge HTTP/TCP/push monitors use local HTTP; reuse the Node runtime.
HEALTHCHECK --interval=30s --timeout=10s --start-period=60s --retries=3 \
    CMD ["node", "-e", "fetch('http://127.0.0.1:3001/',{signal:AbortSignal.timeout(5000)}).then(r=>process.exit(r.ok?0:1)).catch(()=>process.exit(1))"]
