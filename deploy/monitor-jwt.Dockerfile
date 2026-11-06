FROM python:3.12-slim-bookworm AS jwt-source
WORKDIR /build
COPY deploy/monitor-jwt.json deploy/prepare_monitor_jwt.py /build/
RUN python prepare_monitor_jwt.py

FROM numforge-uptime:binding-20261105
USER root
RUN python3 -c "import jwt,jwt.jwks_client,hashlib,json,pathlib; assert jwt.__version__=='2.6.0'; r=json.loads(pathlib.Path('/opt/numforge/system-jwt-backport.json').read_text()); assert hashlib.sha256(pathlib.Path(jwt.jwks_client.__file__).read_bytes()).hexdigest()==r['afterSha256']; assert not pathlib.Path('/usr/local/lib/python3.11/dist-packages/jwt').exists()"
COPY --from=jwt-source /out/ /usr/local/lib/python3.11/dist-packages/
COPY --from=jwt-source /receipt.json /opt/numforge/system-jwt-upgrade.json
RUN python3 -c "import jwt,importlib.metadata as m; assert jwt.__version__=='2.15.1'; assert m.version('PyJWT')=='2.15.1'; assert jwt.__file__.startswith('/usr/local/lib/python3.11/dist-packages/jwt/')"
USER node
