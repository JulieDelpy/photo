#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class FileAspectRatioChecker : public IQualityChecker {
public:
    const char* name() const override { return "file_aspect_ratio_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo&,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "file";

        if (std.expected_height == 0) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "No aspect ratio standard defined";
            return result;
        }

        double expected_ar = static_cast<double>(std.expected_width) / std.expected_height;
        double actual_ar = static_cast<double>(image.cols) / image.rows;
        double diff = std::abs(actual_ar - expected_ar);

        result.actual_value = actual_ar;
        result.max_threshold = 0.01;

        if (diff > 0.01) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "Wrong aspect ratio: "
                           + std::to_string(actual_ar).substr(0,5)
                           + " (expected " + std::to_string(expected_ar).substr(0,5) + ")";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Aspect ratio OK: " + std::to_string(actual_ar).substr(0,5);
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, FileAspectRatioChecker, "file_aspect_ratio_check")

} // namespace photo
