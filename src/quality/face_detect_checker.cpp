#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class FaceDetectChecker : public IQualityChecker {
public:
    const char* name() const override { return "face_detect_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat&, const FaceInfo& face,
                      const IDPhotoStandard&) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "face";

        result.actual_value = face.detected ? 1.0 : 0.0;
        result.min_threshold = 1.0;

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "No face detected in the image";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Face detected successfully";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, FaceDetectChecker, "face_detect_check")

} // namespace photo
