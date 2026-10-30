FROM golang:1.27.3-alpine AS build
ENV CGO_ENABLED=0 GOMAXPROCS=2 GOTOOLCHAIN=local
WORKDIR /build
# Keep Caddy's release; update the vulnerable library/toolchain versions explicitly.
RUN --mount=type=cache,target=/go/pkg/mod --mount=type=cache,target=/root/.cache/go-build \
    go mod init numforge.local/gateway \
    && go get github.com/caddyserver/caddy/v2/cmd/caddy@v2.11.7 \
        github.com/go-jose/go-jose/v4@v4.1.6 golang.org/x/net@v0.60.0 \
    && go build -p 2 -trimpath -o /out/caddy github.com/caddyserver/caddy/v2/cmd/caddy

FROM caddy:2.11.7-alpine
RUN apk upgrade --no-cache
COPY --from=build /out/caddy /usr/bin/caddy
