#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class FaceSizeChecker : public IQualityChecker {
public:
    const char* name() const override { return "face_size_check"; }
    const char* version() const override { return "2.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard&) override {
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

        constexpr double kMinAreaRatio   = 0.25;
        constexpr double kMaxAreaRatio   = 0.45;
        constexpr double kMinHeightRatio = 0.44;
        constexpr double kMaxHeightRatio = 0.65;

        // primary metric: area ratio; secondary: height ratio
        bool area_too_small   = (area_ratio < kMinAreaRatio);
        bool area_too_large   = (area_ratio > kMaxAreaRatio);
        bool height_too_small = (height_ratio < kMinHeightRatio);
        bool height_too_large = (height_ratio > kMaxHeightRatio);

        result.actual_value = area_ratio;
        result.min_threshold = kMinAreaRatio;
        result.max_threshold = kMaxAreaRatio;

        if (area_too_small) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "人脸过小: 面积比="
                           + std::to_string(static_cast<int>(area_ratio * 100))
                           + "% < " + std::to_string(static_cast<int>(kMinAreaRatio * 100))
                           + "% 高度比=" + std::to_string(static_cast<int>(height_ratio * 100)) + "%";
        } else if (area_too_large) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "人脸过大: 面积比="
                           + std::to_string(static_cast<int>(area_ratio * 100))
                           + "% > " + std::to_string(static_cast<int>(kMaxAreaRatio * 100))
                           + "% 高度比=" + std::to_string(static_cast<int>(height_ratio * 100)) + "%";
        } else if (height_too_small) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "面部高度比过小: "
                           + std::to_string(static_cast<int>(height_ratio * 100))
                           + "% < " + std::to_string(static_cast<int>(kMinHeightRatio * 100))
                           + "% 面积比=" + std::to_string(static_cast<int>(area_ratio * 100)) + "%";
        } else if (height_too_large) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "面部高度比过大: "
                           + std::to_string(static_cast<int>(height_ratio * 100))
                           + "% > " + std::to_string(static_cast<int>(kMaxHeightRatio * 100))
                           + "% 面积比=" + std::to_string(static_cast<int>(area_ratio * 100)) + "%";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "人脸尺寸正常: 面积比="
                           + std::to_string(static_cast<int>(area_ratio * 100))
                           + "% 高度比=" + std::to_string(static_cast<int>(height_ratio * 100)) + "%";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, FaceSizeChecker, "face_size_check")

} // namespace photo
