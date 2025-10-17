# Multi-stage build for smallest runtime image
FROM mcr.microsoft.com/vs/preview/vcglib:latest AS build-windows  # placeholder for Windows builds (not used)

# POSIX build stage
FROM debian:stable-slim AS builder
RUN apt-get update && apt-get install -y --no-install-recommends     g++ cmake make git ca-certificates  && rm -rf /var/lib/apt/lists/*
WORKDIR /src
# Copy source
COPY . /src
# Build Release
RUN mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build . -j

# Runtime stage
FROM debian:stable-slim
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates  && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=builder /src/build/cpp_proxy /app/cpp_proxy
EXPOSE 8080
# Default runtime args can be overridden
ENTRYPOINT ["/app/cpp_proxy", "--listen", "0.0.0.0", "--port", "8080", "--workers", "8", "--cache-mb", "128"]
