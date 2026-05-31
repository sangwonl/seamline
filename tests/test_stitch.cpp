#include <catch2/catch_test_macros.hpp>
#include "stitch.h"
#include "seamline.h"
#include <algorithm>
#include <cstring>

// --- Helpers ---

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

static void fill_rect(seamline::Image& img, int x0, int y0, int x1, int y1, uint8_t gray) {
    x0 = std::max(0, x0);
    y0 = std::max(0, y0);
    x1 = std::min(img.width(), x1);
    y1 = std::min(img.height(), y1);
    for (int y = y0; y < y1; y++) {
        uint8_t* p = img.data() + y * img.stride();
        for (int x = x0; x < x1; x++) {
            p[x * 4 + 0] = gray;
            p[x * 4 + 1] = gray;
            p[x * 4 + 2] = gray;
            p[x * 4 + 3] = 255;
        }
    }
}

static seamline::Image make_terminal_like_source(int width, int height) {
    seamline::Image img(width, height);
    for (int r = 0; r < height; r++) fill_row(img, r, 250);

    for (int r = 0; r < height; r++) {
        if (r % 18 == 0) fill_row(img, r, 226);
        if (r % 18 == 1) fill_row(img, r, 238);
    }

    for (int line = 0; line < height / 18; line++) {
        int y = line * 18 + 4;
        if (y + 8 >= height) break;
        uint32_t state = 1000 + line * 7919;
        for (int token = 0; token < 6; token++) {
            int x = 8 + token * 24 + static_cast<int>(xorshift(state) % 10);
            int w = 8 + static_cast<int>(xorshift(state) % 18);
            uint8_t gray = static_cast<uint8_t>(40 + (xorshift(state) % 150));
            fill_rect(img, x, y, x + w, y + 5, gray);
        }
    }

    return img;
}

static seamline::Image make_sheet_like_source(int width, int height) {
    seamline::Image img(width, height);
    for (int r = 0; r < height; r++) fill_row(img, r, 255);

    for (int y = 0; y < height; y += 20) fill_row(img, y, 210);
    for (int x = 0; x < width; x += 24) {
        for (int y = 0; y < height; y++) {
            uint8_t* p = img.data() + y * img.stride() + x * 4;
            p[0] = p[1] = p[2] = 210;
            p[3] = 255;
        }
    }

    for (int cell_y = 0; cell_y < height / 20; cell_y++) {
        for (int cell_x = 0; cell_x < width / 24; cell_x++) {
            uint32_t state = 5000 + cell_y * 131 + cell_x * 17;
            uint8_t gray = static_cast<uint8_t>(40 + (xorshift(state) % 120));
            int x = cell_x * 24 + 4;
            int y = cell_y * 20 + 5;
            fill_rect(img, x, y, x + 10, y + 3, gray);
            fill_rect(img, x + 12, y, x + 16, y + 3, static_cast<uint8_t>(gray + 30));
        }
    }

    return img;
}

static seamline::Image crop_rows(const seamline::Image& src, int start_row, int rows) {
    seamline::Image out(src.width(), rows);
    std::memcpy(out.data(),
                src.data() + start_row * src.stride(),
                rows * src.stride());
    return out;
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

TEST_CASE("find_overlap detects overlap on terminal-like periodic content", "[stitch]") {
    auto source = make_terminal_like_source(180, 240);
    auto a = crop_rows(source, 0, 140);
    auto b = crop_rows(source, 103, 137);

    auto result = seamline::find_overlap(a, b);
    REQUIRE(result.found);
    REQUIRE(result.offset == 37);
}

TEST_CASE("find_overlap detects overlap on sheet-like grid content", "[stitch]") {
    auto source = make_sheet_like_source(192, 260);
    auto a = crop_rows(source, 0, 150);
    auto b = crop_rows(source, 133, 120);

    auto result = seamline::find_overlap(a, b);
    REQUIRE(result.found);
    REQUIRE(result.offset == 17);
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
    REQUIRE(result.height <= 200);
    REQUIRE(result.height >= 160);

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
