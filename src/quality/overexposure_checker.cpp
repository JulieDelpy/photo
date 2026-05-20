#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class OverexposureChecker : public IQualityChecker {
public:
    const char* name() const override { return "overexposure_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
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

        // Check overexposure on face region (not background, which is intentionally bright)
        cv::Rect roi = face.detected ? face.bbox : cv::Rect(0, 0, image.cols, image.rows);
        roi &= cv::Rect(0, 0, image.cols, image.rows);
        if (roi.area() <= 0) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "No valid region for overexposure check";
            return result;
        }

        cv::Mat region = gray(roi);
        int total = region.rows * region.cols;
        int overexposed = cv::countNonZero(region > 250);
        double ratio = static_cast<double>(overexposed) / total;

        constexpr double kMaxOverexposedRatio = 0.10;

        result.actual_value = ratio;
        result.max_threshold = kMaxOverexposedRatio;

        if (ratio > kMaxOverexposedRatio) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Face has overexposed regions: "
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
