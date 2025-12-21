# SiteSurveyor Docker Build
# Multi-stage build for Linux

# Stage 1: Build environment
FROM ubuntu:22.04 AS builder

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    qt6-base-dev \
    libqt6concurrent6 \
    libqt6network6 \
    libqt6printsupport6 \
    libgdal-dev \
    libgeos-dev \
    libproj-dev \
    libgl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /src

# Copy source code
COPY . .

# Initialize submodules
RUN git submodule update --init --recursive || true

# Configure and build
RUN cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DWITH_GDAL=ON \
    -DWITH_GEOS=ON

RUN cmake --build build --parallel $(nproc)

# Stage 2: Runtime image
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies only
RUN apt-get update && apt-get install -y --no-install-recommends \
    libqt6core6 \
    libqt6gui6 \
    libqt6widgets6 \
    libqt6network6 \
    libqt6printsupport6 \
    libqt6concurrent6 \
    libgdal30 \
    libgeos-c1v5 \
    libproj22 \
    libgl1 \
    libxcb-cursor0 \
    libxkbcommon0 \
    libxcb-xinerama0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-render-util0 \
    libxcb-shape0 \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -m -s /bin/bash surveyor
USER surveyor
WORKDIR /home/surveyor

# Copy built application from builder stage
COPY --from=builder /src/build/bin/SiteSurveyor /usr/local/bin/

# Copy resources if needed
COPY --from=builder /src/resources /usr/local/share/sitesurveyor/resources

# Set display environment variable (for X11 forwarding)
ENV DISPLAY=:0
ENV QT_QPA_PLATFORM=xcb

# Entry point
ENTRYPOINT ["/usr/local/bin/SiteSurveyor"]

# Labels
LABEL org.opencontainers.image.title="SiteSurveyor"
LABEL org.opencontainers.image.description="Professional Geomatics Software"
LABEL org.opencontainers.image.version="1.0.8"
LABEL org.opencontainers.image.vendor="Eineva Incorporated"
LABEL org.opencontainers.image.url="https://sitesurveyor.dev"
LABEL org.opencontainers.image.source="https://github.com/ConsoleMangena/sitesurveyor"
