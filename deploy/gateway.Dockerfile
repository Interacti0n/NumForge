FROM golang:1.27.4-alpine AS build
ENV CGO_ENABLED=0 GOMAXPROCS=2 GOTOOLCHAIN=local
WORKDIR /build
# Keep Caddy's release; update the vulnerable library/toolchain versions explicitly.
RUN --mount=type=cache,target=/go/pkg/mod --mount=type=cache,target=/root/.cache/go-build \
    go mod init numforge.local/gateway \
    && go get github.com/caddyserver/caddy/v2/cmd/caddy@v2.11.7 \
        github.com/go-jose/go-jose/v4@v4.1.6 golang.org/x/net@v0.62.0 \
        github.com/go-chi/chi/v5@v5.4.0 \
        golang.org/x/crypto@v0.58.1-0.20261009181718-2f1f834330b2 \
    && go build -p 2 -trimpath -o /out/caddy github.com/caddyserver/caddy/v2/cmd/caddy \
    && go version -m /out/caddy > /out/caddy.buildinfo

FROM caddy:2.11.7-alpine
RUN apk upgrade --no-cache && apk info -v zlib
COPY --from=build /out/caddy /usr/bin/caddy
COPY --from=build /out/caddy.buildinfo /opt/numforge/caddy.buildinfo
