#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class FaceConfidenceChecker : public IQualityChecker {
public:
    const char* name() const override { return "face_confidence_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat&, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "face";

        result.actual_value = face.confidence;
        result.min_threshold = static_cast<double>(std.min_face_confidence);

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "No face detected";
        } else if (face.confidence < std.min_face_confidence) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Face detection confidence low: "
                           + std::to_string(face.confidence)
                           + " < " + std::to_string(std.min_face_confidence);
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Face detection confidence OK: "
                           + std::to_string(face.confidence);
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, FaceConfidenceChecker, "face_confidence_check")

} // namespace photo
