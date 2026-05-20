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

        // Check overexposure on face region only
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
        int overexposed_250 = cv::countNonZero(region > 250);
        int overexposed_230 = cv::countNonZero(region > 230);
        double ratio_250 = static_cast<double>(overexposed_250) / total;
        double ratio_230 = static_cast<double>(overexposed_230) / total;

        constexpr double kMaxOverexposedRatio = 0.08;
        constexpr double kMaxBrightSpotsRatio = 0.20;

        result.actual_value = ratio_250;
        result.max_threshold = kMaxOverexposedRatio;

        if (ratio_250 > kMaxOverexposedRatio) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Face has overexposed regions: "
                           + std::to_string(static_cast<int>(ratio_250 * 100)) + "% pixels > 250";
        } else if (ratio_230 > kMaxBrightSpotsRatio) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = std::string("Face has bright spots: ")
                           + std::to_string(static_cast<int>(ratio_230 * 100)) + "% pixels > 230"
                           + " (>250:" + std::to_string(static_cast<int>(ratio_250 * 100)) + "%)";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = std::string("No significant overexposure or bright spots")
                           + " (>250:" + std::to_string(static_cast<int>(ratio_250 * 100))
                           + "% >230:" + std::to_string(static_cast<int>(ratio_230 * 100)) + "%)";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, OverexposureChecker, "overexposure_check")

} // namespace photo
