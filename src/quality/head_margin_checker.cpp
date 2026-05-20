#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class HeadMarginChecker : public IQualityChecker {
public:
    const char* name() const override { return "head_margin_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "composition";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "No face detected; cannot check head margin";
            return result;
        }

        double top_ratio = static_cast<double>(face.bbox.y) / image.rows;

        result.actual_value = top_ratio;
        result.min_threshold = static_cast<double>(std.top_margin_ratio_min);
        result.max_threshold = static_cast<double>(std.top_margin_ratio_max);

        if (top_ratio < std.top_margin_ratio_min) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "Head too close to top: ratio = "
                           + std::to_string(top_ratio).substr(0, 4)
                           + " < " + std::to_string(std.top_margin_ratio_min);
        } else if (top_ratio > std.top_margin_ratio_max) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Head too far from top: ratio = "
                           + std::to_string(top_ratio).substr(0, 4)
                           + " > " + std::to_string(std.top_margin_ratio_max);
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Head top margin OK: ratio = "
                           + std::to_string(top_ratio).substr(0, 4);
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, HeadMarginChecker, "head_margin_check")

} // namespace photo
