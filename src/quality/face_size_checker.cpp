#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <iomanip>
#include <sstream>

namespace photo {

class FaceSizeChecker : public IQualityChecker {
public:
    const char* name() const override { return "face_size_check"; }
    const char* version() const override { return "2.1.0"; }

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
                      + " tolerance=" + std::to_string(kRatioTolerance);

        if (area_too_small) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "人脸过小: 面积比="
                           + pct(area_ratio)
                           + "% < " + pct(kMinAreaRatio)
                           + "% 高度比=" + pct(height_ratio) + "%";
        } else if (area_too_large) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "人脸过大: 面积比="
                           + pct(area_ratio)
                           + "% > " + pct(kMaxAreaRatio)
                           + "% 高度比=" + pct(height_ratio) + "%";
        } else if (height_too_small) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "面部高度比过小: "
                           + pct(height_ratio)
                           + "% < " + pct(kMinHeightRatio)
                           + "% 面积比=" + pct(area_ratio) + "%";
        } else if (height_too_large) {
            result.passed = false;
            result.severity = Severity::FAIL;
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
