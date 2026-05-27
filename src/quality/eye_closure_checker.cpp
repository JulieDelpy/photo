#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

namespace photo {

class EyeClosureChecker : public IQualityChecker {
public:
    const char* name() const override { return "eye_closure_check"; }
    const char* version() const override { return "1.1.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "state";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "未检测到人脸，无法检查眼睛状态";
            return result;
        }

        // EAR 作为基础参考
        double ear = std::min(face.ear_left, face.ear_right);

        // 像素级眼部纹理方差：睁眼时虹膜/瞳孔/巩膜边界产生高方差，
        // 闭眼时只有均匀皮肤色，方差极低。
        double eye_texture_var = computeEyeTextureVariance(image, face);

        constexpr double kEarMin        = 0.20;
        constexpr double kTextureVarMin = 400.0;  // 眼区灰度方差最低阈值（闭眼273 vs 正常736-3600）

        result.actual_value = eye_texture_var;
        result.min_threshold = kTextureVarMin;
        // 附带 EAR 到 detail 字段便于调试
        result.detail = "EAR=" + std::to_string(ear).substr(0, 5);

        // 两级判定：纹理方差为主（像素级），EAR 为辅（几何级）
        // 只有两者同时低才判 FAIL（双眼信号一致），仅纹理低为 WARNING（避免小眼/眼镜/大图误报）
        bool earLow     = (ear < kEarMin);
        bool textureLow = (eye_texture_var < kTextureVarMin);

        if (textureLow && earLow) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "疑似闭眼: 眼区纹理方差="
                           + std::to_string(static_cast<int>(eye_texture_var))
                           + " (阈值" + std::to_string(static_cast<int>(kTextureVarMin)) + ")"
                           + ", EAR=" + std::to_string(ear).substr(0, 4);
        } else if (textureLow && !earLow) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "眼区纹理偏弱: 方差="
                           + std::to_string(static_cast<int>(eye_texture_var))
                           + " (阈值" + std::to_string(static_cast<int>(kTextureVarMin)) + ")"
                           + ", EAR=" + std::to_string(ear).substr(0, 4) + "(正常)";
        } else if (earLow && !textureLow) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "眼裂偏窄: EAR=" + std::to_string(ear).substr(0, 4)
                           + " (阈值" + std::to_string(kEarMin).substr(0, 4) + ")";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "双眼睁开正常: 纹理方差="
                           + std::to_string(static_cast<int>(eye_texture_var))
                           + ", EAR=" + std::to_string(ear).substr(0, 4);
        }
        return result;
    }

private:
    // 基于眼部区域的像素纹理方差评估眼睛是否睁开
    double computeEyeTextureVariance(const cv::Mat& image, const FaceInfo& face) const {
        if (face.landmarks.size() < 48) return 100.0;  // 无眼部关键点，默认正常

        cv::Mat gray;
        if (image.channels() == 3)
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        else
            gray = image;

        double var_left  = eyeRegionVariance(gray, face.landmarks, 36);
        double var_right = eyeRegionVariance(gray, face.landmarks, 42);

        return std::min(var_left, var_right);
    }

    double eyeRegionVariance(const cv::Mat& gray,
                             const std::vector<cv::Point2f>& lm, int es) const {
        if (es + 6 > static_cast<int>(lm.size())) return 100.0;

        // 计算眼部 6 个关键点的包围盒
        float minX = lm[es].x, maxX = lm[es].x;
        float minY = lm[es].y, maxY = lm[es].y;
        for (int i = 1; i < 6; i++) {
            minX = std::min(minX, lm[es+i].x);
            maxX = std::max(maxX, lm[es+i].x);
            minY = std::min(minY, lm[es+i].y);
            maxY = std::max(maxY, lm[es+i].y);
        }

        // 扩展 20% 以包含眼周纹理（虹膜/巩膜边界）
        float cx = (minX + maxX) / 2.0f;
        float cy = (minY + maxY) / 2.0f;
        float halfW = (maxX - minX) * 0.6f;
        float halfH = (maxY - minY) * 0.6f;

        int x0 = std::max(0, static_cast<int>(cx - halfW));
        int y0 = std::max(0, static_cast<int>(cy - halfH));
        int x1 = std::min(gray.cols - 1, static_cast<int>(cx + halfW));
        int y1 = std::min(gray.rows - 1, static_cast<int>(cy + halfH));

        if (x1 <= x0 || y1 <= y0) return 100.0;

        cv::Rect roi(x0, y0, x1 - x0, y1 - y0);
        cv::Mat eyePatch = gray(roi);

        // 计算眼区的灰度标准差 → 纹理强度
        cv::Scalar mean, stddev;
        cv::meanStdDev(eyePatch, mean, stddev);
        double variance = stddev.val[0] * stddev.val[0];

        return variance;
    }
};

REGISTER_PLUGIN(IQualityChecker, EyeClosureChecker, "eye_closure_check")

} // namespace photo
