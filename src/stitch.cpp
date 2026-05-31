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
    int seam_cut = choose_seam_cut(img_a, img_b, overlap_rows);
    seam_cut = std::max(0, std::min(seam_cut, overlap_rows));
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

static std::unique_ptr<Image> crop_width(const Image& src, int target_width) {
    if (src.width() == target_width) return nullptr;
    auto dst = std::make_unique<Image>(target_width, src.height());
    int copy_bytes = target_width * 4;
    for (int y = 0; y < src.height(); y++) {
        std::memcpy(dst->data() + y * copy_bytes,
                    src.data() + y * src.stride(),
                    copy_bytes);
    }
    return dst;
}

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
                           double match_threshold, bool horizontal,
                           bool auto_crop) {
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
            if (!auto_crop) {
                result.error = "Width mismatch at frame " + std::to_string(i);
                return result;
            }
            int min_w = std::min(current->width(), next->width());
            if (current->width() != min_w) {
                current = crop_width(*current, min_w);
            }
            if (next->width() != min_w) {
                next = crop_width(*next, min_w);
            }
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
                            double match_threshold, bool horizontal,
                            bool auto_crop) {
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
            if (!auto_crop) {
                result.error = "Width mismatch at buffer " + std::to_string(i);
                return result;
            }
            int min_w = std::min(current->width(), next->width());
            if (current->width() != min_w) {
                current = crop_width(*current, min_w);
            }
            if (next->width() != min_w) {
                next = crop_width(*next, min_w);
            }
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
