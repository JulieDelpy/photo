#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class FaceAspectRatioChecker : public IQualityChecker {
public:
    const char* name() const override { return "face_aspect_ratio_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat&, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "composition";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "No face detected; cannot check face aspect ratio";
            return result;
        }

        double ar = static_cast<double>(face.bbox.width) / face.bbox.height;

        result.actual_value = ar;
        result.min_threshold = static_cast<double>(std.face_aspect_ratio_min);
        result.max_threshold = static_cast<double>(std.face_aspect_ratio_max);

        if (ar < std.face_aspect_ratio_min || ar > std.face_aspect_ratio_max) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Face aspect ratio unusual: "
                           + std::to_string(ar).substr(0,4)
                           + " (expected " + std::to_string(std.face_aspect_ratio_min)
                           + "-" + std::to_string(std.face_aspect_ratio_max) + ")";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Face aspect ratio OK: " + std::to_string(ar).substr(0,4);
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, FaceAspectRatioChecker, "face_aspect_ratio_check")

} // namespace photo
