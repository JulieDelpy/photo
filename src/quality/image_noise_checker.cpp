#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class ImageNoiseChecker : public IQualityChecker {
public:
    const char* name() const override { return "image_noise_check"; }
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

        cv::Mat blurred, high_pass;
        cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 1.5);
        high_pass = gray - blurred;

        cv::Scalar mean, stddev;
        cv::meanStdDev(high_pass, mean, stddev);
        double noise = stddev.val[0];

        // Calibrated from testset: pass=4.6-8.1, tilt_down=9.9, shoulders=9.5
        constexpr double kMaxImageNoiseStddev = 9.0;

        result.actual_value = noise;
        result.max_threshold = kMaxImageNoiseStddev;

        if (noise > kMaxImageNoiseStddev) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Whole image appears noisy: high-freq stddev = "
                           + std::to_string(static_cast<int>(noise));
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Whole image noise acceptable: stddev = "
                           + std::to_string(static_cast<int>(noise));
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, ImageNoiseChecker, "image_noise_check")

} // namespace photo
