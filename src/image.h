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
