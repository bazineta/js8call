# Docker Build Instructions for JS8Call

This Docker setup provides an optimized containerized build environment for JS8Call on Ubuntu 24.04 with advanced caching support.

## Prerequisites

- Docker installed on your system
- docker-compose (or Docker Compose plugin)
- At least 4GB of free disk space (10GB recommended for full caching)

## Quick Start

1. Build JS8Call and create packages:
   ```bash
   ./docker-build.sh
   ```

2. Find the built packages in the `output/` directory:
   - `js8call_*.deb` - Debian package for Ubuntu/Debian systems
   - `JS8Call-*.AppImage` - Portable AppImage that runs on most Linux distributions

## Build Options

### Standard Build
```bash
./docker-build.sh
```
Builds both .deb package and AppImage with caching enabled.

### Quick Rebuild (for code changes)
```bash
./docker-build.sh --rebuild
```
Uses cached base images for faster rebuilds when only source code has changed.

### Development Mode
```bash
./docker-build.sh --dev
```
Starts an interactive shell in the build environment for development and debugging.

### Build Base Image
```bash
./docker-build.sh --base
```
Rebuilds the base image with all dependencies (Qt6, build tools, etc.).

### Build Hamlib Only
```bash
./docker-build.sh --hamlib-only
```
Builds only the Hamlib library (useful for testing).

### Clean Build
```bash
./docker-build.sh --clean
```
Removes all builds, Docker images, and caches.

### Build Without Cache
```bash
./docker-build.sh --no-cache
```
Forces a complete rebuild without using any Docker cache.

## Using Docker Compose Directly

### Build all stages:
```bash
docker-compose build js8call-build
```

### Extract build artifacts:
```bash
docker-compose run --rm js8call-build
```

### Development shell:
```bash
docker-compose run --rm js8call-dev
```

## Manual Docker Commands

### Build the complete image:
```bash
docker build -t js8call-builder:ubuntu-24.04 .
```

### Extract packages after build:
```bash
# Create container
docker create --name js8call-temp js8call-builder:ubuntu-24.04

# Copy files
docker cp js8call-temp:/js8call_*.deb ./output/
docker cp js8call-temp:/*.AppImage ./output/

# Cleanup
docker rm js8call-temp
```

## Troubleshooting

1. **Build fails with "No space left on device"**
   - Clean up Docker: `docker system prune -a`
   - Ensure you have at least 4GB free space

2. **Permission denied on build script**
   - Run: `chmod +x docker-build.sh`

3. **AppImage creation fails**
   - This is often due to missing dependencies. Check the Docker build logs.

4. **Can't run AppImage**
   - Make it executable: `chmod +x JS8Call-*.AppImage`
   - Install FUSE: `sudo apt install libfuse2`

## Build Artifacts

After a successful build, you'll find:

- **Debian Package** (`js8call_*.deb`): 
  - Install with: `sudo dpkg -i js8call_*.deb`
  - Fix dependencies: `sudo apt-get install -f`

- **AppImage** (`JS8Call-*.AppImage`):
  - Make executable: `chmod +x JS8Call-*.AppImage`
  - Run directly: `./JS8Call-*.AppImage`

## Caching and Performance

The optimized build system uses multiple layers of caching:

1. **Base Image Caching**: All build dependencies are cached in `js8call-base:ubuntu-24.04`
2. **Hamlib Caching**: Hamlib is built separately and cached in `js8call-hamlib:latest`
3. **ccache**: C++ compilation is cached using ccache (stored in Docker volume)
4. **Layer Caching**: Docker BuildKit optimizes layer caching for source files

### Build Time Comparison

- **First build**: 15-20 minutes (builds everything)
- **Subsequent builds (code changes only)**: 2-5 minutes with `--rebuild`
- **Full rebuild without cache**: 15-20 minutes with `--no-cache`
