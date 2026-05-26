#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <cmath>

namespace photo {

class HeadTiltChecker : public IQualityChecker {
public:
    const char* name() const override { return "head_tilt_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat&, const FaceInfo& face,
                      const IDPhotoStandard& cfg) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "composition";

        double max_tilt = cfg.max_head_roll > 0 ? cfg.max_head_roll : 7.5;

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "未检测到人脸，无法检查头部倾斜";
            return result;
        }

        // 直接使用 eye_tilt（两眼连线角度），与 head_pose_check 保持一致
        double angle = std::abs(face.eye_tilt);

        result.actual_value = angle;
        result.max_threshold = max_tilt;

        if (angle > max_tilt) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "头部倾斜: 眼线角度 = "
                           + std::to_string(angle).substr(0,4) + " deg";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "头部倾斜正常: angle = "
                           + std::to_string(angle).substr(0,4) + " deg";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, HeadTiltChecker, "head_tilt_check")

} // namespace photo
