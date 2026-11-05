ARG BASE_IMAGE=numforge-accounts:sqlite-backport-20261104
FROM ${BASE_IMAGE}
ARG RUNTIME_USER=10001:10001
USER root
# Disable the vulnerable retry path while bootstrapping the fixed APT itself.
RUN apt-get -o Acquire::Retries=0 update \
    && apt-get -o Acquire::Retries=0 install -y --only-upgrade --no-install-recommends apt=2.6.2 libapt-pkg6.0=2.6.2 \
    && rm -rf /var/lib/apt/lists/*
USER ${RUNTIME_USER}
