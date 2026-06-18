#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class ResolutionChecker : public IQualityChecker {
public:
    const char* name() const override { return "resolution_check"; }
    const char* version() const override { return "1.1.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "quality";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "No face detected; cannot check face resolution";
            return result;
        }

        // Face resolution = face bbox area in pixels.
        double face_pixel_area = static_cast<double>(face.bbox.area());
        double image_area = static_cast<double>(image.cols * image.rows);
        double face_area_ratio = (image_area > 0) ? face_pixel_area / image_area : 0.0;

        int kMinFacePixels = std.min_face_pixels > 0 ? std.min_face_pixels : 16000;
        int kMaxFacePixels = std.max_face_pixels > 0 ? std.max_face_pixels : 30000;

        result.actual_value = face_pixel_area;
        result.min_threshold = kMinFacePixels;
        result.max_threshold = kMaxFacePixels;
        result.detail = "face_area_ratio=" + std::to_string(face_area_ratio)
                      + " bbox=" + std::to_string(face.bbox.width)
                      + "x" + std::to_string(face.bbox.height);

        if (face_pixel_area < kMinFacePixels) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "人脸分辨率不足: "
                           + std::to_string(static_cast<int>(face_pixel_area))
                           + " pixels < " + std::to_string(kMinFacePixels);
        } else if (face_pixel_area > kMaxFacePixels) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "人脸分辨率过高/裁切过近: "
                           + std::to_string(static_cast<int>(face_pixel_area))
                           + " pixels > " + std::to_string(kMaxFacePixels);
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "人脸分辨率正常: "
                           + std::to_string(static_cast<int>(face_pixel_area)) + " pixels";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, ResolutionChecker, "resolution_check")

} // namespace photo
