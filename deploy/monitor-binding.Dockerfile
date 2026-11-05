FROM python:3.12-slim-bookworm AS source
WORKDIR /build
COPY deploy/monitor-binding.json deploy/monitor-sqlite.json deploy/prepare_monitor_binding.py /build/
RUN python prepare_monitor_binding.py

FROM numforge-uptime:system-20261105 AS builder
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

FROM numforge-uptime:system-20261105
USER root
RUN rm -rf /app/node_modules/@louislam/sqlite3
COPY --from=builder /app/node_modules/@louislam/sqlite3 /app/node_modules/@louislam/sqlite3
COPY --from=builder /app/package.json /app/package-lock.json /app/
COPY --from=builder /app/node_modules/.package-lock.json /app/node_modules/.package-lock.json
COPY --from=builder /tmp/monitor-sqlite-receipt.json /opt/numforge/monitor-sqlite.json
USER node
