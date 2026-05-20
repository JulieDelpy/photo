#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class SharpnessChecker : public IQualityChecker {
public:
    const char* name() const override { return "sharpness_check"; }
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

        // Tenengrad sharpness measure
        cv::Mat grad_x, grad_y;
        cv::Sobel(gray, grad_x, CV_64F, 1, 0, 3);
        cv::Sobel(gray, grad_y, CV_64F, 0, 1, 3);
        cv::Mat magnitude;
        cv::magnitude(grad_x, grad_y, magnitude);
        cv::Scalar mean_mag = cv::mean(magnitude);

        result.actual_value = mean_mag.val[0];
        result.min_threshold = 5.0;

        if (mean_mag.val[0] < 5.0) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Image appears soft/low sharpness: Tenengrad = "
                           + std::to_string(static_cast<int>(mean_mag.val[0]));
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Image sharpness acceptable: Tenengrad = "
                           + std::to_string(static_cast<int>(mean_mag.val[0]));
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, SharpnessChecker, "sharpness_check")

} // namespace photo
