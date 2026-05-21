#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class FileDimensionChecker : public IQualityChecker {
public:
    const char* name() const override { return "file_dimension_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo&,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "file";

        result.actual_value = image.cols;
        result.min_threshold = std.expected_width;
        result.max_threshold = std.expected_width;

        bool w_ok = (image.cols == std.expected_width);
        bool h_ok = (image.rows == std.expected_height);

        if (w_ok && h_ok) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Dimensions OK: " + std::to_string(image.cols) + "x" + std::to_string(image.rows);
        } else {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Wrong dimensions: " + std::to_string(image.cols) + "x" + std::to_string(image.rows)
                           + " (expected " + std::to_string(std.expected_width) + "x" + std::to_string(std.expected_height) + ")";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, FileDimensionChecker, "file_dimension_check")

} // namespace photo
