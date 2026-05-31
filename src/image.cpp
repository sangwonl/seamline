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
