FROM python:3.12-slim-bookworm AS sqlite-build
USER root
RUN sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/debian.sources \
    && apt-get update && apt-get build-dep -y --no-install-recommends sqlite3
WORKDIR /build
RUN apt-get source sqlite3=3.40.1-2+deb12u2
COPY deploy/backport_accounts_sqlite.py /build/backport.py
RUN python /build/backport.py
RUN --network=none cd /build/sqlite3-3.40.1 && DEB_BUILD_OPTIONS=nocheck dpkg-buildpackage -b -uc -us -j2
RUN mkdir /out && dpkg-deb -x /build/libsqlite3-0_3.40.1-2+deb12u2_amd64.deb /out

FROM python:3.12-slim-bookworm
WORKDIR /service
COPY apps/accounts/requirements.txt deploy/accounts-requirements.txt ./
RUN apt-get update && apt-get upgrade -y --no-install-recommends \
    && rm -rf /var/lib/apt/lists/* \
    && python -m pip install --no-cache-dir --upgrade pip==26.3 \
    && python -m pip install --no-cache-dir -r accounts-requirements.txt \
    && python -m pip check \
    && python -m pip uninstall --yes pip \
    && python -c "import ensurepip, pathlib, shutil; shutil.rmtree(pathlib.Path(ensurepip.__file__).parent)"
# Reject a changed distribution library before applying the qualified backport.
RUN python -c "import pathlib,hashlib;assert hashlib.sha256(pathlib.Path('/usr/lib/x86_64-linux-gnu/libsqlite3.so.0.8.6').read_bytes()).hexdigest()=='2e6eef9a727f081f0d453b4e5e6cbd8b9ef8b6f86cbf7681cbad444d3b0b55c8'"
COPY --from=sqlite-build /out/usr/lib/x86_64-linux-gnu/libsqlite3.so.0.8.6 /usr/lib/x86_64-linux-gnu/libsqlite3.so.0.8.6
COPY --from=sqlite-build /build/backport-receipt.json /opt/numforge/system-sqlite-backport.json
COPY apps/accounts/ ./
COPY scripts/accounts_db.py deploy/accounts_setup.py /support/
USER 10001:10001
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1 DJANGO_SETTINGS_MODULE=config.settings
CMD ["gunicorn", "config.wsgi:application", "--bind", "0.0.0.0:8000", "--workers", "1", "--threads", "2", "--worker-tmp-dir", "/tmp", "--no-control-socket"]
