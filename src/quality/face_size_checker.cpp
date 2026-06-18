#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace photo {

class FaceSizeChecker : public IQualityChecker {
public:
    const char* name() const override { return "face_size_check"; }
    const char* version() const override { return "2.2.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& cfg) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "face";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "未检测到人脸，无法检查尺寸";
            return result;
        }

        double image_area = static_cast<double>(image.cols * image.rows);
        double face_area = static_cast<double>(face.bbox.area());
        double area_ratio = (image_area > 0) ? face_area / image_area : 0.0;
        double height_ratio = static_cast<double>(face.bbox.height) / image.rows;
        const double image_aspect = image.rows > 0 ? static_cast<double>(image.cols) / image.rows : 0.0;
        const double expected_aspect = cfg.expected_height > 0
            ? static_cast<double>(cfg.expected_width) / cfg.expected_height : 0.0;
        const bool final_sized_image = cfg.expected_width > 0 && cfg.expected_height > 0
            && image.cols == cfg.expected_width && image.rows == cfg.expected_height;
        const bool final_aspect_image = expected_aspect > 0.0
            && std::abs(image_aspect - expected_aspect) <= 0.02;

        const double kMinAreaRatio = cfg.min_face_area_ratio > 0 ? cfg.min_face_area_ratio : 0.25;
        const double kMaxAreaRatio = cfg.max_face_area_ratio > 0 ? cfg.max_face_area_ratio : 0.45;
        const double kMinHeightRatio = cfg.face_height_ratio_min > 0 ? cfg.face_height_ratio_min : 0.44;
        const double kMaxHeightRatio = cfg.face_height_ratio_max > 0 ? cfg.face_height_ratio_max : 0.65;
        constexpr double kRatioTolerance = 0.005;

        // primary metric: area ratio; secondary: height ratio
        bool area_too_small   = (area_ratio < kMinAreaRatio - kRatioTolerance);
        bool area_too_large   = (area_ratio > kMaxAreaRatio + kRatioTolerance);
        bool height_too_small = (height_ratio < kMinHeightRatio - kRatioTolerance);
        bool height_too_large = (height_ratio > kMaxHeightRatio + kRatioTolerance);

        result.actual_value = area_ratio;
        result.min_threshold = kMinAreaRatio;
        result.max_threshold = kMaxAreaRatio;
        result.detail = "area_ratio=" + std::to_string(area_ratio)
                      + " height_ratio=" + std::to_string(height_ratio)
                      + " tolerance=" + std::to_string(kRatioTolerance)
                      + " final_sized=" + std::to_string(final_sized_image ? 1 : 0)
                      + " final_aspect=" + std::to_string(final_aspect_image ? 1 : 0);

        if (area_too_small) {
            result.passed = false;
            result.severity = final_aspect_image ? Severity::FAIL : Severity::WARNING;
            result.message = "人脸过小: 面积比="
                           + pct(area_ratio)
                           + "% < " + pct(kMinAreaRatio)
                           + "% 高度比=" + pct(height_ratio) + "%";
        } else if (area_too_large) {
            result.passed = false;
            result.severity = final_aspect_image ? Severity::FAIL : Severity::WARNING;
            result.message = "人脸过大: 面积比="
                           + pct(area_ratio)
                           + "% > " + pct(kMaxAreaRatio)
                           + "% 高度比=" + pct(height_ratio) + "%";
        } else if (height_too_small) {
            result.passed = false;
            result.severity = final_aspect_image ? Severity::FAIL : Severity::WARNING;
            result.message = "面部高度比过小: "
                           + pct(height_ratio)
                           + "% < " + pct(kMinHeightRatio)
                           + "% 面积比=" + pct(area_ratio) + "%";
        } else if (height_too_large) {
            result.passed = false;
            result.severity = final_aspect_image ? Severity::FAIL : Severity::WARNING;
            result.message = "面部高度比过大: "
                           + pct(height_ratio)
                           + "% > " + pct(kMaxHeightRatio)
                           + "% 面积比=" + pct(area_ratio) + "%";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "人脸尺寸正常: 面积比="
                           + pct(area_ratio)
                           + "% 高度比=" + pct(height_ratio) + "%";
        }
        return result;
    }

private:
    static std::string pct(double ratio) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << ratio * 100.0;
        return ss.str();
    }
};

REGISTER_PLUGIN(IQualityChecker, FaceSizeChecker, "face_size_check")

} // namespace photo
