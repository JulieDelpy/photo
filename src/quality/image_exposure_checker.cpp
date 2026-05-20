#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class ImageExposureChecker : public IQualityChecker {
public:
    const char* name() const override { return "image_exposure_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo&,
                      const IDPhotoStandard&) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "quality";

        cv::Mat gray;
        if (image.channels() == 3) {
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = image;
        }

        double brightness = cv::mean(gray).val[0];
        result.actual_value = brightness;
        result.min_threshold = 80.0;
        result.max_threshold = 220.0;

        if (brightness < 80.0) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Whole image too dark: mean = "
                           + std::to_string(static_cast<int>(brightness));
        } else if (brightness > 220.0) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Whole image too bright: mean = "
                           + std::to_string(static_cast<int>(brightness));
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Whole image exposure OK: mean = "
                           + std::to_string(static_cast<int>(brightness));
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, ImageExposureChecker, "image_exposure_check")

} // namespace photo
