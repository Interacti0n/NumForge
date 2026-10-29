FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends cmake gcc make libc6-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /source
COPY . .
RUN cmake -S . -B /build -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release \
    -DNUMFORGE_WARNINGS_AS_ERRORS=ON \
    && cmake --build /build --target numforge_web calculator --parallel 2

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends curl ca-certificates \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /build/numforge_web /build/calculator /usr/local/bin/
USER 10001:10001
EXPOSE 18876
ENTRYPOINT ["numforge_web", "--no-browser", "--bind", "0.0.0.0"]
CMD ["--port", "18876", "--workers", "4", "--queue-capacity", "64"]
