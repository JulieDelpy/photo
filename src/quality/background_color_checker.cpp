#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class BackgroundColorChecker : public IQualityChecker {
public:
    const char* name() const override { return "background_color_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "background";

        // Sample background from four corners and edges (avoiding face region)
        cv::Rect face_roi = face.detected ? face.bbox : cv::Rect(0,0,0,0);

        // Sample 8 regions (4 corners + 4 edge midpoints)
        int w = image.cols;
        int h_img = image.rows;
        int sw = w / 10;
        int sh = h_img / 10;
        sw = std::max(sw, 10);
        sh = std::max(sh, 10);

        struct Region { int x, y; };
        Region regions[] = {
            {0, 0}, {w - sw, 0}, {0, h_img - sh}, {w - sw, h_img - sh},
            {w/2 - sw/2, 0}, {w/2 - sw/2, h_img - sh},
            {0, h_img/2 - sh/2}, {w - sw, h_img/2 - sh/2}
        };

        // 同时采集 HSV 和 LAB，分别用于背景色检测和偏色检测
        cv::Mat hsv, lab;
        cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
        cv::cvtColor(image, lab, cv::COLOR_BGR2Lab);

        double h_sum = 0, s_sum = 0, v_sum = 0;
        double a_sum = 0, b_sum = 0;
        double bgr_b_sum = 0, bgr_g_sum = 0, bgr_r_sum = 0;
        int valid_regions = 0;

        for (auto& r : regions) {
            cv::Rect sample_rect(r.x, r.y, sw, sh);
            sample_rect &= cv::Rect(0, 0, w, h_img);

            // Skip if overlaps with face
            if (face.detected && (sample_rect & face_roi).area() > sample_rect.area() / 3) {
                continue;
            }

            cv::Scalar mean_hsv = cv::mean(hsv(sample_rect));
            cv::Scalar mean_lab = cv::mean(lab(sample_rect));
            cv::Scalar mean_bgr = cv::mean(image(sample_rect));
            h_sum += mean_hsv[0];
            s_sum += mean_hsv[1];
            v_sum += mean_hsv[2];
            a_sum += mean_lab[1];
            b_sum += mean_lab[2];
            bgr_b_sum += mean_bgr[0];
            bgr_g_sum += mean_bgr[1];
            bgr_r_sum += mean_bgr[2];
            valid_regions++;
        }

        if (valid_regions == 0) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "无法采样背景：人脸覆盖整个画面";
            return result;
        }

        double s_mean = s_sum / valid_regions;
        double v_mean = v_sum / valid_regions;
        double a_mean = a_sum / valid_regions;  // LAB A (0-255, 128=中性)
        double b_lab_mean = b_sum / valid_regions;  // LAB B (0-255, 128=中性)
        double bgr_b_mean = bgr_b_sum / valid_regions;
        double bgr_g_mean = bgr_g_sum / valid_regions;
        double bgr_r_mean = bgr_r_sum / valid_regions;

        // HSV 背景色阈值
        constexpr double kBgExpectedSMax = 85.0;
        constexpr double kBgExpectedVMin = 150.0;

        result.actual_value = s_mean;
        result.max_threshold = kBgExpectedSMax;

        bool s_ok = (s_mean <= kBgExpectedSMax);
        bool v_ok = (v_mean >= kBgExpectedVMin);

        // LAB 偏色检测：仅对低饱和度背景（白/灰底）做中性灰检查
        // 高饱和度背景（蓝底、红底等）不检查 LAB 中性，仅检查 S/V 范围
        double a_dev = a_mean - 128.0;
        double b_dev = b_lab_mean - 128.0;
        double color_cast_dist = std::sqrt(a_dev * a_dev + b_dev * b_dev);
        constexpr double kMaxColorCastDist = 10.0;   // LAB 中性偏离距离 > 10 = 偏色
        constexpr double kColorBgSMin = 60.0;         // S > 60 视为有彩色背景，跳过偏色检测

        // RGB 白平衡作为辅助参考
        double rgb_max = std::max({bgr_r_mean, bgr_g_mean, bgr_b_mean});
        double rgb_min = std::min({bgr_r_mean, bgr_g_mean, bgr_b_mean});
        double rgb_spread = (rgb_max > 0) ? (rgb_max - rgb_min) / rgb_max : 0;

        // S > kColorBgSMin 说明是有意为之的彩色背景（如蓝底），不检查偏色
        bool cc_ok = (s_mean > kColorBgSMin) || (color_cast_dist <= kMaxColorCastDist);

        if (s_ok && v_ok && cc_ok) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "背景颜色正常: S=" + std::to_string(static_cast<int>(s_mean))
                           + " V=" + std::to_string(static_cast<int>(v_mean))
                           + " 偏色距=" + std::to_string(static_cast<int>(color_cast_dist));
        } else if (!cc_ok) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "背景存在偏色: S=" + std::to_string(static_cast<int>(s_mean))
                           + " LAB A=" + std::to_string(static_cast<int>(a_mean))
                           + " B=" + std::to_string(static_cast<int>(b_lab_mean))
                           + " 偏色距=" + std::to_string(static_cast<int>(color_cast_dist))
                           + " RGB偏差=" + std::to_string(static_cast<int>(rgb_spread * 100)) + "%";
        } else {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "背景颜色不符合要求: S=" + std::to_string(static_cast<int>(s_mean))
                           + " V=" + std::to_string(static_cast<int>(v_mean));
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, BackgroundColorChecker, "background_color_check")

} // namespace photo
