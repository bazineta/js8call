#!/bin/bash
# Script to run JS8Call AppImage in Docker with X11 and audio support
# 
# Usage: ./docker-run.sh [--rebuild] [--debug] [--help]

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[*]${NC} $1"
}

print_error() {
    echo -e "${RED}[!]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

print_info() {
    echo -e "${BLUE}[i]${NC} $1"
}

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    print_error "Docker is not installed. Please install Docker first."
    exit 1
fi

# Parse command line arguments
REBUILD_IMAGE=false
DEBUG_MODE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --rebuild)
            REBUILD_IMAGE=true
            shift
            ;;
        --debug)
            DEBUG_MODE=true
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --rebuild  Rebuild the runtime image"
            echo "  --debug    Run bash instead of JS8Call for debugging"
            echo "  --help     Show this help message"
            echo ""
            echo "Any additional arguments are passed to JS8Call"
            exit 0
            ;;
        *)
            # Pass through to JS8Call
            break
            ;;
    esac
done

# Rebuild image if requested
if [ "$REBUILD_IMAGE" = true ]; then
    print_status "Removing old runtime image..."
    docker rmi js8call-runtime:ubuntu-24.04 2>/dev/null || true
    print_status "Runtime image will be rebuilt"
fi

# Check if AppImage exists
APPIMAGE_PATH=""
if [ -f "output/js8call-x86_64.AppImage" ]; then
    APPIMAGE_PATH="output/js8call-x86_64.AppImage"
elif [ -f "../output/js8call-x86_64.AppImage" ]; then
    APPIMAGE_PATH="../output/js8call-x86_64.AppImage"
else
    # Try to find it
    APPIMAGE_PATH=$(find . -name "js8call*.AppImage" -type f | head -1)
    if [ -z "$APPIMAGE_PATH" ]; then
        print_error "JS8Call AppImage not found!"
        print_info "Please build it first with: ./docker-build.sh"
        exit 1
    fi
fi

print_status "Found AppImage at: $APPIMAGE_PATH"

# Audio setup - we'll run PulseAudio inside the container
if [ -d /dev/snd ]; then
    AUDIO_OPTS="--device /dev/snd --privileged"
    print_status "Audio devices will be available in container"
else
    print_warning "No audio devices found in /dev/snd"
    AUDIO_OPTS=""
fi

# Build runtime image if it doesn't exist
if ! docker images | grep -q "js8call-runtime.*ubuntu-24.04"; then
    print_status "Building runtime image..."
    docker build -f Dockerfile.runtime -t js8call-runtime:ubuntu-24.04 .
fi

# Get the absolute path of the AppImage
APPIMAGE_ABS_PATH=$(realpath "$APPIMAGE_PATH")

# Make sure the AppImage is executable on the host
chmod +x "$APPIMAGE_ABS_PATH" 2>/dev/null || true

# Set up X11 forwarding
XSOCK=/tmp/.X11-unix
XAUTH=/tmp/.docker.xauth.$$

# Allow X connections from local Docker containers
print_info "Setting up X11 forwarding..."
xhost +local:docker 2>/dev/null || xhost +local: 2>/dev/null || true

# Create new Xauthority file
rm -f $XAUTH
# Since xhost +local:docker allows access, we don't strictly need xauth
# But we'll create an empty file to avoid warnings
touch $XAUTH
chmod 644 $XAUTH

print_status "Starting JS8Call with X11 and audio forwarding..."

# Copy AppImage to a temp location for the container
TEMP_APPIMAGE="/tmp/js8call-${RANDOM}.AppImage"
cp "$APPIMAGE_ABS_PATH" "$TEMP_APPIMAGE"

# Choose command based on debug mode
if [ "$DEBUG_MODE" = true ]; then
    print_info "Running in debug mode - starting bash shell"
else
    print_info "Running JS8Call normally"
fi

# Get audio group ID silently
AUDIO_GID=$(getent group audio 2>/dev/null | cut -d: -f3 || echo "29")

# Run the container
if [ "$DEBUG_MODE" = true ]; then
    docker run --rm -it \
        --name js8call-runtime \
        -e DISPLAY=$DISPLAY \
        -e XAUTHORITY=/tmp/.docker.xauth \
        -e QT_X11_NO_MITSHM=1 \
        -v $XSOCK:$XSOCK:rw \
        -v $XAUTH:/tmp/.docker.xauth:rw \
        -v "$TEMP_APPIMAGE:/tmp/js8call.AppImage:ro" \
        -v "$HOME/.config/JS8Call:/home/js8call/.config/JS8Call" \
        $AUDIO_OPTS \
        --device /dev/dri \
        --group-add "$AUDIO_GID" \
        --network host \
        --ipc=host \
        --entrypoint /bin/bash \
        js8call-runtime:ubuntu-24.04
else
    docker run --rm -it \
        --name js8call-runtime \
        -e DISPLAY=$DISPLAY \
        -e XAUTHORITY=/tmp/.docker.xauth \
        -e QT_X11_NO_MITSHM=1 \
        -v $XSOCK:$XSOCK:rw \
        -v $XAUTH:/tmp/.docker.xauth:rw \
        -v "$TEMP_APPIMAGE:/tmp/js8call.AppImage:ro" \
        -v "$HOME/.config/JS8Call:/home/js8call/.config/JS8Call" \
        $AUDIO_OPTS \
        --device /dev/dri \
        --group-add "$AUDIO_GID" \
        --network host \
        --ipc=host \
        js8call-runtime:ubuntu-24.04 "$@"
fi

# Check exit code
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then
    print_error "Container exited with code $EXIT_CODE"
fi

# Cleanup function
cleanup() {
    rm -f "$TEMP_APPIMAGE"
    rm -f $XAUTH
    xhost -local:docker 2>/dev/null || true
}

# Set trap to cleanup on exit
trap cleanup EXIT

# Wait for container to finish
wait
