#include "photo/core/image.hpp"
#include <opencv2/imgcodecs.hpp>
#include <fstream>

namespace photo {

Image::Image(const std::string& filepath)
    : filepath_(filepath)
{
    load(filepath);
}

bool Image::load(const std::string& filepath) {
    filepath_ = filepath;
    mat_ = cv::imread(filepath, cv::IMREAD_COLOR);
    if (mat_.empty()) return false;

    // Determine file size
    std::ifstream f(filepath, std::ios::binary | std::ios::ate);
    if (f.is_open()) {
        file_size_ = static_cast<size_t>(f.tellg());
    }

    // Determine format from extension
    auto dot = filepath.find_last_of('.');
    if (dot != std::string::npos) {
        format_ = filepath.substr(dot + 1);
        // Uppercase
        for (auto& c : format_) c = static_cast<char>(std::toupper(c));
    }

    return true;
}

bool Image::save(const std::string& filepath) const {
    if (mat_.empty()) return false;
    return cv::imwrite(filepath, mat_);
}

} // namespace photo
