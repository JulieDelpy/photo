#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class ExposureChecker : public IQualityChecker {
public:
    const char* name() const override { return "exposure_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "quality";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "No face detected; cannot check face exposure";
            return result;
        }

        cv::Rect roi = face.bbox & cv::Rect(0, 0, image.cols, image.rows);
        if (roi.area() <= 0) {
            result.passed = false;
            result.severity = Severity::WARNING;
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

        cv::Scalar mean_val = cv::mean(gray);
        double brightness = mean_val.val[0];

        // Calibrated from testset: pass=145-194, underexposed=86, overexposed=204
        constexpr double kExposureMin = 120.0;
        constexpr double kExposureMax = 200.0;

        result.actual_value = brightness;
        result.min_threshold = kExposureMin;
        result.max_threshold = kExposureMax;

        if (brightness < kExposureMin) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Face underexposed: mean brightness = "
                           + std::to_string(static_cast<int>(brightness))
                           + " < " + std::to_string(static_cast<int>(kExposureMin));
        } else if (brightness > kExposureMax) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Face overexposed: mean brightness = "
                           + std::to_string(static_cast<int>(brightness))
                           + " > " + std::to_string(static_cast<int>(kExposureMax));
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Face exposure OK: mean brightness = "
                           + std::to_string(static_cast<int>(brightness));
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, ExposureChecker, "exposure_check")

} // namespace photo
