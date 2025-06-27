# Docker Build Instructions for JS8Call

This Docker setup provides a containerized build environment for JS8Call on Ubuntu 24.04.

## Prerequisites

- Docker installed on your system
- docker-compose (or Docker Compose plugin)
- At least 4GB of free disk space

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
Builds both .deb package and AppImage.

### Development Mode
```bash
./docker-build.sh --dev
```
Starts an interactive shell in the build environment for development and debugging.

### Build Hamlib Only
```bash
./docker-build.sh --hamlib-only
```
Builds only the Hamlib library (useful for testing).

### Clean Build
```bash
./docker-build.sh --clean
```
Removes previous builds and Docker images.

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

## Notes

- The build process compiles Hamlib from source for better compatibility
- Qt6 is used (matching the recent codebase updates)
- The AppImage should work on most Linux distributions
- Build time is typically 10-20 minutes depending on your system