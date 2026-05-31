#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT_DIR/build/android"

if [ -z "$ANDROID_NDK_HOME" ]; then
    echo "Error: ANDROID_NDK_HOME not set"
    exit 1
fi

ABIS="arm64-v8a armeabi-v7a x86_64"

for ABI in $ABIS; do
    echo "Building for $ABI..."
    cmake -S "$ROOT_DIR" -B "$BUILD_DIR/$ABI" \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM=android-24 \
        -DSEAMLINE_BUILD_TESTS=OFF \
        -DSEAMLINE_BUILD_SHARED=ON \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR/$ABI" --config Release
done

echo "Built libraries:"
find "$BUILD_DIR" -name "libseamline.*" -type f
