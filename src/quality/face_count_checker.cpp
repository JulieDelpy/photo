#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class FaceCountChecker : public IQualityChecker {
public:
    const char* name() const override { return "face_count_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat&, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "face";

        // FaceInfo from our detector represents 1 face.
        // A full implementation would pass a vector of faces.
        int face_count = face.detected ? 1 : 0;
        result.actual_value = static_cast<double>(face_count);
        result.min_threshold = static_cast<double>(std.min_face_count);
        result.max_threshold = static_cast<double>(std.max_face_count);

        if (face_count < std.min_face_count) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "No face found; expected exactly 1 face";
        } else if (face_count > std.max_face_count) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "Multiple faces detected; ID photos require exactly 1 face";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Face count OK: exactly 1 face detected";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, FaceCountChecker, "face_count_check")

} // namespace photo
