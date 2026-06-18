#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class FaceSkinToneChecker : public IQualityChecker {
public:
    const char* name() const override { return "face_skin_tone_check"; }
    const char* version() const override { return "1.1.0"; }

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

        cv::Mat ycrcb;
        cv::cvtColor(image(roi), ycrcb, cv::COLOR_BGR2YCrCb);

        std::vector<cv::Mat> channels;
        cv::split(ycrcb, channels);
        cv::Mat cr_channel = channels[1];
        cv::Mat cb_channel = channels[2];

        cv::Scalar cr_mean, cr_std, cb_mean, cb_std;
        cv::meanStdDev(cr_channel, cr_mean, cr_std);
        cv::meanStdDev(cb_channel, cb_mean, cb_std);

        double cr = cr_mean.val[0];
        double cb = cb_mean.val[0];
        double cr_sd = cr_std.val[0];
        double cb_sd = cb_std.val[0];
        double cr_cb_ratio = (cb > 0.0) ? cr / cb : 0.0;

        constexpr double kCrMin = 135.0;
        constexpr double kCrMax = 165.0;
        constexpr double kCbMin = 105.0;
        constexpr double kCbMax = 130.0;
        constexpr double kCrCbRatioWarnMax = 1.45;
        constexpr double kCrCbRatioFailMax = 1.60;
        constexpr double kCrSdWarnMax = 25.0;
        constexpr double kCbSdWarnMax = 20.0;
        constexpr double kCrSdFailMax = 35.0;
        constexpr double kCbSdFailMax = 30.0;

        bool cr_ok = (cr >= kCrMin && cr <= kCrMax);
        bool cb_ok = (cb >= kCbMin && cb <= kCbMax);
        bool ratio_ok = (cr_cb_ratio <= kCrCbRatioWarnMax);
        bool cr_sd_ok = (cr_sd <= kCrSdWarnMax);
        bool cb_sd_ok = (cb_sd <= kCbSdWarnMax);
        bool severe = (cr < kCrMin - 10.0) || (cr > kCrMax + 10.0)
                   || (cb < kCbMin - 10.0) || (cb > kCbMax + 10.0)
                   || (cr_cb_ratio > kCrCbRatioFailMax)
                   || (cr_sd > kCrSdFailMax)
                   || (cb_sd > kCbSdFailMax);

        result.actual_value = cr;
        result.min_threshold = kCrMin;
        result.max_threshold = kCrMax;
        result.detail = "Cr=" + std::to_string(static_cast<int>(cr))
                      + " Cb=" + std::to_string(static_cast<int>(cb))
                      + " CrCb=" + std::to_string(cr_cb_ratio).substr(0, 4)
                      + " CrSD=" + std::to_string(static_cast<int>(cr_sd))
                      + " CbSD=" + std::to_string(static_cast<int>(cb_sd));

        if (cr_ok && cb_ok && ratio_ok && cr_sd_ok && cb_sd_ok) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "肤色正常: Cr=" + std::to_string(static_cast<int>(cr))
                           + " Cb=" + std::to_string(static_cast<int>(cb));
        } else {
            result.passed = false;
            result.severity = severe ? Severity::FAIL : Severity::WARNING;
            std::string prefix = severe ? "面部肤色异常" : "面部肤色边缘";
            if (!ratio_ok) {
                result.message = prefix + ": 偏暖/偏红 Cr/Cb="
                               + std::to_string(static_cast<int>(cr_cb_ratio * 100))
                               + "%";
            } else if (!cb_ok) {
                result.message = prefix + ": Cb=" + std::to_string(static_cast<int>(cb));
            } else if (!cr_ok) {
                result.message = prefix + ": Cr=" + std::to_string(static_cast<int>(cr));
            } else {
                result.message = prefix + ": CrSD=" + std::to_string(static_cast<int>(cr_sd))
                               + " CbSD=" + std::to_string(static_cast<int>(cb_sd));
            }
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, FaceSkinToneChecker, "face_skin_tone_check")

} // namespace photo
