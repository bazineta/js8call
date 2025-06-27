# Multi-stage Dockerfile for building JS8Call on Ubuntu 24.04

# Stage 1: Build Hamlib
FROM ubuntu:24.04 AS hamlib-builder

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install Hamlib build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    git \
    automake \
    autoconf \
    libtool \
    texinfo \
    pkg-config \
    libusb-1.0-0-dev \
    && rm -rf /var/lib/apt/lists/*

# Clone and build Hamlib
WORKDIR /hamlib-prefix
RUN git clone https://github.com/Hamlib/Hamlib.git src

WORKDIR /hamlib-prefix/src
RUN ./bootstrap

WORKDIR /hamlib-prefix/build
RUN ../src/configure --prefix=/hamlib-prefix \
    --disable-shared --enable-static \
    --without-cxx-binding --disable-winradio \
    CFLAGS="-g -O2 -fdata-sections -ffunction-sections" \
    LDFLAGS="-Wl,--gc-sections" \
    && make -j$(nproc) \
    && make install-strip

# Stage 2: Build JS8Call
FROM ubuntu:24.04 AS js8call-builder

ENV DEBIAN_FRONTEND=noninteractive

# Install JS8Call build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    qt6-base-dev \
    qt6-multimedia-dev \
    libqt6serialport6-dev \
    libqt6svg6-dev \
    libgl1-mesa-dev \
    libfftw3-dev \
    libfftw3-single3 \
    libudev-dev \
    libusb-1.0-0-dev \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy Hamlib from previous stage
COPY --from=hamlib-builder /hamlib-prefix /hamlib-prefix

# Copy JS8Call source
WORKDIR /js8call-prefix
COPY . /js8call-prefix/src/

# Build JS8Call
WORKDIR /js8call-prefix/build
RUN cmake -D CMAKE_PREFIX_PATH=/hamlib-prefix \
    -D CMAKE_INSTALL_PREFIX=/js8call-prefix \
    -D CMAKE_BUILD_TYPE=Release \
    ../src \
    && make -j$(nproc)

# Create debian package
RUN make package

# Stage 3: Create AppImage
FROM ubuntu:24.04 AS appimage-builder

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies and AppImage tools
RUN apt-get update && apt-get install -y \
    wget \
    file \
    libfuse2 \
    build-essential \
    cmake \
    qt6-base-dev \
    qt6-multimedia-dev \
    libqt6serialport6-dev \
    libqt6svg6-dev \
    libgl1-mesa-dev \
    libfftw3-dev \
    libfftw3-single3 \
    libboost-all-dev \
    libusb-1.0-0-dev \
    libudev-dev \
    imagemagick \
    && rm -rf /var/lib/apt/lists/*

# Download linuxdeploy and Qt plugin
WORKDIR /tools
RUN wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
    && wget -q https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage \
    && chmod +x linuxdeploy*.AppImage \
    && ./linuxdeploy-x86_64.AppImage --appimage-extract \
    && mv squashfs-root linuxdeploy \
    && ./linuxdeploy-plugin-qt-x86_64.AppImage --appimage-extract \
    && mv squashfs-root linuxdeploy-plugin-qt

# Copy built application from previous stage
COPY --from=js8call-builder /js8call-prefix /js8call-prefix
COPY --from=hamlib-builder /hamlib-prefix /hamlib-prefix

# Create AppDir structure
WORKDIR /appimage
RUN mkdir -p AppDir/usr/bin AppDir/usr/share/applications AppDir/usr/share/icons/hicolor/128x128/apps

# Install JS8Call to AppDir
WORKDIR /js8call-prefix/build
RUN make DESTDIR=/appimage/AppDir install

# Debug: List installed files
RUN find /appimage/AppDir -type f -name "js8call*" -o -name "JS8Call*" | head -20

# Copy desktop file and icon if not already installed
RUN test -f /appimage/AppDir/usr/share/applications/js8call.desktop || \
    cp /js8call-prefix/src/js8call.desktop /appimage/AppDir/usr/share/applications/ && \
    if [ ! -f /appimage/AppDir/usr/share/icons/hicolor/128x128/apps/js8call_icon.png ]; then \
        convert /js8call-prefix/src/artwork/js8call_icon.png -resize 128x128 \
            /appimage/AppDir/usr/share/icons/hicolor/128x128/apps/js8call_icon.png; \
    fi

# Create AppImage
WORKDIR /appimage
ENV PATH="/tools/linuxdeploy/usr/bin:/tools/linuxdeploy-plugin-qt/usr/bin:${PATH}"
ENV QMAKE=/usr/lib/qt6/bin/qmake
ENV QT_SELECT=qt6

# Find the actual executable path
RUN EXEC_PATH=$(find AppDir -type f -name "js8call" -executable | head -1) && \
    echo "Found executable at: $EXEC_PATH" && \
    linuxdeploy --appdir AppDir \
    --executable "$EXEC_PATH" \
    --plugin qt \
    --output appimage \
    --desktop-file AppDir/usr/share/applications/js8call.desktop \
    --icon-file AppDir/usr/share/icons/hicolor/128x128/apps/js8call_icon.png

# Final stage: Output collector
FROM scratch AS output
COPY --from=js8call-builder /js8call-prefix/build/*.deb /
COPY --from=appimage-builder /appimage/*.AppImage /