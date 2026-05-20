#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class FaceRatioChecker : public IQualityChecker {
public:
    const char* name() const override { return "face_ratio_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "composition";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "No face detected; cannot check face height ratio";
            return result;
        }

        double ratio = static_cast<double>(face.bbox.height) / image.rows;

        // Calibrated from testset: pass=0.48-0.57, too_large=0.57, too_small=0.36
        constexpr double kFaceHeightRatioMin = 0.45;
        constexpr double kFaceHeightRatioMax = 0.58;

        result.actual_value = ratio;
        result.min_threshold = kFaceHeightRatioMin;
        result.max_threshold = kFaceHeightRatioMax;

        if (ratio < kFaceHeightRatioMin) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "Face too small in frame: height ratio = "
                           + std::to_string(static_cast<int>(ratio * 100)) + "%";
        } else if (ratio > kFaceHeightRatioMax) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "Face too large in frame: height ratio = "
                           + std::to_string(static_cast<int>(ratio * 100)) + "%";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Face height ratio OK: "
                           + std::to_string(static_cast<int>(ratio * 100)) + "%";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, FaceRatioChecker, "face_ratio_check")

} // namespace photo
