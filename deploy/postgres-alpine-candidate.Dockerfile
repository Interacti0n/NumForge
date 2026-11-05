# Qualification only: libc/locale and data migration gates precede promotion.
ARG GOSU_IMAGE=numforge-postgres:apt-20261105
FROM ${GOSU_IMAGE} AS gosu
FROM postgres:18.6-alpine3.24@sha256:8a334ab9502c8ccb8d7dec4e83b36b5ce8ed7b396e585f1c86b0e69c416f4fca
RUN apk upgrade --no-cache
COPY --from=gosu /usr/local/bin/gosu /usr/local/bin/gosu
COPY --from=gosu /opt/numforge/gosu.buildinfo /opt/numforge/gosu.buildinfo
RUN echo 'b9130179291781a9c2da8a6b5cd206bfbe486a476b00ce88fe3debda2883cf71  /usr/local/bin/gosu' | sha256sum -c - \
    && gosu postgres true
