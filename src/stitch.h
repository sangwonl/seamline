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
                           double match_threshold = 5.0, bool horizontal = false,
                           bool auto_crop = false);

StitchResult stitch_buffers(const std::vector<const uint8_t*>& buffers,
                            const std::vector<int>& widths,
                            const std::vector<int>& heights,
                            double match_threshold = 5.0, bool horizontal = false,
                            bool auto_crop = false);

} // namespace seamline
