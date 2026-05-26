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

        // 局部亮斑检测：连通域分析
        // 分两级——>250 极端亮斑小面积即拦截，>240 一般高光需大面才报警
        cv::Mat bright_250 = (region > 250);
        cv::Mat bright_240 = (region > 240);
        cv::Mat labels, stats, centroids;

        int n_250 = cv::connectedComponentsWithStats(bright_250, labels, stats, centroids, 4);
        int max_spot_250 = 0;
        for (int i = 1; i < n_250; i++) {
            int area = stats.at<int>(i, cv::CC_STAT_AREA);
            if (area > max_spot_250) max_spot_250 = area;
        }

        int n_240 = cv::connectedComponentsWithStats(bright_240, labels, stats, centroids, 4);
        int max_spot_240 = 0;
        for (int i = 1; i < n_240; i++) {
            int area = stats.at<int>(i, cv::CC_STAT_AREA);
            if (area > max_spot_240) max_spot_240 = area;
        }

        constexpr int kSevereSpotMax = 50;   // >250 极端亮斑，50px 即警告
        constexpr int kMildSpotMax   = 400;  // >240 一般高光，400px 才报警

        result.actual_value = ratio_250;
        result.max_threshold = kMaxOverexposedRatio;

        if (ratio_250 > kMaxOverexposedRatio) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "面部有区域性过曝: "
                           + std::to_string(static_cast<int>(ratio_250 * 100)) + "% >250";
        } else if (max_spot_250 > kSevereSpotMax) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "面部存在局部亮斑: 最大"
                           + std::to_string(max_spot_250) + "px (>250)"
                           + " (>250:" + std::to_string(static_cast<int>(ratio_250 * 100)) + "%)";
        } else if (max_spot_240 > kMildSpotMax) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "面部高光区域偏大: 最大"
                           + std::to_string(max_spot_240) + "px (>240)"
                           + " (>250:" + std::to_string(static_cast<int>(ratio_250 * 100)) + "%)";
        } else if (ratio_230 > kMaxBrightSpotsRatio) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = std::string("面部亮区偏多: ")
                           + std::to_string(static_cast<int>(ratio_230 * 100)) + "% pixels > 230"
                           + " (>250:" + std::to_string(static_cast<int>(ratio_250 * 100)) + "%)";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = std::string("面部曝光正常")
                           + " (>250:" + std::to_string(static_cast<int>(ratio_250 * 100))
                           + "% >230:" + std::to_string(static_cast<int>(ratio_230 * 100))
                           + "% 亮斑:" + std::to_string(max_spot_250) + "px)";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, OverexposureChecker, "overexposure_check")

} // namespace photo
