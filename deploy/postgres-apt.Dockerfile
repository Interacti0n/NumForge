ARG BASE_IMAGE=numforge-postgres:maintenance-20261103
FROM ${BASE_IMAGE}
# Preserve the base image's root entrypoint and its gosu privilege drop.
RUN apt-get -o Acquire::Retries=0 update \
    && apt-get -o Acquire::Retries=0 install -y --only-upgrade --no-install-recommends apt=2.6.2 libapt-pkg6.0=2.6.2 \
    && rm -rf /var/lib/apt/lists/*
