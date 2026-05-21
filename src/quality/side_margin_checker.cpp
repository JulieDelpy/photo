#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class SideMarginChecker : public IQualityChecker {
public:
    const char* name() const override { return "side_margin_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "composition";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "No face detected; cannot check side margins";
            return result;
        }

        int left_margin = face.bbox.x;
        int right_margin = image.cols - (face.bbox.x + face.bbox.width);
        int min_side = std::min(left_margin, right_margin);

        result.actual_value = min_side;
        result.min_threshold = std.side_margin_min_px;

        if (left_margin < std.side_margin_min_px || right_margin < std.side_margin_min_px) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Side margin too small: left="
                           + std::to_string(left_margin) + "px right="
                           + std::to_string(right_margin) + "px";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Side margins OK: left="
                           + std::to_string(left_margin) + "px right="
                           + std::to_string(right_margin) + "px";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, SideMarginChecker, "side_margin_check")

} // namespace photo
