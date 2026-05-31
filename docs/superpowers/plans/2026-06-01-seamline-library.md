# seamline Library Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract the image stitching algorithm from snapr-cli into a standalone, cross-platform C library with pure C API.

**Architecture:** Internal C++ implementation (Image class + stitch algorithm) wrapped by a C API (`seamline.h`). Image I/O uses stb_image/stb_image_write for portability. Build system is CMake producing static/shared libraries for all platforms.

**Tech Stack:** C++17 (internal), C99 (public API), CMake 3.16+, stb_image, stb_image_write, Catch2 v3 (tests)

---

## File Structure

```
seamline/
├── include/
│   └── seamline.h              # Public C API header
├── src/
│   ├── image.h                 # Internal Image class
│   ├── image.cpp               # Image implementation + save (stb)
│   ├── stitch.h                # Internal stitch C++ API
│   ├── stitch.cpp              # Core algorithm (find_overlap, stitch_two, etc.)
│   ├── seamline.cpp            # C API wrapper
│   └── stb_impl.cpp            # stb_image + stb_image_write implementation
├── deps/
│   └── stb/                    # stb headers (fetched or vendored)
├── tests/
│   ├── CMakeLists.txt
│   └── test_stitch.cpp         # Catch2 unit tests
├── CMakeLists.txt              # Main build
├── cmake/
│   └── Dependencies.cmake      # FetchContent for deps
├── scripts/
│   ├── build-ios.sh
│   └── build-android.sh
├── README.md                   # Already written
└── LICENSE
```

---

## Task 1: Project Skeleton + CMake

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/Dependencies.cmake`
- Create: `LICENSE`

- [ ] **Step 1: Create root CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.16)
project(seamline VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SEAMLINE_BUILD_TESTS "Build test suite" ON)
option(SEAMLINE_BUILD_SHARED "Build shared library" OFF)
option(SEAMLINE_BUILD_CLI "Build CLI tool" OFF)

include(cmake/Dependencies.cmake)

set(SEAMLINE_SOURCES
    src/image.cpp
    src/stitch.cpp
    src/seamline.cpp
    src/stb_impl.cpp
)

if(SEAMLINE_BUILD_SHARED)
    add_library(seamline SHARED ${SEAMLINE_SOURCES})
else()
    add_library(seamline STATIC ${SEAMLINE_SOURCES})
endif()

target_include_directories(seamline
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/stb
)

if(SEAMLINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

- [ ] **Step 2: Create cmake/Dependencies.cmake**

```cmake
include(FetchContent)

# stb - header-only image loading/writing
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)

# Copy stb headers to deps/stb for stable include path
file(GLOB STB_HEADERS "${stb_SOURCE_DIR}/stb_image.h" "${stb_SOURCE_DIR}/stb_image_write.h")
file(COPY ${STB_HEADERS} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/deps/stb)

# Catch2 (tests only)
if(SEAMLINE_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.8.1
    )
    FetchContent_MakeAvailable(Catch2)
endif()
```

- [ ] **Step 3: Create LICENSE (MIT)**

```
MIT License

Copyright (c) 2026 pineple

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

- [ ] **Step 4: Verify CMake configures**

