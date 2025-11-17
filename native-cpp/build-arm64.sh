#!/bin/bash

# Sample Build script for ARM64 cross-compilation on x64 Ubuntu
# This builds only the krisp-wav-cli application (which doesn't require libsndfile)

set -e

echo "Building Krisp SDK Sample Apps for ARM64..."

# Clean up previous builds
rm -rf build-arm64 krisp-sdk

# Create build directory
mkdir -p build-arm64
cd build-arm64

# Configure with ARM64 toolchain, ensure DCOPY_KRISP_SDK is set to the correct path
cmake ../cmake \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm64.cmake \
    -DBUILD_ARM64=ON \
    -DBUILD_WAV_CLI=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DCOPY_KRISP_SDK=$HOME/dev/krisp-sdk/krisp-audio-sdk-9.9.3-lin_arm64 

# Build
make -j$(nproc)

echo "ARM64 build completed!"
echo "Binary location: ../bin/krisp-wav-cli"