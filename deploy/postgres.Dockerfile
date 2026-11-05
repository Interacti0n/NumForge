FROM golang:1.27.3-alpine AS gosu
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
