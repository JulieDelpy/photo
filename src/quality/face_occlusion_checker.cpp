#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class FaceOcclusionChecker : public IQualityChecker {
public:
    const char* name() const override { return "face_occlusion_check"; }
    const char* version() const override { return "1.1.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard&) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "face";

        if (!face.detected || face.landmarks.size() < 27) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "未检测到人脸或特征点不足，无法检查遮挡";
            return result;
        }

        cv::Mat gray;
        if (image.channels() == 3)
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        else
            gray = image;

        cv::Rect bbox = face.bbox;
        cv::Rect skin_roi(bbox.x + bbox.width/4, bbox.y + bbox.height/3,
                          bbox.width/2, bbox.height/3);
        skin_roi &= cv::Rect(0, 0, image.cols, image.rows);
        double skin_brightness = cv::mean(gray(skin_roi))[0];

        auto sampleBand = [&](int start_idx, int offset_px, int band_h) -> double {
            int top_y = image.rows;
            int min_x = image.cols, max_x = 0;
            for (int i = start_idx; i < start_idx + 5; i++) {
                auto& p = face.landmarks[i];
                if (p.y < top_y) top_y = p.y;
                if (p.x < min_x) min_x = p.x;
                if (p.x > max_x) max_x = p.x;
            }
            int sy = top_y - offset_px - band_h;
            if (sy < 0) sy = 0;
            cv::Rect roi(min_x, sy, max_x - min_x, top_y - offset_px - sy);
            roi &= cv::Rect(0, 0, image.cols, image.rows);
            if (roi.area() <= 0) return skin_brightness;
            return cv::mean(gray(roi))[0];
        };

        double left_brow  = sampleBand(17, 8, 10);
        double left_fore  = sampleBand(17, 35, 12);
        double right_brow = sampleBand(22, 8, 10);
        double right_fore = sampleBand(22, 35, 12);

        auto eyelidBrightness = [&](int brow_start, int eye_top_idx) -> double {
            int brow_bottom = 0;
            for (int i = brow_start; i < brow_start + 5; i++) {
                auto& p = face.landmarks[i];
                if (p.y > brow_bottom) brow_bottom = p.y;
            }
            int eye_top_y = static_cast<int>(face.landmarks[eye_top_idx].y);
            int eyelid_top = brow_bottom + (eye_top_y - brow_bottom) / 3;
            int eyelid_bot = brow_bottom + (eye_top_y - brow_bottom) * 2 / 3;
            if (eyelid_top >= eyelid_bot) return skin_brightness;

            int min_x = image.cols, max_x = 0;
            for (int i = brow_start; i < brow_start + 5; i++) {
                auto& p = face.landmarks[i];
                if (p.x < min_x) min_x = p.x;
                if (p.x > max_x) max_x = p.x;
            }
            cv::Rect roi(min_x, eyelid_top, max_x - min_x, eyelid_bot - eyelid_top);
            roi &= cv::Rect(0, 0, image.cols, image.rows);
            if (roi.area() <= 0) return skin_brightness;
            return cv::mean(gray(roi))[0];
        };

        double left_lid  = eyelidBrightness(17, 37);
        double right_lid = eyelidBrightness(22, 43);

        // ---- 眼区边缘密度分析（眼镜镜框/镜片检测） ----
        auto eyeRegionEdgeDensity = [&](int eye_start) -> double {
            int min_x = image.cols, max_x = 0, min_y = image.rows, max_y = 0;
            for (int i = eye_start; i < eye_start + 6; i++) {
                auto& p = face.landmarks[i];
                if (p.x < min_x) min_x = p.x;
                if (p.x > max_x) max_x = p.x;
                if (p.y < min_y) min_y = p.y;
                if (p.y > max_y) max_y = p.y;
            }
            int ew = max_x - min_x, eh = max_y - min_y;
            cv::Rect roi(min_x - ew/4, min_y - eh/4, ew + ew/2, eh + eh/2);
            roi &= cv::Rect(0, 0, image.cols, image.rows);
            if (roi.area() <= 0) return 0;

            cv::Mat eye_roi = gray(roi);
            cv::Mat edges;
            cv::Canny(eye_roi, edges, 50, 150);
            return static_cast<double>(cv::countNonZero(edges)) / roi.area();
        };

        double left_edge  = eyeRegionEdgeDensity(36);
        double right_edge = eyeRegionEdgeDensity(42);
        double avg_edge_density = (left_edge + right_edge) / 2.0;

        double brow_ratio_l = (skin_brightness > 0) ? left_brow / skin_brightness : 1.0;
        double brow_ratio_r = (skin_brightness > 0) ? right_brow / skin_brightness : 1.0;
        double brow_ratio   = std::min(brow_ratio_l, brow_ratio_r);
        double fore_ratio   = (skin_brightness > 0) ? std::min(left_fore, right_fore) / skin_brightness : 1.0;
        double lid_ratio    = (skin_brightness > 0) ? std::min(left_lid, right_lid) / skin_brightness : 1.0;

        double brow_asymmetry = std::abs(brow_ratio_l - brow_ratio_r);
        constexpr double kAsymmetryThreshold = 0.25;
        constexpr double kDarkThreshold = 0.50;
        constexpr double kEdgeDensityThreshold = 0.08;  // 眼区边缘占比 > 8%

        bool brow_dark = (brow_ratio < kDarkThreshold);
        bool lid_dark  = (lid_ratio < kDarkThreshold);
        bool asymmetric = (brow_asymmetry > kAsymmetryThreshold);
        bool high_edge  = (avg_edge_density > kEdgeDensityThreshold);

        result.actual_value = brow_ratio;
        result.max_threshold = kDarkThreshold;

        // 判断逻辑：
        // - 眉区亮 + 眼睑亮 + 低边缘 → 正常 PASS
        // - 眉区亮 + 高边缘密度 → 眼镜/镜框遮挡 WARNING
        // - 眉区亮 + 眼睑暗 → 可能眼镜/眼部遮挡 WARNING
        // - 眉区暗 + 左右不对称 → 单侧头发遮挡 FAIL
        // - 眉区暗 + 眼睑暗 → 头发遮到眼区 FAIL
        // - 眉区暗 + 对称 + 眼睑亮 → 正常刘海 PASS
        if (!brow_dark && !lid_dark && !high_edge) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "面部无遮挡: 眉区亮度="
                           + std::to_string(static_cast<int>(brow_ratio * 100)) + "%";
        } else if (!brow_dark && high_edge) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "眼部区域边缘密集(可能眼镜/镜框): 眉区="
                           + std::to_string(static_cast<int>(brow_ratio * 100))
                           + "% 边缘密度=" + std::to_string(static_cast<int>(avg_edge_density * 100)) + "%";
        } else if (!brow_dark && lid_dark) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "眼部区域偏暗(可能眼镜/镜框遮挡): 眉区="
                           + std::to_string(static_cast<int>(brow_ratio * 100))
                           + "% 眼睑=" + std::to_string(static_cast<int>(lid_ratio * 100)) + "%";
        } else if (asymmetric) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "单侧眉毛被头发遮挡: 左眉="
                           + std::to_string(static_cast<int>(brow_ratio_l * 100))
                           + "% 右眉=" + std::to_string(static_cast<int>(brow_ratio_r * 100))
                           + "% 差异=" + std::to_string(static_cast<int>(brow_asymmetry * 100)) + "%";
        } else if (lid_dark) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "眼部区域被头发遮挡: 眉区="
                           + std::to_string(static_cast<int>(brow_ratio * 100))
                           + "% 眼睑=" + std::to_string(static_cast<int>(lid_ratio * 100)) + "%";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "眉区偏暗(刘海)但眼区可见: 眉区="
                           + std::to_string(static_cast<int>(brow_ratio * 100))
                           + "% 眼睑=" + std::to_string(static_cast<int>(lid_ratio * 100)) + "%";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, FaceOcclusionChecker, "face_occlusion_check")

} // namespace photo
