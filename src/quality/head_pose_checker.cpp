#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <cmath>

namespace photo {

class HeadPoseChecker : public IQualityChecker {
public:
    const char* name() const override { return "head_pose_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat&, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "state";

        constexpr double kMaxHeadYaw   = 10.0;
        constexpr double kMaxHeadPitch = 10.0;
        constexpr double kMaxHeadRoll  = 10.0;

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "No face detected; cannot check head pose";
            return result;
        }

        double max_angle = std::max({std::abs(face.yaw), std::abs(face.pitch), std::abs(face.roll)});
        double max_allowed = std::max({kMaxHeadYaw, kMaxHeadPitch, kMaxHeadRoll});

        result.actual_value = max_angle;
        result.max_threshold = max_allowed;

        bool yaw_ok   = std::abs(face.yaw) <= kMaxHeadYaw;
        bool pitch_ok = std::abs(face.pitch) <= kMaxHeadPitch;
        bool roll_ok  = std::abs(face.roll) <= kMaxHeadRoll;

        if (yaw_ok && pitch_ok && roll_ok) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Head pose OK: yaw=" + std::to_string(face.yaw).substr(0,4)
                           + " pitch=" + std::to_string(face.pitch).substr(0,4)
                           + " roll=" + std::to_string(face.roll).substr(0,4);
        } else {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.detail = "yaw=" + std::to_string(face.yaw).substr(0,4)
                          + " pitch=" + std::to_string(face.pitch).substr(0,4)
                          + " roll=" + std::to_string(face.roll).substr(0,4);
            if (!yaw_ok) result.message = "Head turned too far left/right (yaw="
                + std::to_string(face.yaw).substr(0,4) + ")";
            else if (!pitch_ok) result.message = "Head tilted too far up/down (pitch="
                + std::to_string(face.pitch).substr(0,4) + ")";
            else result.message = "Head tilted (roll="
                + std::to_string(face.roll).substr(0,4) + ")";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, HeadPoseChecker, "head_pose_check")

} // namespace photo
