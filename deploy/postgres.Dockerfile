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

FROM golang:1.27.4-alpine AS gosu
ENV CGO_ENABLED=0 GOMAXPROCS=2 GOTOOLCHAIN=local
WORKDIR /build
# Upstream gosu 1.19, immutable tag commit; no launcher source changes.
RUN --mount=type=cache,target=/go/pkg/mod --mount=type=cache,target=/root/.cache/go-build \
    go mod init numforge.local/gosu \
    && go get github.com/tianon/gosu@6456aaa0f3c854d199d0f037f068eb97515b7513 \
        github.com/moby/sys/user@v0.4.1 golang.org/x/sys@v0.44.0 \
    && go build -p 2 -trimpath -o /out/gosu github.com/tianon/gosu \
    && go version -m /out/gosu > /out/gosu.buildinfo

FROM postgres:18.6-bookworm
RUN apt-get -o Acquire::Retries=0 update && apt-get -o Acquire::Retries=0 upgrade -y --no-install-recommends \
    && rm -rf /var/lib/apt/lists/*
COPY --from=gosu /out/gosu /usr/local/bin/gosu
COPY --from=gosu /out/gosu.buildinfo /opt/numforge/gosu.buildinfo
RUN gosu nobody true
RUN echo 'c05750a6f1c9a90c254df313a9dda9b4c958c0a768b0faf4c15e04b3515c7d93  /usr/lib/x86_64-linux-gnu/libxml2.so.2.9.14' | sha256sum -c -
COPY --from=xml-build /out/usr/lib/x86_64-linux-gnu/libxml2.so.2.13.9 /usr/lib/x86_64-linux-gnu/libxml2.so.2.13.9
COPY --from=xml-build /build/libxml-receipt.json /opt/numforge/system-libxml2.json
COPY --from=xml-build /build/check_libxml /opt/numforge/check_libxml
RUN ln -sfn libxml2.so.2.13.9 /usr/lib/x86_64-linux-gnu/libxml2.so.2 \
    && rm /usr/lib/x86_64-linux-gnu/libxml2.so.2.9.14 && ldconfig \
    && /opt/numforge/check_libxml
