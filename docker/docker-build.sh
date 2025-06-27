#!/bin/bash
# Docker build script for JS8Call on Ubuntu 22.04

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
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

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    print_error "Docker is not installed. Please install Docker first."
    exit 1
fi

# Check if docker-compose is installed
if ! command -v docker-compose &> /dev/null; then
    print_warning "docker-compose not found, trying 'docker compose'"
    COMPOSE_CMD="docker compose"
else
    COMPOSE_CMD="docker-compose"
fi

# Create output directory
mkdir -p output

print_status "Starting JS8Call Docker build for Ubuntu 24.04..."

# Parse command line arguments
BUILD_TYPE="release"
TARGET="all"

while [[ $# -gt 0 ]]; do
    case $1 in
        --dev)
            BUILD_TYPE="dev"
            shift
            ;;
        --hamlib-only)
            TARGET="hamlib"
            shift
            ;;
        --clean)
            print_status "Cleaning previous builds..."
            rm -rf output/*
            docker rmi js8call-builder:ubuntu-24.04 js8call-dev:ubuntu-24.04 hamlib-builder:ubuntu-24.04 2>/dev/null || true
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --dev          Start development container with shell"
            echo "  --hamlib-only  Build only Hamlib"
            echo "  --clean        Clean previous builds and images"
            echo "  --help         Show this help message"
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Build based on target
case $TARGET in
    hamlib)
        print_status "Building Hamlib only..."
        $COMPOSE_CMD build hamlib-build
        print_status "Hamlib build complete!"
        ;;
    all)
        if [ "$BUILD_TYPE" = "dev" ]; then
            print_status "Starting development container..."
            $COMPOSE_CMD build js8call-dev
            $COMPOSE_CMD run --rm js8call-dev
        else
            print_status "Building JS8Call (this may take a while)..."
            
            # Build using docker-compose with output target
            if ! $COMPOSE_CMD build js8call-build; then
                print_error "Build failed!"
                exit 1
            fi
            
            print_status "Extracting build artifacts..."
            
            # Build the output stage and extract files
            docker build --target appimage-builder -t js8call-appimage:ubuntu-24.04 -f Dockerfile ..
            
            # Find and extract .deb packages
            print_status "Extracting .deb packages..."
            docker run --rm -v "$(pwd)/output":/output js8call-appimage:ubuntu-24.04 \
                sh -c "find /js8call-prefix/build -name '*.deb' -exec cp {} /output/ \;"
            
            # Find and extract AppImage
            print_status "Extracting AppImage..."
            docker run --rm -v "$(pwd)/output":/output js8call-appimage:ubuntu-24.04 \
                sh -c "find /appimage -name '*.AppImage' -exec cp {} /output/ \;"
            
            print_status "Build complete! Output files:"
            ls -la output/
            
            # Check if we got the expected outputs
            if [ -z "$(ls output/*.deb 2>/dev/null)" ] && [ -z "$(ls output/*.AppImage 2>/dev/null)" ]; then
                print_warning "No output files found. Build may have failed."
            else
                print_status "Success! Build artifacts are in the 'output' directory."
            fi
        fi
        ;;
esac