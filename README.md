# seamline

A cross-platform C library for stitching sequential images into a single long image. Designed for scroll capture scenarios — where a series of overlapping screenshots are combined into one continuous result.

## How It Works

seamline detects overlapping regions between consecutive images using a block-matching algorithm that handles periodic content (terminals, spreadsheets, code editors) reliably. It then finds the optimal seam line within each overlap to produce artifact-free joins.

**Algorithm overview:**

1. Identify "distinctive" rows in each image (high variance or strong transitions)
2. Evaluate all candidate overlaps using contiguous block matching
3. Score candidates by matched block ratio, row ratio, and error
4. Choose the best seam cut within the overlap to minimize boundary artifacts
5. Concatenate images with overlap removed

Supports both vertical (scroll down) and horizontal (scroll right) stitching.

## Quick Start

### Build

```bash
git clone https://github.com/pineple/seamline.git
cd seamline
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

This produces `libseamline.a` (static library).

### Usage (C)

```c
#include "seamline.h"

// From file paths
const char* paths[] = {"frame_001.png", "frame_002.png", "frame_003.png"};
SeamlineOptions opts = seamline_default_options();
opts.auto_crop = 1;  // handle slight width differences between frames
SeamlineResult result = seamline_stitch_files(paths, 3, opts);

if (result.ok) {
    seamline_save_png(&result, "output.png");
} else {
    fprintf(stderr, "Error: %s\n", result.error);
}
seamline_free(&result);
```

### Usage (memory buffers)

```c
#include "seamline.h"

// Assume you have RGBA buffers from your platform's image API
SeamlineImage images[2] = {
    { .data = rgba_buffer_1, .width = 1080, .height = 1920 },
    { .data = rgba_buffer_2, .width = 1080, .height = 1920 },
};

SeamlineOptions opts = seamline_default_options();
SeamlineResult result = seamline_stitch(images, 2, opts);

if (result.ok) {
    // result.data contains RGBA pixels (result.width * result.height * 4 bytes)
    // Use result.data directly or save to file
    seamline_save_png(&result, "stitched.png");
}
seamline_free(&result);
```

## API Reference

### Types

```c
typedef struct {
    const uint8_t* data;   // RGBA pixel buffer (width * height * 4 bytes)
    int width;             // Image width in pixels
    int height;            // Image height in pixels
} SeamlineImage;

typedef struct {
    double match_threshold;  // Max avg pixel diff for row matching (0-255, default: 5.0)
    int horizontal;          // 0 = vertical stitch (default), 1 = horizontal
    int auto_crop;           // 0 = error on width mismatch (default), 1 = crop to min width
} SeamlineOptions;

typedef struct {
    uint8_t* data;           // RGBA result buffer (owned by seamline, free with seamline_free)
    int width;               // Result width
    int height;              // Result height
    int ok;                  // 1 = success, 0 = error
    char error[256];         // Error message (when ok == 0)
} SeamlineResult;
```

### Functions

| Function | Description |
|----------|-------------|
| `seamline_default_options()` | Returns default options (`threshold=5.0`, `horizontal=0`) |
| `seamline_stitch(images, count, opts)` | Stitch from RGBA memory buffers |
| `seamline_stitch_files(paths, count, opts)` | Stitch from PNG/JPG file paths |
| `seamline_save_png(result, path)` | Save result to PNG. Returns 1 on success. |
| `seamline_save_jpg(result, path, quality)` | Save result to JPG. Quality: 1-100. |
| `seamline_free(result)` | Free result buffer. Must be called after use. |

### Options

| Field | Default | Description |
|-------|---------|-------------|
| `match_threshold` | `5.0` | Pixel difference tolerance for matching rows. Increase for noisy captures (e.g., 10-15). Decrease for pixel-perfect sources (e.g., 1-2). |
| `horizontal` | `0` | Set to `1` for horizontal scroll captures (left-to-right stitching). |
| `auto_crop` | `0` | Set to `1` to automatically crop images to the minimum width when dimensions differ slightly (e.g., scrollbar appearing/disappearing during capture). Without this, width mismatch returns an error. |

## Platform Integration

### iOS (Swift)

```swift
// Bridging header: #include "seamline.h"

