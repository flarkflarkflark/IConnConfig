#!/bin/bash
set -e

echo "Building Windows version using Docker..."

# Create output directory
mkdir -p windows-build

# Build Docker image and run
docker build -f Dockerfile.windows -t iconnconfig-windows .
docker run --rm -v "$(pwd)/windows-build:/output" iconnconfig-windows

echo "Windows build complete! Check windows-build/ directory"