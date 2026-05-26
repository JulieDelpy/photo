#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <iomanip>

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

        // HSV 背景色阈值（来自配置）
        int s_max = std.bg_expected_s_max > 0 ? std.bg_expected_s_max : 85;
        int v_min = std.bg_expected_v_min > 0 ? std.bg_expected_v_min : 150;

        bool s_ok = (s_mean <= s_max);
        bool v_ok = (v_mean >= v_min);

        // LAB 偏色检测：分级阈值
        // 低饱和度（白/灰底）→ 严格；中饱和度 → 中等；高饱和度（蓝/红底）→ 宽松
        double a_dev = a_mean - 128.0;
        double b_dev = b_lab_mean - 128.0;
        double color_cast_dist = std::sqrt(a_dev * a_dev + b_dev * b_dev);

        double max_cc_dist;
        if (s_mean <= 30)       max_cc_dist = 8.0;   // 纯白/灰底，允许轻微自然偏差
        else if (s_mean <= 60)  max_cc_dist = 13.0;  // 浅色底，允许正常灯光暖色调
        else                    max_cc_dist = 15.0;  // 彩色底（蓝/红），只抓极端偏色

        bool cc_ok = (color_cast_dist <= max_cc_dist);

        // 将偏色距和阈值暴露给前端（保留一位小数避免取整误差）
        auto fmt1 = [](double v) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(1) << v;
            return ss.str();
        };
        result.actual_value = color_cast_dist;
        result.max_threshold = max_cc_dist;

        if (s_ok && v_ok && cc_ok) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "背景颜色正常: S=" + std::to_string(static_cast<int>(s_mean))
                           + " V=" + std::to_string(static_cast<int>(v_mean))
                           + " 偏色距=" + fmt1(color_cast_dist);
        } else if (!cc_ok) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "背景存在偏色: S=" + std::to_string(static_cast<int>(s_mean))
                           + " LAB A=" + std::to_string(static_cast<int>(a_mean))
                           + " B=" + std::to_string(static_cast<int>(b_lab_mean))
                           + " 偏色距=" + fmt1(color_cast_dist)
                           + " (阈值≤" + fmt1(max_cc_dist) + ")";
        } else if (!s_ok) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "背景饱和度过高: S=" + std::to_string(static_cast<int>(s_mean))
                           + " (阈值≤" + std::to_string(s_max) + ")"
                           + " V=" + std::to_string(static_cast<int>(v_mean));
        } else {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "背景亮度过低: V=" + std::to_string(static_cast<int>(v_mean))
                           + " (阈值≥" + std::to_string(v_min) + ")"
                           + " S=" + std::to_string(static_cast<int>(s_mean));
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, BackgroundColorChecker, "background_color_check")

} // namespace photo