func stitchImages(_ images: [UIImage]) -> UIImage? {
    let seamlineImages: [SeamlineImage] = images.map { img in
        let cgImage = img.cgImage!
        let data = cgImage.dataProvider!.data! as Data
        return SeamlineImage(data: (data as NSData).bytes.assumingMemoryBound(to: UInt8.self),
                            width: Int32(cgImage.width),
                            height: Int32(cgImage.height))
    }

    var opts = seamline_default_options()
    let result = seamlineImages.withUnsafeBufferPointer { buf in
        seamline_stitch(buf.baseAddress, Int32(buf.count), opts)
    }
    defer { var r = result; seamline_free(&r) }

    guard result.ok == 1 else { return nil }

    let colorSpace = CGColorSpaceCreateDeviceRGB()
    let ctx = CGContext(data: UnsafeMutableRawPointer(result.data),
                       width: Int(result.width), height: Int(result.height),
                       bitsPerComponent: 8, bytesPerRow: Int(result.width) * 4,
                       space: colorSpace,
                       bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)
    guard let cgImage = ctx?.makeImage() else { return nil }
    return UIImage(cgImage: cgImage)
}
```

### Android (Kotlin + JNI)

```kotlin
// Native method declaration
external fun nativeStitch(imagePaths: Array<String>): String?  // returns output path

// JNI implementation (C)
JNIEXPORT jstring JNICALL
Java_com_example_Seamline_nativeStitch(JNIEnv* env, jobject obj, jobjectArray paths) {
    int count = (*env)->GetArrayLength(env, paths);
    const char** c_paths = malloc(count * sizeof(char*));
    for (int i = 0; i < count; i++) {
        jstring s = (*env)->GetObjectArrayElement(env, paths, i);
        c_paths[i] = (*env)->GetStringUTFChars(env, s, NULL);
    }

    SeamlineOptions opts = seamline_default_options();
    SeamlineResult result = seamline_stitch_files(c_paths, count, opts);

    // cleanup and return...
}
```

### React Native (Native Module)

From the native side of your module, call `seamline_stitch_files()` with paths received from JS, save the result, and return the output path to JavaScript.

```javascript
// JS side
import { NativeModules } from 'react-native';
const { SeamlineModule } = NativeModules;

const outputPath = await SeamlineModule.stitch([
  '/tmp/frame_001.png',
  '/tmp/frame_002.png',
  '/tmp/frame_003.png',
]);
```

## Build Options

### CMake Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `SEAMLINE_BUILD_TESTS` | `ON` | Build test suite |
| `SEAMLINE_BUILD_SHARED` | `OFF` | Build shared library instead of static |
| `SEAMLINE_BUILD_CLI` | `OFF` | Build CLI tool for testing |

### CLI Tool

```bash
cmake -B build -DSEAMLINE_BUILD_CLI=ON
cmake --build build --target stitch_cli

# Vertical stitch
./build/stitch_cli output.png frame1.png frame2.png frame3.png

# With auto-crop (handles slight width differences)
./build/stitch_cli output.png frame1.png frame2.png --auto-crop

# Horizontal stitch
./build/stitch_cli output.png left.png right.png --horizontal --auto-crop
```

### iOS (xcframework)

```bash
./scripts/build-ios.sh
# Output: build/Seamline.xcframework (arm64 + arm64-simulator)
```

### Android (NDK)

```bash
./scripts/build-android.sh
# Output: build/android/{arm64-v8a,armeabi-v7a,x86_64}/libseamline.so
```

## Requirements

- **All images must have the same width** (vertical) or same height (horizontal)
- Input format: RGBA, 8 bits per channel, tightly packed (stride = width * 4)
- Minimum overlap: 10 rows (configurable via search parameters)
- Supported file formats (file API): PNG, JPG, BMP, TGA

## Performance

The algorithm is O(n * search_height * width) per image pair, where search_height defaults to 90% of image height. For typical scroll captures (1080px wide, 50-200px overlap), stitching is sub-millisecond per pair on modern mobile hardware.

Memory usage: 2x the largest intermediate result (current + next frame in RGBA).

## Limitations

- No support for non-overlapping frames (they are concatenated without alignment)
- Assumes linear scroll (no rotation, scale, or perspective changes between frames)
- Very low-contrast or uniform images may produce false overlap matches
- Alpha channel is preserved but not used in matching

## License

MIT

## Contributing

Issues and pull requests welcome. The core algorithm lives in `src/stitch.cpp` — see inline comments for the matching strategy.
