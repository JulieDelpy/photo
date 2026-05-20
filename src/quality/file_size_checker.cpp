#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <fstream>

namespace photo {

class FileSizeChecker : public IQualityChecker {
public:
    const char* name() const override { return "file_size_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat&, const FaceInfo&,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "file";

        // File size is passed through the standard thresholds.
        // Actual file size check requires the original file path.
        // This checker reports thresholds for reference.

        if (std.file_size_min_kb == 0 && std.file_size_max_kb == 0) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "No file size constraints defined";
            result.actual_value = 0;
            return result;
        }

        result.min_threshold = std.file_size_min_kb;
        result.max_threshold = std.file_size_max_kb;
        result.actual_value = 0;  // Require file path for actual check
        result.passed = true;
        result.severity = Severity::PASS;
        result.message = "File size: expected "
                       + std::to_string(std.file_size_min_kb) + "-"
                       + std::to_string(std.file_size_max_kb) + " KB";
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, FileSizeChecker, "file_size_check")

} // namespace photo
