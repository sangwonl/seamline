#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT_DIR/build/ios"

# Build for device (arm64)
cmake -S "$ROOT_DIR" -B "$BUILD_DIR/arm64" \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
    -DSEAMLINE_BUILD_TESTS=OFF \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR/arm64" --config Release

# Build for simulator (arm64)
cmake -S "$ROOT_DIR" -B "$BUILD_DIR/sim-arm64" \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_SYSROOT=iphonesimulator \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
    -DSEAMLINE_BUILD_TESTS=OFF \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR/sim-arm64" --config Release

# Create xcframework
rm -rf "$BUILD_DIR/Seamline.xcframework"
xcodebuild -create-xcframework \
    -library "$BUILD_DIR/arm64/libseamline.a" \
    -headers "$ROOT_DIR/include" \
    -library "$BUILD_DIR/sim-arm64/libseamline.a" \
    -headers "$ROOT_DIR/include" \
    -output "$BUILD_DIR/Seamline.xcframework"

echo "Built: $BUILD_DIR/Seamline.xcframework"
