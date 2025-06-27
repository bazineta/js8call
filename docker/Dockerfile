# Optimized multi-stage Dockerfile for JS8Call
# Uses pre-built base and hamlib images for faster builds

# Build JS8Call using cached base image
FROM js8call-hamlib:latest AS js8call-builder

# Copy source files for building
WORKDIR /js8call-prefix/src

# Copy everything except what's in .dockerignore
# This is simpler and matches the original working Dockerfile
COPY --link . .

# Build JS8Call with ccache
WORKDIR /js8call-prefix/build
RUN --mount=type=cache,target=/ccache \
    cmake -D CMAKE_PREFIX_PATH=/hamlib-prefix \
    -D CMAKE_INSTALL_PREFIX=/js8call-prefix \
    -D CMAKE_BUILD_TYPE=Release \
    -D CMAKE_C_COMPILER_LAUNCHER=ccache \
    -D CMAKE_CXX_COMPILER_LAUNCHER=ccache \
    ../src \
    && make -j$(nproc)

# Create debian package
RUN make package

# Stage for AppImage creation
FROM js8call-builder AS appimage-builder

# Create AppDir structure
WORKDIR /appimage
RUN mkdir -p AppDir

# Install JS8Call to AppDir
WORKDIR /js8call-prefix/build
RUN make DESTDIR=/appimage/AppDir install

# Copy desktop file and icon if needed
RUN test -f /appimage/AppDir/usr/share/applications/js8call.desktop || \
    cp /js8call-prefix/src/js8call.desktop /appimage/AppDir/usr/share/applications/ && \
    if [ ! -f /appimage/AppDir/usr/share/icons/hicolor/128x128/apps/js8call_icon.png ]; then \
        mkdir -p /appimage/AppDir/usr/share/icons/hicolor/128x128/apps && \
        convert /js8call-prefix/src/artwork/js8call_icon.png -resize 128x128 \
            /appimage/AppDir/usr/share/icons/hicolor/128x128/apps/js8call_icon.png; \
    fi

# Create AppImage
WORKDIR /appimage
RUN EXEC_PATH=$(find AppDir -type f -name "js8call" -executable | head -1) && \
    echo "Found executable at: $EXEC_PATH" && \
    linuxdeploy --appdir AppDir \
    --executable "$EXEC_PATH" \
    --plugin qt \
    --output appimage \
    --desktop-file AppDir/usr/share/applications/js8call.desktop \
    --icon-file AppDir/usr/share/icons/hicolor/128x128/apps/js8call_icon.png

# Final output stage - using minimal image with shell for extraction
FROM busybox:latest AS output
COPY --from=js8call-builder /js8call-prefix/build/*.deb /
COPY --from=appimage-builder /appimage/*.AppImage /

# Development stage with source mounted
FROM js8call-hamlib:latest AS development

# Install additional development tools
RUN apt-get update && apt-get install -y \
    gdb \
    valgrind \
    clang-format \
    vim \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /js8call-prefix
CMD ["/bin/bash"]