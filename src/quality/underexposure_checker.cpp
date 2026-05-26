#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class UnderexposureChecker : public IQualityChecker {
public:
    const char* name() const override { return "underexposure_check"; }
    const char* version() const override { return "1.1.0"; }

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

        int total = gray.rows * gray.cols;
        int underexposed = cv::countNonZero(gray < 10);
        double ratio = static_cast<double>(underexposed) / total;

        // 面部区域平均亮度
        double face_brightness = 128.0;
        if (face.detected) {
            cv::Rect roi = face.bbox & cv::Rect(0, 0, image.cols, image.rows);
            if (roi.area() > 0)
                face_brightness = cv::mean(gray(roi)).val[0];
        }

        constexpr double kMaxUnderexposedRatio = 0.03;
        constexpr double kMinFaceBrightness = 100.0;

        result.actual_value = face_brightness;
        result.max_threshold = kMaxUnderexposedRatio;
        result.min_threshold = kMinFaceBrightness;

        if (ratio > kMaxUnderexposedRatio) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "图像存在欠曝区域: "
                           + std::to_string(static_cast<int>(ratio * 100)) + "% 像素 <10";
        } else if (face_brightness < kMinFaceBrightness) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "面部亮度过低(欠曝): 均值="
                           + std::to_string(static_cast<int>(face_brightness))
                           + " (要求 ≥" + std::to_string(static_cast<int>(kMinFaceBrightness)) + ")";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "曝光正常: 面部亮度="
                           + std::to_string(static_cast<int>(face_brightness));
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, UnderexposureChecker, "underexposure_check")

} // namespace photo
