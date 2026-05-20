#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class EyeClosureChecker : public IQualityChecker {
public:
    const char* name() const override { return "eye_closure_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat&, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "state";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "No face detected; cannot check eye state";
            return result;
        }

        constexpr double kEarMin = 0.20;

        double ear = std::min(face.ear_left, face.ear_right);
        result.actual_value = ear;
        result.min_threshold = kEarMin;

        if (ear < kEarMin) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "Eyes appear closed: EAR = "
                           + std::to_string(ear).substr(0, 4)
                           + " < " + std::to_string(kEarMin);
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Eyes open: EAR = " + std::to_string(ear).substr(0, 4);
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, EyeClosureChecker, "eye_closure_check")

} // namespace photo
