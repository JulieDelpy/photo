#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class ImageContrastChecker : public IQualityChecker {
public:
    const char* name() const override { return "image_contrast_check"; }
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

        cv::Scalar mean, stddev;
        cv::meanStdDev(gray, mean, stddev);
        double sd = stddev.val[0];

        result.actual_value = sd;
        result.min_threshold = 25.0;

        if (sd < 25.0) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Whole image contrast too low: stddev = "
                           + std::to_string(static_cast<int>(sd));
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Whole image contrast OK: stddev = "
                           + std::to_string(static_cast<int>(sd));
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, ImageContrastChecker, "image_contrast_check")

} // namespace photo
