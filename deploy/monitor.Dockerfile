FROM python:3.12-slim-bookworm AS source
WORKDIR /build
COPY deploy/monitor-binding.json deploy/monitor-sqlite.json deploy/prepare_monitor_binding.py /build/
RUN python prepare_monitor_binding.py

FROM python:3.12-slim-bookworm AS system-sqlite-build
USER root
RUN sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/debian.sources \
    && apt-get -o Acquire::Retries=0 update && apt-get -o Acquire::Retries=0 build-dep -y --no-install-recommends sqlite3
WORKDIR /build
RUN apt-get -o Acquire::Retries=0 source sqlite3=3.40.1-2+deb12u2
COPY deploy/backport_accounts_sqlite.py /build/backport.py
RUN python /build/backport.py
RUN --network=none cd /build/sqlite3-3.40.1 && DEB_BUILD_OPTIONS=nocheck dpkg-buildpackage -b -uc -us -j2
RUN mkdir /out && dpkg-deb -x /build/libsqlite3-0_3.40.1-2+deb12u2_amd64.deb /out

FROM louislam/uptime-kuma:2.5.9-slim@sha256:c41c46667c116066ad252b4887e1c4c66cca8891ca17f3161c1d16540645d1cc AS patched
COPY deploy/monitor-patches.json /opt/numforge/monitor-patches.json
COPY deploy/patch_monitor.cjs /tmp/patch_monitor.cjs
RUN node /tmp/patch_monitor.cjs prepare /opt/numforge/monitor-patches.json /tmp/monitor-before.json \
    && npm install --omit=dev --ignore-scripts --no-audit --no-fund \
    && node /tmp/patch_monitor.cjs verify /opt/numforge/monitor-patches.json /tmp/monitor-before.json \
    && rm -f /tmp/patch_monitor.cjs /tmp/monitor-before.json \
    && rm -rf /root/.npm
RUN apt-get -o Acquire::Retries=0 update && apt-get -o Acquire::Retries=0 upgrade -y --no-install-recommends \
    && rm -rf /var/lib/apt/lists/* \
    && rm -f /app/extra/healthcheck /usr/bin/cloudflared /usr/local/bin/npm /usr/local/bin/npx \
    && rm -rf /usr/local/lib/node_modules/npm
FROM patched AS sqlite-build
USER root
RUN apt-get -o Acquire::Retries=0 update && apt-get -o Acquire::Retries=0 install -y --no-install-recommends g++ make python3 \
    && rm -rf /var/lib/apt/lists/*
COPY deploy/build_monitor_sqlite.cjs deploy/install_monitor_binding.cjs deploy/monitor-binding.json deploy/monitor-sqlite.json /tmp/
RUN node /tmp/install_monitor_binding.cjs check-base && rm -rf /app/node_modules/@louislam/sqlite3
COPY --from=source /out/binding /app/node_modules/@louislam/sqlite3
RUN node /tmp/install_monitor_binding.cjs install
RUN --network=none cd /app/node_modules/@louislam/sqlite3 \
    && CFLAGS='-DSQLITE_MAX_FUNCTION_ARG=127 -DSQLITE_MAX_PAGE_COUNT=1073741823 -DSQLITE_MAX_MMAP_SIZE=0x7fff0000' \
       node /app/node_modules/node-gyp/bin/node-gyp.js rebuild --nodedir=/usr/local --jobs=2
RUN --network=none node /tmp/install_monitor_binding.cjs verify
RUN cp /app/node_modules/@louislam/sqlite3/build/Release/vscode-sqlite3.node /tmp/binding-native.node \
    && rm -rf /app/node_modules/@louislam/sqlite3/build \
    && mkdir -p /app/node_modules/@louislam/sqlite3/build/Release \
    && mv /tmp/binding-native.node /app/node_modules/@louislam/sqlite3/build/Release/vscode-sqlite3.node

FROM patched
RUN python3 -c "from pathlib import Path; import hashlib; assert hashlib.sha256(Path('/usr/lib/x86_64-linux-gnu/libsqlite3.so.0.8.6').read_bytes()).hexdigest()=='2e6eef9a727f081f0d453b4e5e6cbd8b9ef8b6f86cbf7681cbad444d3b0b55c8'"
COPY --from=system-sqlite-build /out/usr/lib/x86_64-linux-gnu/libsqlite3.so.0.8.6 /usr/lib/x86_64-linux-gnu/libsqlite3.so.0.8.6
COPY --from=system-sqlite-build /build/backport-receipt.json /opt/numforge/system-sqlite-backport.json
COPY deploy/backport_monitor_jwt.py /tmp/backport_monitor_jwt.py
RUN python3 /tmp/backport_monitor_jwt.py && rm /tmp/backport_monitor_jwt.py
RUN rm -rf /app/node_modules/@louislam/sqlite3
COPY --from=sqlite-build /app/node_modules/@louislam/sqlite3 /app/node_modules/@louislam/sqlite3
COPY --from=sqlite-build /app/package.json /app/package-lock.json /app/
COPY --from=sqlite-build /app/node_modules/.package-lock.json /app/node_modules/.package-lock.json
COPY --from=sqlite-build /tmp/monitor-sqlite-receipt.json /opt/numforge/monitor-sqlite.json
USER node
# These NumForge HTTP/TCP/push monitors use local HTTP; reuse the Node runtime.
HEALTHCHECK --interval=30s --timeout=10s --start-period=60s --retries=3 \
    CMD ["node", "-e", "fetch('http://127.0.0.1:3001/',{signal:AbortSignal.timeout(5000)}).then(r=>process.exit(r.ok?0:1)).catch(()=>process.exit(1))"]
