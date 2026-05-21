#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class ImageBlurChecker : public IQualityChecker {
public:
    const char* name() const override { return "image_blur_check"; }
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

        // Laplacian variance on full image (vs blur_check which is face-only)
        cv::Mat laplacian;
        cv::Laplacian(gray, laplacian, CV_64F);
        cv::Scalar mean, stddev;
        cv::meanStdDev(laplacian, mean, stddev);
        double var = stddev.val[0] * stddev.val[0];

        result.actual_value = var;
        result.min_threshold = 80.0;

        if (var < 80.0) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Whole image is blurry: Laplacian var = "
                           + std::to_string(static_cast<int>(var));
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Whole image sharpness OK: Laplacian var = "
                           + std::to_string(static_cast<int>(var));
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, ImageBlurChecker, "image_blur_check")

} // namespace photo
