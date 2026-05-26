#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class HeadMarginChecker : public IQualityChecker {
public:
    const char* name() const override { return "head_margin_check"; }
    const char* version() const override { return "1.1.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "composition";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "未检测到人脸，无法检查头顶边距";
            return result;
        }

        // bbox.y 包含额头，但不含头发。扣除图像高度 3% 的头发边距
        double hair_margin = 0.03 * image.rows;
        double est_head_top = face.bbox.y - hair_margin;
        if (est_head_top < 0) est_head_top = 0;
        double top_ratio = est_head_top / image.rows;

        result.actual_value = top_ratio;
        result.min_threshold = static_cast<double>(std.top_margin_ratio_min);
        result.max_threshold = static_cast<double>(std.top_margin_ratio_max);

        if (top_ratio < std.top_margin_ratio_min) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "头顶留白不足: ratio = "
                           + std::to_string(static_cast<int>(top_ratio * 100))
                           + "%, 要求 ≥"
                           + std::to_string(static_cast<int>(std.top_margin_ratio_min * 100)) + "%";
        } else if (top_ratio > std.top_margin_ratio_max) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "头顶留白过多: ratio = "
                           + std::to_string(static_cast<int>(top_ratio * 100))
                           + "%, 要求 ≤"
                           + std::to_string(static_cast<int>(std.top_margin_ratio_max * 100)) + "%";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "头顶留白正常: "
                           + std::to_string(static_cast<int>(top_ratio * 100)) + "%";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, HeadMarginChecker, "head_margin_check")

} // namespace photo
