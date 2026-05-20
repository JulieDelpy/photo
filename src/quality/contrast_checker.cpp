#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class ContrastChecker : public IQualityChecker {
public:
    const char* name() const override { return "contrast_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "quality";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "No face detected; cannot check face contrast";
            return result;
        }

        cv::Rect roi = face.bbox & cv::Rect(0, 0, image.cols, image.rows);
        if (roi.area() <= 0) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "Face bounding box is invalid";
            return result;
        }

        cv::Mat face_roi = image(roi);
        cv::Mat gray;
        if (face_roi.channels() == 3) {
            cv::cvtColor(face_roi, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = face_roi;
        }

        // RMS contrast
        cv::Scalar mean, stddev;
        cv::meanStdDev(gray, mean, stddev);
        double rms_contrast = stddev.val[0];

        // Calibrated from testset: pass=28-65, face_shadow=80 (higher contrast from shadows)
        constexpr double kMinRmsContrast = 25.0;
        constexpr double kMaxRmsContrast = 70.0;

        result.actual_value = rms_contrast;
        result.min_threshold = kMinRmsContrast;
        result.max_threshold = kMaxRmsContrast;

        if (rms_contrast < kMinRmsContrast) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "Face contrast too low: RMS contrast = "
                           + std::to_string(static_cast<int>(rms_contrast))
                           + " < " + std::to_string(static_cast<int>(kMinRmsContrast));
        } else if (rms_contrast > kMaxRmsContrast) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "Face contrast too high (possible shadows): RMS contrast = "
                           + std::to_string(static_cast<int>(rms_contrast))
                           + " > " + std::to_string(static_cast<int>(kMaxRmsContrast));
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Face contrast OK: RMS contrast = "
                           + std::to_string(static_cast<int>(rms_contrast));
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, ContrastChecker, "contrast_check")

} // namespace photo
