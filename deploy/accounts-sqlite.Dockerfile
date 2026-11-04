FROM numforge-accounts:maintenance-20261103 AS builder
USER root
RUN sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/debian.sources \
    && apt-get update && apt-get build-dep -y --no-install-recommends sqlite3
WORKDIR /build
RUN apt-get source sqlite3=3.40.1-2+deb12u2
COPY deploy/backport_accounts_sqlite.py /build/backport.py
RUN python /build/backport.py
RUN --network=none cd /build/sqlite3-3.40.1 && DEB_BUILD_OPTIONS=nocheck dpkg-buildpackage -b -uc -us -j2
RUN mkdir /out && dpkg-deb -x /build/libsqlite3-0_3.40.1-2+deb12u2_amd64.deb /out
FROM numforge-accounts:maintenance-20261103
RUN python -c "import pathlib,hashlib;assert hashlib.sha256(pathlib.Path('/usr/lib/x86_64-linux-gnu/libsqlite3.so.0.8.6').read_bytes()).hexdigest()=='2e6eef9a727f081f0d453b4e5e6cbd8b9ef8b6f86cbf7681cbad444d3b0b55c8'"
COPY --from=builder /out/usr/lib/x86_64-linux-gnu/libsqlite3.so.0.8.6 /usr/lib/x86_64-linux-gnu/libsqlite3.so.0.8.6
COPY --from=builder /build/backport-receipt.json /opt/numforge/system-sqlite-backport.json
