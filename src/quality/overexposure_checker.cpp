#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class OverexposureChecker : public IQualityChecker {
public:
    const char* name() const override { return "overexposure_check"; }
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

        // Count pixels above 250 (overexposed)
        int total = gray.rows * gray.cols;
        int overexposed = cv::countNonZero(gray > 250);
        double ratio = static_cast<double>(overexposed) / total;

        result.actual_value = ratio;
        result.max_threshold = 0.05; // 5% overexposed pixels max

        if (ratio > 0.05) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Image has overexposed regions: "
                           + std::to_string(static_cast<int>(ratio * 100)) + "% pixels";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "No significant overexposure detected";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, OverexposureChecker, "overexposure_check")

} // namespace photo
