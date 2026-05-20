#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class MouthOpenChecker : public IQualityChecker {
public:
    const char* name() const override { return "mouth_open_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat&, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "state";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "No face detected; cannot check mouth state";
            return result;
        }

        constexpr double kMarMax = 0.25;

        result.actual_value = face.mar;
        result.max_threshold = kMarMax;

        if (face.mar > kMarMax) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "Mouth appears open: MAR = "
                           + std::to_string(face.mar).substr(0, 4)
                           + " > " + std::to_string(kMarMax);
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Mouth closed: MAR = " + std::to_string(face.mar).substr(0, 4);
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, MouthOpenChecker, "mouth_open_check")

} // namespace photo
