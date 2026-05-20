#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <cmath>

namespace photo {

class HeadTiltChecker : public IQualityChecker {
public:
    const char* name() const override { return "head_tilt_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat&, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        constexpr double kMaxTiltAngle = 5.0;

        CheckResult result;
        result.checker_name = name();
        result.category = "composition";

        if (!face.detected || face.landmarks.size() < 68) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "No landmarks; using head pose roll for tilt check";
            double roll = std::abs(face.roll);
            result.actual_value = roll;
            result.max_threshold = kMaxTiltAngle;
            if (roll > kMaxTiltAngle) {
                result.passed = false;
                result.severity = Severity::FAIL;
                result.message = "Head tilted: roll = " + std::to_string(roll).substr(0,4) + " deg";
            } else {
                result.passed = true;
                result.message = "Head tilt OK: roll = " + std::to_string(roll).substr(0,4) + " deg";
            }
            return result;
        }

        // Compute tilt from eye landmarks (left eye 36-41, right eye 42-47)
        cv::Point2f left_eye(0,0), right_eye(0,0);
        for (int i = 36; i <= 41; i++) left_eye += face.landmarks[i];
        left_eye *= (1.0f / 6.0f);
        for (int i = 42; i <= 47; i++) right_eye += face.landmarks[i];
        right_eye *= (1.0f / 6.0f);

        double dx = right_eye.x - left_eye.x;
        double dy = right_eye.y - left_eye.y;
        double angle = std::abs(std::atan2(dy, dx) * 180.0 / CV_PI);

        result.actual_value = angle;
        result.max_threshold = kMaxTiltAngle;

        if (angle > kMaxTiltAngle) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "Head tilted: eye-line angle = "
                           + std::to_string(angle).substr(0,4) + " deg";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Head tilt OK: angle = " + std::to_string(angle).substr(0,4) + " deg";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, HeadTiltChecker, "head_tilt_check")

} // namespace photo
