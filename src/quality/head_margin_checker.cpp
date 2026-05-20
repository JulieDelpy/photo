#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class HeadMarginChecker : public IQualityChecker {
public:
    const char* name() const override { return "head_margin_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat&, const FaceInfo& face,
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

        int top_margin = face.bbox.y;
        result.actual_value = top_margin;
        result.min_threshold = std.top_margin_min_px;
        result.max_threshold = std.top_margin_max_px;

        if (top_margin < std.top_margin_min_px) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "Head too close to top: margin = "
                           + std::to_string(top_margin) + "px < "
                           + std::to_string(std.top_margin_min_px) + "px";
        } else if (top_margin > std.top_margin_max_px) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Head too far from top: margin = "
                           + std::to_string(top_margin) + "px > "
                           + std::to_string(std.top_margin_max_px) + "px";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Head top margin OK: " + std::to_string(top_margin) + "px";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, HeadMarginChecker, "head_margin_check")

} // namespace photo
