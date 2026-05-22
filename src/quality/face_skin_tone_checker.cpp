#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class FaceSkinToneChecker : public IQualityChecker {
public:
    const char* name() const override { return "face_skin_tone_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard&) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "face";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "未检测到人脸，无法检查肤色";
            return result;
        }

        // 取面部中心 60% 区域，排除头发和背景边缘
        cv::Rect roi = face.bbox;
        int margin_x = roi.width / 5;
        int margin_y = roi.height / 5;
        roi.x += margin_x;
        roi.y += margin_y;
        roi.width -= margin_x * 2;
        roi.height -= margin_y * 2;
        roi &= cv::Rect(0, 0, image.cols, image.rows);

        if (roi.area() <= 0) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "面部区域过小，无法检查肤色";
            return result;
        }

        cv::Mat face_roi = image(roi);
        cv::Mat ycrcb;
        cv::cvtColor(face_roi, ycrcb, cv::COLOR_BGR2YCrCb);

        // 分离 Cr/Cb 通道，肤色在这两个通道上聚拢
        std::vector<cv::Mat> channels;
        cv::split(ycrcb, channels);
        cv::Mat Cr = channels[1];
        cv::Mat Cb = channels[2];

        cv::Scalar cr_mean, cr_std, cb_mean, cb_std;
        cv::meanStdDev(Cr, cr_mean, cr_std);
        cv::meanStdDev(Cb, cb_mean, cb_std);

        double cr = cr_mean.val[0];
        double cb = cb_mean.val[0];
        double cr_sd = cr_std.val[0];
        double cb_sd = cb_std.val[0];

        // 亚洲肤色在 YCrCb 空间的正常范围（经验值）
        constexpr double kCrMin = 135.0, kCrMax = 165.0;
        constexpr double kCbMin = 100.0, kCbMax = 130.0;
        // Cr/Cb 标准差过高说明色彩不均匀
        constexpr double kCrSdMax = 25.0;
        constexpr double kCbSdMax = 20.0;

        result.actual_value = cr;
        result.min_threshold = kCrMin;
        result.max_threshold = kCrMax;

        bool cr_ok = (cr >= kCrMin && cr <= kCrMax);
        bool cb_ok = (cb >= kCbMin && cb <= kCbMax);
        bool cr_sd_ok = (cr_sd <= kCrSdMax);
        bool cb_sd_ok = (cb_sd <= kCbSdMax);

        if (cr_ok && cb_ok && cr_sd_ok && cb_sd_ok) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "肤色正常: Cr=" + std::to_string(static_cast<int>(cr))
                           + " Cb=" + std::to_string(static_cast<int>(cb));
        } else {
            result.passed = false;
            result.severity = Severity::WARNING;
            std::string msg = "面部肤色异常";
            if (!cr_ok) msg += ": Cr=" + std::to_string(static_cast<int>(cr))
                             + " (正常" + std::to_string(static_cast<int>(kCrMin))
                             + "-" + std::to_string(static_cast<int>(kCrMax)) + ")";
            else if (!cb_ok) msg += ": Cb=" + std::to_string(static_cast<int>(cb))
                             + " (正常" + std::to_string(static_cast<int>(kCbMin))
                             + "-" + std::to_string(static_cast<int>(kCbMax)) + ")";
            else msg += ": 色彩不均 CrSD=" + std::to_string(static_cast<int>(cr_sd))
                      + " CbSD=" + std::to_string(static_cast<int>(cb_sd));
            result.message = msg;
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, FaceSkinToneChecker, "face_skin_tone_check")

} // namespace photo
