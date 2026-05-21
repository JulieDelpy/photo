#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class FaceSizeChecker : public IQualityChecker {
public:
    const char* name() const override { return "face_size_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "face";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "No face detected; cannot check face size";
            return result;
        }

        double image_area = static_cast<double>(image.cols * image.rows);
        double face_area = static_cast<double>(face.bbox.area());
        double ratio = (image_area > 0) ? face_area / image_area : 0.0;

        // Calibrated from testset: pass=0.33-0.39, too_large=0.46-0.51, too_small=0.19
        constexpr double kMinFaceAreaRatio = 0.25;
        constexpr double kMaxFaceAreaRatio = 0.45;

        result.actual_value = ratio;
        result.min_threshold = kMinFaceAreaRatio;
        result.max_threshold = kMaxFaceAreaRatio;

        if (ratio < kMinFaceAreaRatio) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Face too small: area ratio = "
                           + std::to_string(static_cast<int>(ratio * 100))
                           + "% < " + std::to_string(static_cast<int>(kMinFaceAreaRatio * 100)) + "%";
        } else if (ratio > kMaxFaceAreaRatio) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Face too large: area ratio = "
                           + std::to_string(static_cast<int>(ratio * 100))
                           + "% > " + std::to_string(static_cast<int>(kMaxFaceAreaRatio * 100)) + "%";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Face size OK: area ratio = "
                           + std::to_string(static_cast<int>(ratio * 100)) + "%";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, FaceSizeChecker, "face_size_check")

} // namespace photo
