#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class BackgroundTextureChecker : public IQualityChecker {
public:
    const char* name() const override { return "background_texture_check"; }
    const char* version() const override { return "1.1.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard&) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "background";

        cv::Mat gray, blurred;
        if (image.channels() == 3) {
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = image;
        }

        // Anti-noise: light GaussianBlur to prevent high-frequency sensor
        // noise from being misidentified as background texture.
        cv::GaussianBlur(gray, blurred, cv::Size(3, 3), 0);

        // LBP-like: compute local binary pattern uniformity
        // Simplified: variance of the gradient magnitude in background regions
        cv::Mat grad_x, grad_y, magnitude;
        cv::Sobel(blurred, grad_x, CV_32F, 1, 0, 1);
        cv::Sobel(blurred, grad_y, CV_32F, 0, 1, 1);
        cv::magnitude(grad_x, grad_y, magnitude);

        // Sample corners avoiding face
        int w = image.cols, h = image.rows;
        int sw = std::max(w / 6, 20), sh = std::max(h / 6, 20);

        cv::Rect corners[4] = {
            {0, 0, sw, sh}, {w - sw, 0, sw, sh},
            {0, h - sh, sw, sh}, {w - sw, h - sh, sw, sh}
        };

        cv::Rect face_roi = face.detected ? face.bbox : cv::Rect(0,0,0,0);
        double texture_sum = 0;
        int count = 0;

        for (auto& cr : corners) {
            cv::Rect r = cr & cv::Rect(0, 0, w, h);
            if (face.detected && (r & face_roi).area() > r.area() / 3) continue;
            texture_sum += cv::mean(magnitude(r)).val[0];
            count++;
        }

        if (count == 0) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Cannot measure background texture (face too large)";
            return result;
        }

        double texture = texture_sum / count;
        result.actual_value = texture;
        constexpr double kBgTextureWarn = 16.0;
        constexpr double kBgTextureFail = 35.0;
        result.max_threshold = kBgTextureWarn;

        if (texture > kBgTextureFail) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "背景存在明显纹理/杂物: 梯度="
                           + std::to_string(static_cast<int>(texture));
        } else if (texture > kBgTextureWarn) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "背景纹理偏明显: 梯度="
                           + std::to_string(static_cast<int>(texture));
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "背景纹理正常: 梯度="
                           + std::to_string(static_cast<int>(texture));
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, BackgroundTextureChecker, "background_texture_check")

} // namespace photo
