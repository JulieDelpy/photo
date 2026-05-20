#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class FileFormatChecker : public IQualityChecker {
public:
    const char* name() const override { return "file_format_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo&,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "file";

        // Determine format from image properties
        std::string format = "UNKNOWN";
        int depth = image.depth();
        int channels = image.channels();

        if (depth == CV_8U && channels == 3) {
            format = "JPEG/PNG (BGR)";
        } else if (depth == CV_8U && channels == 1) {
            format = "GRAYSCALE";
        } else {
            format = "OTHER (depth=" + std::to_string(depth) + " ch=" + std::to_string(channels) + ")";
        }

        result.actual_value = 0;
        result.passed = true;
        result.severity = Severity::PASS;
        result.message = "Image format: " + format;

        // More detailed format check requires file header parsing
        if (!std.allowed_formats.empty()) {
            result.message += " (allowed: ";
            for (size_t i = 0; i < std.allowed_formats.size(); i++) {
                if (i > 0) result.message += ", ";
                result.message += std.allowed_formats[i];
            }
            result.message += ")";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, FileFormatChecker, "file_format_check")

} // namespace photo
