#pragma once

#include <opencv2/core.hpp>
#include <string>

namespace photo {

// Thin RAII wrapper around cv::Mat for ownership clarity.
// In practice the quality/beauty pipeline passes around cv::Mat directly
// since OpenCV already uses ref-counted data. This wrapper exists for
// operations that need explicit ownership (e.g., loading from disk).

class Image {
public:
    Image() = default;

    explicit Image(const std::string& filepath);
    Image(const cv::Mat& mat) : mat_(mat) {}
    Image(cv::Mat&& mat) noexcept : mat_(std::move(mat)) {}

    bool load(const std::string& filepath);
    bool save(const std::string& filepath) const;

    [[nodiscard]] bool empty() const { return mat_.empty(); }
    [[nodiscard]] int width() const { return mat_.cols; }
    [[nodiscard]] int height() const { return mat_.rows; }
    [[nodiscard]] int channels() const { return mat_.channels(); }
    [[nodiscard]] cv::Size size() const { return mat_.size(); }

    cv::Mat& mat() { return mat_; }
    const cv::Mat& mat() const { return mat_; }

    // File metadata
    const std::string& filepath() const { return filepath_; }
    size_t fileSize() const { return file_size_; }
    const std::string& format() const { return format_; }

private:
    cv::Mat mat_;
    std::string filepath_;
    size_t file_size_ = 0;
    std::string format_;
};

} // namespace photo
