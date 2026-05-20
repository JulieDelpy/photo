#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class ChinMarginChecker : public IQualityChecker {
public:
    const char* name() const override { return "chin_margin_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "composition";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "No face detected; cannot check chin margin";
            return result;
        }

        int chin_bottom = face.bbox.y + face.bbox.height;
        int bottom_margin = image.rows - chin_bottom;

        result.actual_value = bottom_margin;
        result.min_threshold = std.bottom_margin_min_px;

        if (bottom_margin < std.bottom_margin_min_px) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "Chin too close to bottom: margin = "
                           + std::to_string(bottom_margin) + "px";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Chin bottom margin OK: " + std::to_string(bottom_margin) + "px";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, ChinMarginChecker, "chin_margin_check")

} // namespace photo
