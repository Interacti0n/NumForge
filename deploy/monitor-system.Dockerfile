# Maintenance overlay: consume the already qualified accounts SQLite artifact.
ARG SQLITE_IMAGE=numforge-accounts:apt-20261105
ARG BASE_IMAGE=numforge-uptime:apt-20261105
FROM ${SQLITE_IMAGE} AS sqlite-artifact
FROM ${BASE_IMAGE}
USER root
RUN python3 -c "from pathlib import Path; import hashlib; assert hashlib.sha256(Path('/usr/lib/x86_64-linux-gnu/libsqlite3.so.0.8.6').read_bytes()).hexdigest()=='2e6eef9a727f081f0d453b4e5e6cbd8b9ef8b6f86cbf7681cbad444d3b0b55c8'"
COPY --from=sqlite-artifact /usr/lib/x86_64-linux-gnu/libsqlite3.so.0.8.6 /usr/lib/x86_64-linux-gnu/libsqlite3.so.0.8.6
COPY --from=sqlite-artifact /opt/numforge/system-sqlite-backport.json /opt/numforge/system-sqlite-backport.json
RUN python3 -c "from pathlib import Path; import hashlib; assert hashlib.sha256(Path('/usr/lib/x86_64-linux-gnu/libsqlite3.so.0.8.6').read_bytes()).hexdigest()=='f4b32d83fadc4787cabff8a115e4aca3ac808efe9d9521b9630f1d8815f129b5'"
COPY deploy/backport_monitor_jwt.py /tmp/backport_monitor_jwt.py
RUN python3 /tmp/backport_monitor_jwt.py && rm /tmp/backport_monitor_jwt.py
USER node