Run: `mkdir -p build && cd build && cmake .. -DSEAMLINE_BUILD_TESTS=OFF`
Expected: Configuration succeeds (sources don't exist yet, but cmake should parse)

- [ ] **Step 5: Commit**

```
feat: initial project skeleton with CMake build system
```

---

## Task 2: Internal Image Class

**Files:**
- Create: `src/image.h`
- Create: `src/image.cpp`
- Create: `src/stb_impl.cpp`

- [ ] **Step 1: Create src/image.h**

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace seamline {

class Image {
public:
    Image(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    int stride() const { return width_ * 4; }
    uint8_t* data() { return pixels_.data(); }
    const uint8_t* data() const { return pixels_.data(); }
    size_t size() const { return pixels_.size(); }

    bool save_png(const std::string& path) const;
    bool save_jpg(const std::string& path, int quality = 90) const;

    static std::unique_ptr<Image> load(const std::string& path);

private:
    int width_;
    int height_;
    std::vector<uint8_t> pixels_; // RGBA
};

} // namespace seamline
```

- [ ] **Step 2: Create src/image.cpp**

```cpp
#include "image.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include <cstring>

namespace seamline {

Image::Image(int width, int height)
    : width_(width), height_(height), pixels_(width * height * 4, 0) {}

bool Image::save_png(const std::string& path) const {
    return stbi_write_png(path.c_str(), width_, height_, 4, data(), stride()) != 0;
}

bool Image::save_jpg(const std::string& path, int quality) const {
    return stbi_write_jpg(path.c_str(), width_, height_, 4, data(), quality) != 0;
}

std::unique_ptr<Image> Image::load(const std::string& path) {
    int w, h, channels;
    uint8_t* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data) return nullptr;

    auto img = std::make_unique<Image>(w, h);
    std::memcpy(img->data(), data, w * h * 4);
    stbi_image_free(data);
    return img;
}

} // namespace seamline
```

- [ ] **Step 3: Create src/stb_impl.cpp**

```cpp
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
```

- [ ] **Step 4: Commit**

```
feat: add internal Image class with stb-based I/O
```

---

## Task 3: Core Stitch Algorithm

**Files:**
- Create: `src/stitch.h`
- Create: `src/stitch.cpp`

- [ ] **Step 1: Create src/stitch.h**

```cpp
#pragma once

#include "image.h"
#include <memory>
#include <string>
#include <vector>

namespace seamline {

struct OverlapResult {
    int offset;
    double score;
    bool found;
};

OverlapResult find_overlap(const Image& img_a, const Image& img_b,
                           int search_height = 0, int min_overlap = 10,
                           double match_threshold = 5.0);

std::unique_ptr<Image> stitch_two(const Image& img_a, const Image& img_b,
                                  int overlap_rows);

struct StitchResult {
    std::unique_ptr<Image> image;
    std::string error;
    bool ok() const { return image != nullptr; }
};

StitchResult stitch_images(const std::vector<std::string>& frame_paths,
                           double match_threshold = 5.0, bool horizontal = false);

StitchResult stitch_buffers(const std::vector<const uint8_t*>& buffers,
                            const std::vector<int>& widths,
                            const std::vector<int>& heights,
                            double match_threshold = 5.0, bool horizontal = false);

} // namespace seamline
```

- [ ] **Step 2: Create src/stitch.cpp**

Port from `snapr-cli/src/stitch/stitch.cpp` with these changes:
- Namespace: `capture` → `seamline`
- Remove `#ifdef __APPLE__` PNG loading branch (use `Image::load()` uniformly)
- Add `stitch_buffers()` that takes raw RGBA data instead of file paths

```cpp
#include "stitch.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <limits>
#include <vector>

namespace seamline {

static double avg_row_diff(const Image& img_a, int row_a,
                           const Image& img_b, int row_b) {
    int w = img_a.width();
    int sample_step = std::max(1, w / 24);
    const uint8_t* ra = img_a.data() + row_a * img_a.stride();
    const uint8_t* rb = img_b.data() + row_b * img_b.stride();

    double diff = 0.0;
    int cnt = 0;
    for (int x = 0; x < w; x += sample_step) {
        int idx = x * 4;
        diff += std::abs(static_cast<int>(ra[idx]) - static_cast<int>(rb[idx]));
        diff += std::abs(static_cast<int>(ra[idx + 1]) - static_cast<int>(rb[idx + 1]));
        diff += std::abs(static_cast<int>(ra[idx + 2]) - static_cast<int>(rb[idx + 2]));
        cnt += 3;
    }
    return cnt > 0 ? diff / cnt : 0.0;
}

static int choose_seam_cut(const Image& img_a, const Image& img_b, int overlap_rows) {
    if (overlap_rows <= 1) return overlap_rows;

    int best_cut = overlap_rows;
    double best_score = std::numeric_limits<double>::infinity();

    for (int cut = 1; cut <= overlap_rows; cut++) {
        int a_last = img_a.height() - overlap_rows + cut - 1;
        int b_first = cut;
        if (a_last < 0 || a_last >= img_a.height() || b_first >= img_b.height()) continue;

        double score = avg_row_diff(img_a, a_last, img_b, b_first);
        if (cut > 1 && b_first + 1 < img_b.height()) {
            score += avg_row_diff(img_a, a_last - 1, img_b, b_first - 1) * 0.5;
            score += avg_row_diff(img_a, a_last, img_b, b_first + 1) * 0.5;
        }

        if (score < best_score) {
            best_score = score;
            best_cut = cut;
        }
    }
    return best_cut;
}

OverlapResult find_overlap(const Image& img_a, const Image& img_b,
                           int search_height, int min_overlap,
                           double match_threshold) {
    OverlapResult result{0, 0.0, false};

    if (img_a.width() != img_b.width()) return result;

    int ha = img_a.height();
    int hb = img_b.height();

    int max_search = search_height > 0 ? search_height : ha * 9 / 10;
    max_search = std::min(max_search, std::min(ha, hb));

    if (max_search < min_overlap) return result;

    int w = img_a.width();
    int stride = w * 4;
    int sample_step = std::max(1, w / 24);
    constexpr int kBlockHeight = 8;
    constexpr int kBlockStride = 2;

    std::vector<bool> distinctive(hb, false);
    for (int r = 0; r < hb; r++) {
        const uint8_t* row = img_b.data() + r * stride;
        const uint8_t* prev = r > 0 ? img_b.data() + (r - 1) * stride : nullptr;
        double sum = 0, sum_sq = 0;
        double row_delta = 0;
        int n = 0;
        for (int x = 0; x < w; x += sample_step) {
            int idx = x * 4;
            double v = row[idx] * 0.3 + row[idx + 1] * 0.6 + row[idx + 2] * 0.1;
            sum += v;
            sum_sq += v * v;
            if (prev) {
                double pv = prev[idx] * 0.3 + prev[idx + 1] * 0.6 + prev[idx + 2] * 0.1;
                row_delta += std::abs(v - pv);
            }
            n++;
        }
        double var = n > 0 ? (sum_sq / n - (sum / n) * (sum / n)) : 0;
        double avg_delta = n > 0 ? row_delta / n : 0;
        distinctive[r] = (var > 30.0) || (avg_delta > 8.0);
    }

    struct CandidateScore {
        int overlap = 0;
        int matched_blocks = 0;
        int considered_blocks = 0;
        int matched_rows = 0;
        int distinctive_rows = 0;
        double matched_error = 0.0;
        double score = -1.0;
    };

    auto evaluate = [&](int o) -> CandidateScore {
        CandidateScore candidate;
        candidate.overlap = o;

        std::vector<double> row_avg_diff(o, 999.0);
        std::vector<bool> row_match(o, false);

        for (int r = 0; r < o; r++) {
            int a_row = ha - o + r;
            if (a_row < 0 || a_row >= ha) continue;

            const uint8_t* ra = img_a.data() + a_row * stride;
            const uint8_t* rb = img_b.data() + r * stride;

            double diff = 0;
            int cnt = 0;
            for (int x = 0; x < w; x += sample_step) {
                int idx = x * 4;
                diff += std::abs(static_cast<int>(ra[idx]) - static_cast<int>(rb[idx]));
                diff += std::abs(static_cast<int>(ra[idx + 1]) - static_cast<int>(rb[idx + 1]));
                diff += std::abs(static_cast<int>(ra[idx + 2]) - static_cast<int>(rb[idx + 2]));
                cnt += 3;
            }
            row_avg_diff[r] = cnt > 0 ? diff / cnt : 999.0;
            row_match[r] = row_avg_diff[r] <= match_threshold;
            if (distinctive[r]) {
                candidate.distinctive_rows++;
                if (row_match[r]) candidate.matched_rows++;
            }
        }

        int window_height = std::min(kBlockHeight, o);
        int step = o <= kBlockHeight ? window_height : kBlockStride;
        for (int start = 0; start + window_height <= o; start += step) {
            int block_distinctive = 0;
            int block_matches = 0;
            double block_error = 0.0;
            for (int r = start; r < start + window_height; r++) {
                if (!distinctive[r]) continue;
                block_distinctive++;
                block_error += row_avg_diff[r];
                if (row_match[r]) block_matches++;
            }
            if (block_distinctive < std::max(2, window_height / 3)) continue;
            candidate.considered_blocks++;
            if (block_matches == block_distinctive) {
                candidate.matched_blocks++;
                candidate.matched_error += block_error / block_distinctive;
            }
        }

        double block_ratio = candidate.considered_blocks > 0
            ? static_cast<double>(candidate.matched_blocks) / candidate.considered_blocks
            : 0.0;
        double row_ratio = candidate.distinctive_rows > 0
            ? static_cast<double>(candidate.matched_rows) / candidate.distinctive_rows
            : 0.0;
        double error_bonus = candidate.matched_blocks > 0
            ? 1.0 / (1.0 + candidate.matched_error / candidate.matched_blocks)
            : 0.0;

        candidate.score =
            block_ratio * 10000.0 +
            row_ratio * 1000.0 +
            candidate.matched_blocks * 10.0 +
            candidate.matched_rows * 0.1 +
            error_bonus * 0.01;
        return candidate;
    };

    CandidateScore best;
    for (int o = min_overlap; o <= max_search; o++) {
        auto candidate = evaluate(o);
        if (candidate.score > best.score ||
            (candidate.score == best.score && candidate.overlap > best.overlap)) {
            best = candidate;
        }
    }

    int min_required_matches = best.considered_blocks >= 3 ? 2 : 1;
    double best_block_ratio = best.considered_blocks > 0
        ? static_cast<double>(best.matched_blocks) / best.considered_blocks
        : 0.0;
    double best_row_ratio = best.distinctive_rows > 0
        ? static_cast<double>(best.matched_rows) / best.distinctive_rows
        : 0.0;
    bool strong_row_only_match =
        best.considered_blocks == 0 &&
        best.matched_rows >= 6 &&
        best_row_ratio >= 0.85;

    if (((best.considered_blocks > 0 &&
          best.matched_blocks >= min_required_matches &&
          best_block_ratio >= 0.6 &&
          best.matched_rows >= 3 &&
          best_row_ratio >= 0.5) ||
         strong_row_only_match)) {
        result.offset = best.overlap;
        result.score = best.score;
        result.found = true;
    }

    return result;
}

std::unique_ptr<Image> stitch_two(const Image& img_a, const Image& img_b,
                                  int overlap_rows) {
    int w = img_a.width();
    int seam_cut = std::clamp(choose_seam_cut(img_a, img_b, overlap_rows), 0, overlap_rows);
    int keep_a_rows = img_a.height() - overlap_rows + seam_cut;
    int keep_b_start = seam_cut;
    int new_height = keep_a_rows + (img_b.height() - keep_b_start);
    auto out = std::make_unique<Image>(w, new_height);

    int bpr = w * 4;
    if (keep_a_rows > 0) {
        std::memcpy(out->data(), img_a.data(), keep_a_rows * bpr);
    }

    int b_remaining = img_b.height() - keep_b_start;
    if (b_remaining > 0) {
        std::memcpy(out->data() + keep_a_rows * bpr,
                    img_b.data() + keep_b_start * bpr,
                    b_remaining * bpr);
    }

    return out;
}

// Rotate image 90 degrees clockwise
static std::unique_ptr<Image> rotate_cw(const Image& src) {
    int sw = src.width(), sh = src.height();
    auto dst = std::make_unique<Image>(sh, sw);
    for (int y = 0; y < sh; y++) {
        const uint8_t* row = src.data() + y * sw * 4;
        for (int x = 0; x < sw; x++) {
            int dx = sh - 1 - y;
            int dy = x;
            std::memcpy(dst->data() + (dy * sh + dx) * 4, row + x * 4, 4);
        }
    }
    return dst;
}

// Rotate image 90 degrees counter-clockwise
static std::unique_ptr<Image> rotate_ccw(const Image& src) {
    int sw = src.width(), sh = src.height();
    auto dst = std::make_unique<Image>(sh, sw);
    for (int y = 0; y < sh; y++) {
        const uint8_t* row = src.data() + y * sw * 4;
        for (int x = 0; x < sw; x++) {
            int dx = y;
            int dy = sw - 1 - x;
            std::memcpy(dst->data() + (dy * sh + dx) * 4, row + x * 4, 4);
        }
    }
    return dst;
}

StitchResult stitch_images(const std::vector<std::string>& frame_paths,
                           double match_threshold, bool horizontal) {
    StitchResult result;

    if (frame_paths.empty()) {
        result.error = "No frame paths provided";
        return result;
    }

    auto current = Image::load(frame_paths[0]);
    if (!current) {
        result.error = "Failed to load: " + frame_paths[0];
        return result;
    }

    if (horizontal) current = rotate_cw(*current);

    for (size_t i = 1; i < frame_paths.size(); i++) {
        auto next = Image::load(frame_paths[i]);
        if (!next) {
            result.error = "Failed to load: " + frame_paths[i];
            return result;
        }

        if (horizontal) next = rotate_cw(*next);

        if (current->width() != next->width()) {
            result.error = "Width mismatch at frame " + std::to_string(i);
            return result;
        }

        auto overlap = find_overlap(*current, *next, 0, 10, match_threshold);
        int overlap_rows = overlap.found ? overlap.offset : 0;
        current = stitch_two(*current, *next, overlap_rows);
    }

    if (horizontal) current = rotate_ccw(*current);

    result.image = std::move(current);
    return result;
}

StitchResult stitch_buffers(const std::vector<const uint8_t*>& buffers,
                            const std::vector<int>& widths,
                            const std::vector<int>& heights,
                            double match_threshold, bool horizontal) {
    StitchResult result;

    if (buffers.empty()) {
        result.error = "No buffers provided";
        return result;
    }

    auto current = std::make_unique<Image>(widths[0], heights[0]);
    std::memcpy(current->data(), buffers[0], widths[0] * heights[0] * 4);

    if (horizontal) current = rotate_cw(*current);

    for (size_t i = 1; i < buffers.size(); i++) {
        auto next = std::make_unique<Image>(widths[i], heights[i]);
        std::memcpy(next->data(), buffers[i], widths[i] * heights[i] * 4);

        if (horizontal) next = rotate_cw(*next);

        if (current->width() != next->width()) {
            result.error = "Width mismatch at buffer " + std::to_string(i);
            return result;
        }

        auto overlap = find_overlap(*current, *next, 0, 10, match_threshold);
        int overlap_rows = overlap.found ? overlap.offset : 0;
        current = stitch_two(*current, *next, overlap_rows);
    }

    if (horizontal) current = rotate_ccw(*current);

    result.image = std::move(current);
    return result;
}

} // namespace seamline
```

- [ ] **Step 3: Commit**

```
feat: add core stitch algorithm (ported from snapr-cli)
```

---

## Task 4: Public C API

**Files:**
- Create: `include/seamline.h`
- Create: `src/seamline.cpp`

- [ ] **Step 1: Create include/seamline.h**

```c
#ifndef SEAMLINE_H
#define SEAMLINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#   ifdef SEAMLINE_BUILD_DLL
#       define SEAMLINE_API __declspec(dllexport)
#   elif defined(SEAMLINE_USE_DLL)
#       define SEAMLINE_API __declspec(dllimport)
#   else
#       define SEAMLINE_API
#   endif
#else
#   define SEAMLINE_API __attribute__((visibility("default")))
#endif

typedef struct {
    const uint8_t* data;   /* RGBA pixel buffer */
    int width;
    int height;
} SeamlineImage;

typedef struct {
    double match_threshold;  /* 0-255, default 5.0 */
    int horizontal;          /* 0 = vertical, 1 = horizontal */
} SeamlineOptions;

typedef struct {
    uint8_t* data;           /* RGBA result (free with seamline_free) */
    int width;
    int height;
    int ok;                  /* 1 = success, 0 = error */
    char error[256];
} SeamlineResult;

SEAMLINE_API SeamlineOptions seamline_default_options(void);

SEAMLINE_API SeamlineResult seamline_stitch(const SeamlineImage* images, int count,
                                            SeamlineOptions options);

SEAMLINE_API SeamlineResult seamline_stitch_files(const char** paths, int count,
                                                  SeamlineOptions options);

SEAMLINE_API int seamline_save_png(const SeamlineResult* result, const char* path);
SEAMLINE_API int seamline_save_jpg(const SeamlineResult* result, const char* path, int quality);

SEAMLINE_API void seamline_free(SeamlineResult* result);

#ifdef __cplusplus
}
#endif

#endif /* SEAMLINE_H */
```

- [ ] **Step 2: Create src/seamline.cpp**

```cpp
#include "seamline.h"
#include "stitch.h"
#include <cstring>
#include <cstdlib>

static SeamlineResult make_error(const char* msg) {
    SeamlineResult r{};
    r.ok = 0;
    strncpy(r.error, msg, sizeof(r.error) - 1);
    return r;
}

static SeamlineResult make_success(seamline::Image& img) {
    SeamlineResult r{};
    r.ok = 1;
    r.width = img.width();
    r.height = img.height();
    size_t sz = img.size();
    r.data = static_cast<uint8_t*>(std::malloc(sz));
    std::memcpy(r.data, img.data(), sz);
    return r;
}

extern "C" {

SeamlineOptions seamline_default_options(void) {
    SeamlineOptions opts;
    opts.match_threshold = 5.0;
    opts.horizontal = 0;
    return opts;
}

SeamlineResult seamline_stitch(const SeamlineImage* images, int count,
                               SeamlineOptions options) {
    if (!images || count < 1) return make_error("No images provided");

    std::vector<const uint8_t*> buffers(count);
    std::vector<int> widths(count), heights(count);
    for (int i = 0; i < count; i++) {
        if (!images[i].data) return make_error("Null image data");
        buffers[i] = images[i].data;
        widths[i] = images[i].width;
        heights[i] = images[i].height;
    }

    auto result = seamline::stitch_buffers(buffers, widths, heights,
                                           options.match_threshold,
                                           options.horizontal != 0);
    if (!result.ok()) return make_error(result.error.c_str());
    return make_success(*result.image);
}

SeamlineResult seamline_stitch_files(const char** paths, int count,
                                     SeamlineOptions options) {
    if (!paths || count < 1) return make_error("No paths provided");

    std::vector<std::string> frame_paths;
    frame_paths.reserve(count);
    for (int i = 0; i < count; i++) {
        if (!paths[i]) return make_error("Null path");
        frame_paths.emplace_back(paths[i]);
    }

    auto result = seamline::stitch_images(frame_paths,
                                          options.match_threshold,
                                          options.horizontal != 0);
    if (!result.ok()) return make_error(result.error.c_str());
    return make_success(*result.image);
}

int seamline_save_png(const SeamlineResult* result, const char* path) {
    if (!result || !result->ok || !result->data || !path) return 0;
    seamline::Image img(result->width, result->height);
    std::memcpy(img.data(), result->data, img.size());
    return img.save_png(path) ? 1 : 0;
}

int seamline_save_jpg(const SeamlineResult* result, const char* path, int quality) {
    if (!result || !result->ok || !result->data || !path) return 0;
    seamline::Image img(result->width, result->height);
    std::memcpy(img.data(), result->data, img.size());
    return img.save_jpg(path, quality) ? 1 : 0;
}

void seamline_free(SeamlineResult* result) {
    if (result && result->data) {
        std::free(result->data);
        result->data = nullptr;
    }
}

} // extern "C"
```

- [ ] **Step 3: Commit**

```
feat: add public C API wrapper
```

---

## Task 5: Unit Tests

**Files:**
- Create: `tests/CMakeLists.txt`
- Create: `tests/test_stitch.cpp`

- [ ] **Step 1: Create tests/CMakeLists.txt**

```cmake
add_executable(test_seamline test_stitch.cpp)
target_link_libraries(test_seamline PRIVATE seamline Catch2::Catch2WithMain)
target_include_directories(test_seamline PRIVATE ${CMAKE_SOURCE_DIR}/src)

include(CTest)
include(Catch)
catch_discover_tests(test_seamline)
```

- [ ] **Step 2: Create tests/test_stitch.cpp**

Port tests from `snapr-cli/tests/test_stitch.cpp`, changing namespace to `seamline` and adding C API tests:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "stitch.h"
#include "seamline.h"
#include <algorithm>
#include <cstring>

// --- Helpers (same as snapr-cli tests) ---

static void fill_row(seamline::Image& img, int row, uint8_t gray) {
    uint8_t* p = img.data() + row * img.stride();
    for (int x = 0; x < img.width(); x++) {
        p[x * 4 + 0] = gray;
        p[x * 4 + 1] = gray;
        p[x * 4 + 2] = gray;
        p[x * 4 + 3] = 255;
    }
}

static uint32_t xorshift(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static void fill_random(seamline::Image& img, int start_row, int num_rows, uint32_t seed) {
    uint32_t state = seed;
    for (int r = 0; r < num_rows && (start_row + r) < img.height(); r++) {
        uint8_t* p = img.data() + (start_row + r) * img.stride();
        for (int x = 0; x < img.width(); x++) {
            uint32_t v = xorshift(state);
            p[x * 4 + 0] = static_cast<uint8_t>(v & 0xFF);
            p[x * 4 + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
            p[x * 4 + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
            p[x * 4 + 3] = 255;
        }
    }
}

// --- Internal C++ API tests ---

TEST_CASE("find_overlap detects exact overlap", "[stitch]") {
    seamline::Image a(30, 100);
    seamline::Image b(30, 100);

    fill_random(a, 0, 100, 12345);
    std::memcpy(b.data(), a.data() + 60 * a.stride(), 40 * a.stride());
    fill_random(b, 40, 60, 99999);

    auto result = seamline::find_overlap(a, b);
    REQUIRE(result.found);
    REQUIRE(result.offset == 40);
}

TEST_CASE("find_overlap returns not found for no overlap", "[stitch]") {
    seamline::Image a(30, 50);
    seamline::Image b(30, 50);

    fill_random(a, 0, 50, 11111);
    fill_random(b, 0, 50, 77777);

    auto result = seamline::find_overlap(a, b);
    REQUIRE_FALSE(result.found);
}

TEST_CASE("stitch_two combines images removing overlap", "[stitch]") {
    seamline::Image a(4, 60);
    seamline::Image b(4, 60);
    auto result = seamline::stitch_two(a, b, 20);
    REQUIRE(result != nullptr);
    REQUIRE(result->width() == 4);
    REQUIRE(result->height() == 100);
}

TEST_CASE("stitch_two with zero overlap concatenates fully", "[stitch]") {
    seamline::Image a(4, 50);
    seamline::Image b(4, 30);
    auto result = seamline::stitch_two(a, b, 0);
    REQUIRE(result != nullptr);
    REQUIRE(result->height() == 80);
}

// --- Public C API tests ---

TEST_CASE("C API: seamline_stitch with buffer input", "[c-api]") {
    seamline::Image a(30, 100);
    seamline::Image b(30, 100);

    fill_random(a, 0, 100, 12345);
    std::memcpy(b.data(), a.data() + 60 * a.stride(), 40 * a.stride());
    fill_random(b, 40, 60, 99999);

    SeamlineImage images[2] = {
        {a.data(), a.width(), a.height()},
        {b.data(), b.width(), b.height()},
    };

    SeamlineOptions opts = seamline_default_options();
    SeamlineResult result = seamline_stitch(images, 2, opts);

    REQUIRE(result.ok == 1);
    REQUIRE(result.width == 30);
    REQUIRE(result.height > 0);
    REQUIRE(result.height <= 200); // at most concatenation
    REQUIRE(result.height >= 160); // at least some overlap removed

    seamline_free(&result);
    REQUIRE(result.data == nullptr);
}

TEST_CASE("C API: seamline_stitch with null input returns error", "[c-api]") {
    SeamlineOptions opts = seamline_default_options();
    SeamlineResult result = seamline_stitch(nullptr, 0, opts);
    REQUIRE(result.ok == 0);
    REQUIRE(strlen(result.error) > 0);
}

TEST_CASE("C API: default options", "[c-api]") {
    SeamlineOptions opts = seamline_default_options();
    REQUIRE(opts.match_threshold == 5.0);
    REQUIRE(opts.horizontal == 0);
}
```

- [ ] **Step 3: Build and run tests**

Run:
```bash
cd build && cmake .. -DSEAMLINE_BUILD_TESTS=ON && cmake --build .
ctest --output-on-failure
```
Expected: All tests PASS

- [ ] **Step 4: Commit**

```
feat: add unit tests for stitch algorithm and C API
```

---

## Task 6: Platform Build Scripts

**Files:**
- Create: `scripts/build-ios.sh`
- Create: `scripts/build-android.sh`

- [ ] **Step 1: Create scripts/build-ios.sh**

```bash
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
```

- [ ] **Step 2: Create scripts/build-android.sh**

```bash
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
```

- [ ] **Step 3: Make scripts executable and commit**

Run: `chmod +x scripts/build-ios.sh scripts/build-android.sh`

```
feat: add iOS and Android build scripts
```

---

## Task 7: Verify Full Build

- [ ] **Step 1: Clean build from scratch**

Run:
```bash
rm -rf build
mkdir build && cd build
cmake .. -DSEAMLINE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build .
```
Expected: Compiles with no errors or warnings

- [ ] **Step 2: Run tests**

Run: `cd build && ctest --output-on-failure`
Expected: All tests PASS

- [ ] **Step 3: Verify library output**

Run: `ls -la build/libseamline.a && nm build/libseamline.a | grep seamline_`
Expected: Symbols for `seamline_stitch`, `seamline_stitch_files`, `seamline_free`, etc.

- [ ] **Step 4: Commit any fixes and tag**

```
chore: verify clean build and test pass
```

---

## Summary

| Task | Description | Key Output |
|------|-------------|------------|
| 1 | Project skeleton | CMakeLists.txt, deps |
| 2 | Image class | src/image.{h,cpp} |
| 3 | Core algorithm | src/stitch.{h,cpp} |
| 4 | C API | include/seamline.h, src/seamline.cpp |
| 5 | Tests | tests/test_stitch.cpp |
| 6 | Platform scripts | scripts/build-{ios,android}.sh |
| 7 | Integration verify | Full build + test |
