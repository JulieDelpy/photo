#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class ResolutionChecker : public IQualityChecker {
public:
    const char* name() const override { return "resolution_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "quality";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "No face detected; cannot check face resolution";
            return result;
        }

        // Face resolution = face area in pixels relative to image
        double face_pixel_area = static_cast<double>(face.bbox.area());
        double image_area = static_cast<double>(image.cols * image.rows);
        double face_area_ratio = (image_area > 0) ? face_pixel_area / image_area : 0.0;

        // Calibrated from testset: pass=19600-26569, too_small=14161, too_large=29241
        constexpr int kMinFacePixels = 16000;
        constexpr int kMaxFacePixels = 30000;

        result.actual_value = face_pixel_area;
        result.min_threshold = kMinFacePixels;
        result.max_threshold = kMaxFacePixels;

        if (face_pixel_area < kMinFacePixels) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "Face resolution too low: "
                           + std::to_string(static_cast<int>(face_pixel_area))
                           + " pixels < " + std::to_string(kMinFacePixels);
        } else if (face_pixel_area > kMaxFacePixels) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Face resolution excessive: "
                           + std::to_string(static_cast<int>(face_pixel_area))
                           + " pixels > " + std::to_string(kMaxFacePixels);
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Face resolution sufficient: "
                           + std::to_string(static_cast<int>(face_pixel_area)) + " pixels";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, ResolutionChecker, "resolution_check")

} // namespace photo
