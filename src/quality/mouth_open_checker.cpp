#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class MouthOpenChecker : public IQualityChecker {
public:
    const char* name() const override { return "mouth_open_check"; }
    const char* version() const override { return "1.4.0"; }

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
        constexpr double kMarLow   = 0.13;   // 低于此 MAR 不检测内口腔（闭嘴状态 landmarks 不准）
        constexpr int    kInnerMouthVDark  = 80;
        constexpr int    kInnerMouthVBright = 160;
        constexpr int    kTeethLowS = 100;   // 牙齿饱和度低（偏白）

        result.actual_value = face.mar;
        result.max_threshold = kMarTier1;

        double oral_v = 0, oral_s = 0;
        double teeth_ratio = 0.0;
        int teeth_pixels = 0;

        if (face.landmarks.size() >= 68) {
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

            std::vector<cv::Point> mouth_pts;
            for (int i = 48; i <= 67; i++)
                mouth_pts.push_back(face.landmarks[i]);
            cv::Rect mouth_rect = cv::boundingRect(mouth_pts);
            float cx = mouth_rect.x + mouth_rect.width * 0.5f;
            float cy = mouth_rect.y + mouth_rect.height * 0.5f;
            float half_w = std::max(mouth_rect.width * 0.58f, face.bbox.width * 0.10f);
            float half_h = std::max(mouth_rect.height * 0.90f, face.bbox.height * 0.035f);
            cv::Rect teeth_rect(static_cast<int>(cx - half_w),
                                static_cast<int>(cy - half_h),
                                static_cast<int>(half_w * 2.0f),
                                static_cast<int>(half_h * 2.0f));
            teeth_rect &= cv::Rect(0, 0, image.cols, image.rows);
            if (teeth_rect.area() > 0) {
                cv::Mat roi = image(teeth_rect);
                cv::Mat hsv;
                cv::cvtColor(roi, hsv, cv::COLOR_BGR2HSV);
                std::vector<cv::Mat> hsv_channels;
                cv::split(hsv, hsv_channels);
                for (int y = 0; y < hsv.rows; ++y) {
                    const uchar* s_row = hsv_channels[1].ptr<uchar>(y);
                    const uchar* v_row = hsv_channels[2].ptr<uchar>(y);
                    for (int x = 0; x < hsv.cols; ++x) {
                        if (v_row[x] > 175 && s_row[x] < 85) {
                            teeth_pixels++;
                        }
                    }
                }
                teeth_ratio = static_cast<double>(teeth_pixels) / teeth_rect.area();
            }
        }

        bool mouth_dark   = (oral_v < kInnerMouthVDark);
        bool mouth_bright = (oral_v > kInnerMouthVBright);
        bool is_teeth     = mouth_bright && (oral_s < kTeethLowS);  // 亮+低饱和=牙齿
        bool is_teeth_hard = mouth_bright && (oral_s < 70) && (oral_v > 180);  // 半张嘴区间更严格
        bool is_low_mar_teeth = (face.mar > 0.045) && (face.mar < kMarLow)
                             && (oral_v > 180) && (oral_s < 85);
        bool tier1 = (face.mar > kMarTier1);
        bool tier2 = (face.mar > kMarTier2);
        result.detail = "oral_v=" + std::to_string(static_cast<int>(oral_v))
                      + " oral_s=" + std::to_string(static_cast<int>(oral_s))
                      + " teeth_px=" + std::to_string(teeth_pixels)
                      + " teeth_ratio=" + std::to_string(teeth_ratio).substr(0, 5)
                      + " low_mar_teeth=" + std::to_string(is_low_mar_teeth ? 1 : 0);

        // 张嘴 → 暗腔/亮牙 都拦截
        // 半张嘴(tier2) → 暗腔拦截 或 强牙齿信号(V>180+S<70)
        // 闭嘴(!tier2) → 仅最强牙齿信号(V>180+S<70)，防止嘴唇反光误报
        bool fail_dark   = tier2 && mouth_dark;
        bool fail_bright = (tier1 && mouth_bright)          // 张嘴+亮区
                        || (tier2 && is_teeth_hard)         // 半张嘴+强牙齿信号
                        || (!tier2 && is_teeth_hard)        // 闭嘴仅保留最强牙齿信号
                        || is_low_mar_teeth;                // 低MAR露齿：牙齿可见但嘴巴张幅很小

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
