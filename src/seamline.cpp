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
    opts.auto_crop = 0;
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
                                           options.horizontal != 0,
                                           options.auto_crop != 0);
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
                                          options.horizontal != 0,
                                          options.auto_crop != 0);
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
