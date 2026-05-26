#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class EyePositionChecker : public IQualityChecker {
public:
    const char* name() const override { return "eye_position_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "composition";

        if (!face.detected || face.landmarks.size() < 68) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "无特征点，无法检查眼睛位置";
            return result;
        }

        // Left eye center: average of landmarks 36-41 (68-point model)
        // Right eye center: average of landmarks 42-47
        cv::Point2f left_eye(0,0), right_eye(0,0);
        for (int i = 36; i <= 41; i++) left_eye += face.landmarks[i];
        left_eye *= (1.0f / 6.0f);
        for (int i = 42; i <= 47; i++) right_eye += face.landmarks[i];
        right_eye *= (1.0f / 6.0f);

        float eye_y = std::max(left_eye.y, right_eye.y);
        double eye_ratio = static_cast<double>(eye_y) / image.rows;

        result.actual_value = eye_ratio;
        result.min_threshold = static_cast<double>(std.eye_position_ratio_min);

        if (eye_ratio < std.eye_position_ratio_min) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "眼睛位置偏上: ratio = "
                           + std::to_string(eye_ratio).substr(0, 4);
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "眼睛位置正常: ratio = "
                           + std::to_string(eye_ratio).substr(0, 4);
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, EyePositionChecker, "eye_position_check")

} // namespace photo
