FROM python:3.12-slim-bookworm AS xml-source
WORKDIR /build
COPY deploy/monitor-xml.json deploy/prepare_monitor_xml.py /build/
RUN python prepare_monitor_xml.py

FROM numforge-uptime:jwt-20261106
USER root
COPY --from=xml-source /out/ /tmp/xml-source/
COPY deploy/monitor-xml.json deploy/install_monitor_xml.cjs /tmp/
RUN node /tmp/install_monitor_xml.cjs && rm -rf /tmp/xml-source /tmp/install_monitor_xml.cjs /tmp/monitor-xml.json
USER node
