FROM python:3.12-slim-bookworm AS xml-build
RUN apt-get -o Acquire::Retries=0 update \
    && apt-get -o Acquire::Retries=0 install -y --no-install-recommends \
       build-essential pkg-config libicu-dev zlib1g-dev liblzma-dev patch \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /build
COPY deploy/postgres-libxml.json deploy/build_postgres_libxml.py deploy/check_libxml.c /build/
ENV CFLAGS="-O2 -g -fstack-protector-strong -Wformat -Werror=format-security -D_FORTIFY_SOURCE=2 -ffile-prefix-map=/build=." \
    LDFLAGS="-Wl,-z,relro,-z,now"
RUN python /build/build_postgres_libxml.py prepare
RUN --network=none cd /build/libxml2-2.13.9 && make -j2 testparser \
    && ulimit -c 0 && python /build/build_postgres_libxml.py negative
RUN --network=none python /build/build_postgres_libxml.py apply \
    && cd /build/libxml2-2.13.9 && make -j2 \
    && ./testparser && timeout 180 make runtests \
    && make DESTDIR=/out install \
    && cc /build/check_libxml.c -I/build/libxml2-2.13.9/include \
       -L/build/libxml2-2.13.9/.libs -Wl,-z,relro,-z,now -lxml2 -o /build/check_libxml \
    && python /build/build_postgres_libxml.py receipt

FROM numforge-postgres:apt-20261105
RUN echo 'c05750a6f1c9a90c254df313a9dda9b4c958c0a768b0faf4c15e04b3515c7d93  /usr/lib/x86_64-linux-gnu/libxml2.so.2.9.14' | sha256sum -c -
COPY --from=xml-build /out/usr/lib/x86_64-linux-gnu/libxml2.so.2.13.9 /usr/lib/x86_64-linux-gnu/libxml2.so.2.13.9
COPY --from=xml-build /build/libxml-receipt.json /opt/numforge/system-libxml2.json
COPY --from=xml-build /build/check_libxml /opt/numforge/check_libxml
RUN ln -sfn libxml2.so.2.13.9 /usr/lib/x86_64-linux-gnu/libxml2.so.2 \
    && rm /usr/lib/x86_64-linux-gnu/libxml2.so.2.9.14 && ldconfig \
    && /opt/numforge/check_libxml
