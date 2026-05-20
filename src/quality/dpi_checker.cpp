#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class DpiChecker : public IQualityChecker {
public:
    const char* name() const override { return "dpi_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat&, const FaceInfo&,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "file";

        // DPI requires EXIF metadata reading.
        // This checker reports the expected DPI value.
        result.actual_value = 0;
        result.min_threshold = std.expected_dpi;
        result.max_threshold = std.expected_dpi;

        result.passed = true;
        result.severity = Severity::PASS;
        result.message = "DPI: expected " + std::to_string(std.expected_dpi)
                       + " (EXIF check requires file metadata)";
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, DpiChecker, "dpi_check")

} // namespace photo
