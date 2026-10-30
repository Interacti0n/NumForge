FROM golang:1.27.3-alpine AS gosu
ENV CGO_ENABLED=0 GOMAXPROCS=2 GOTOOLCHAIN=local
# Upstream gosu 1.19, immutable tag commit; no launcher source changes.
RUN --mount=type=cache,target=/go/pkg/mod --mount=type=cache,target=/root/.cache/go-build \
    go install -p 2 -trimpath github.com/tianon/gosu@6456aaa0f3c854d199d0f037f068eb97515b7513

FROM postgres:18.6-bookworm
COPY --from=gosu /go/bin/gosu /usr/local/bin/gosu
RUN gosu nobody true
