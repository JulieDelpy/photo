#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>
#include <algorithm>

namespace photo {

class EyeClosureChecker : public IQualityChecker {
public:
    const char* name() const override { return "eye_closure_check"; }
    const char* version() const override { return "1.3.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard&) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "state";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "未检测到人脸，无法检查眼睛状态";
            return result;
        }

        double ear = std::min(face.ear_left, face.ear_right);
        EyeTexture texture = computeEyeTexture(image, face);
        double eye_texture_var = texture.min_var;

        constexpr double kEarMin = 0.20;
        constexpr double kAsymmetricEarMax = 0.45;
        constexpr double kTextureVarMin = 400.0;
        constexpr double kSingleEyeClosedVarMax = 320.0;
        constexpr double kOtherEyeTextureMin = 800.0;

        bool ear_low = (ear < kEarMin);
        bool texture_low = (eye_texture_var < kTextureVarMin);
        bool asymmetric_closed = (texture.min_var < kSingleEyeClosedVarMax)
                              && (texture.max_var > kOtherEyeTextureMin);
        bool asymmetric_fail = asymmetric_closed && (ear < kAsymmetricEarMax);

        result.actual_value = eye_texture_var;
        result.min_threshold = kTextureVarMin;
        result.detail = "EAR=" + std::to_string(ear).substr(0, 5)
                      + " left_var=" + std::to_string(static_cast<int>(texture.left_var))
                      + " right_var=" + std::to_string(static_cast<int>(texture.right_var))
                      + " asymmetric_closed=" + std::to_string(asymmetric_closed ? 1 : 0)
                      + " asymmetric_fail=" + std::to_string(asymmetric_fail ? 1 : 0);

        if ((texture_low && ear_low) || asymmetric_fail) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "疑似闭眼: 眼区纹理方差="
                           + std::to_string(static_cast<int>(eye_texture_var))
                           + ", EAR=" + std::to_string(ear).substr(0, 4);
        } else if (texture_low || asymmetric_closed) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "眼区纹理偏弱: 方差="
                           + std::to_string(static_cast<int>(eye_texture_var))
                           + ", EAR=" + std::to_string(ear).substr(0, 4);
        } else if (ear_low) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "眼裂偏窄: EAR=" + std::to_string(ear).substr(0, 4);
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
    struct EyeTexture {
        double left_var = 100.0;
        double right_var = 100.0;
        double min_var = 100.0;
        double max_var = 100.0;
    };

    EyeTexture computeEyeTexture(const cv::Mat& image, const FaceInfo& face) const {
        EyeTexture texture;
        if (face.landmarks.size() < 48) return texture;

        cv::Mat gray;
        if (image.channels() == 3) {
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = image;
        }

        texture.left_var = eyeRegionVariance(gray, face.landmarks, 36);
        texture.right_var = eyeRegionVariance(gray, face.landmarks, 42);
        texture.min_var = std::min(texture.left_var, texture.right_var);
        texture.max_var = std::max(texture.left_var, texture.right_var);
        return texture;
    }

    double eyeRegionVariance(const cv::Mat& gray,
                             const std::vector<cv::Point2f>& lm,
                             int es) const {
        if (es + 6 > static_cast<int>(lm.size())) return 100.0;

        float min_x = lm[es].x;
        float max_x = lm[es].x;
        float min_y = lm[es].y;
        float max_y = lm[es].y;
        for (int i = 1; i < 6; i++) {
            min_x = std::min(min_x, lm[es + i].x);
            max_x = std::max(max_x, lm[es + i].x);
            min_y = std::min(min_y, lm[es + i].y);
            max_y = std::max(max_y, lm[es + i].y);
        }

        float cx = (min_x + max_x) / 2.0f;
        float cy = (min_y + max_y) / 2.0f;
        float half_w = (max_x - min_x) * 0.6f;
        float half_h = (max_y - min_y) * 0.6f;

        int x0 = std::max(0, static_cast<int>(cx - half_w));
        int y0 = std::max(0, static_cast<int>(cy - half_h));
        int x1 = std::min(gray.cols - 1, static_cast<int>(cx + half_w));
        int y1 = std::min(gray.rows - 1, static_cast<int>(cy + half_h));
        if (x1 <= x0 || y1 <= y0) return 100.0;

        cv::Mat eye_patch = gray(cv::Rect(x0, y0, x1 - x0, y1 - y0));
        cv::Scalar mean;
        cv::Scalar stddev;
        cv::meanStdDev(eye_patch, mean, stddev);
        return stddev.val[0] * stddev.val[0];
    }
};

REGISTER_PLUGIN(IQualityChecker, EyeClosureChecker, "eye_closure_check")

} // namespace photo
