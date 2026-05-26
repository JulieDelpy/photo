#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class MouthOpenChecker : public IQualityChecker {
public:
    const char* name() const override { return "mouth_open_check"; }
    const char* version() const override { return "1.2.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "state";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "未检测到人脸，无法检查嘴部状态";
            return result;
        }

        constexpr double kMarTier1 = 0.25;
        constexpr double kMarTier2 = 0.18;
        constexpr double kMarLow   = 0.05;   // 极低 MAR 也检测（闭嘴露牙）
        constexpr int    kInnerMouthVDark  = 80;
        constexpr int    kInnerMouthVBright = 160;
        constexpr int    kTeethLowS = 100;   // 牙齿饱和度低（偏白）

        result.actual_value = face.mar;
        result.max_threshold = kMarTier1;

        double oral_v = 0, oral_s = 0;

        if (face.landmarks.size() >= 68 && face.mar > kMarLow) {
            std::vector<cv::Point> inner_pts;
            for (int i = 61; i <= 67; i++)
                inner_pts.push_back(face.landmarks[i]);
            if (!inner_pts.empty()) {
                cv::Rect inner_rect = cv::boundingRect(inner_pts);
                inner_rect &= cv::Rect(0, 0, image.cols, image.rows);
                if (inner_rect.area() > 0) {
                    cv::Mat roi = image(inner_rect);
                    cv::Mat hsv;
                    cv::cvtColor(roi, hsv, cv::COLOR_BGR2HSV);
                    // V channel (brightness)
                    cv::Mat v_channel;
                    cv::extractChannel(hsv, v_channel, 2);
                    oral_v = cv::mean(v_channel).val[0];
                    // S channel (saturation) — 牙齿低S，嘴唇/牙龈高S
                    std::vector<cv::Mat> hsv_channels;
                    cv::split(hsv, hsv_channels);
                    oral_s = cv::mean(hsv_channels[1]).val[0];
                }
            }
        }

        bool mouth_dark   = (oral_v < kInnerMouthVDark);
        bool mouth_bright = (oral_v > kInnerMouthVBright);
        bool is_teeth = mouth_bright && (oral_s < kTeethLowS);  // 亮+低饱和=牙齿
        bool tier1 = (face.mar > kMarTier1);
        bool tier2 = (face.mar > kMarTier2);

        // 张嘴 → 暗腔/亮牙 都拦截
        // 半张嘴(tier2) → 暗腔拦截
        // is_teeth 条件严格(V>160+S<100)，任意 MAR 均生效，避免厚唇误报
        bool fail_dark   = tier2 && mouth_dark;
        bool fail_bright = (tier1 && mouth_bright)          // 张嘴+亮区
                        || is_teeth;                         // 任意MAR+牙齿特征

        if (fail_dark) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "张嘴: MAR="
                           + std::to_string(face.mar).substr(0, 4)
                           + " 口腔暗区 V=" + std::to_string(static_cast<int>(oral_v));
        } else if (fail_bright) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "露牙: MAR="
                           + std::to_string(face.mar).substr(0, 4)
                           + " 口腔 V=" + std::to_string(static_cast<int>(oral_v))
                           + " S=" + std::to_string(static_cast<int>(oral_s));
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "嘴部状态正常: MAR=" + std::to_string(face.mar).substr(0, 4);
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, MouthOpenChecker, "mouth_open_check")

} // namespace photo
