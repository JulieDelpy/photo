#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>
#include <algorithm>

namespace photo {

class BackgroundUniformityChecker : public IQualityChecker {
public:
    const char* name() const override { return "background_uniformity_check"; }
    const char* version() const override { return "1.1.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "background";

        cv::Mat gray;
        if (image.channels() == 3) {
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = image;
        }

        int w = image.cols;
        int h = image.rows;
        int sw = std::max(w / 8, 15);
        int sh = std::max(h / 8, 15);

        cv::Rect corners[4] = {
            {0, 0, sw, sh}, {w - sw, 0, sw, sh},
            {0, h - sh, sw, sh}, {w - sw, h - sh, sw, sh}
        };

        cv::Rect face_roi = face.detected ? face.bbox : cv::Rect(0,0,0,0);
        std::vector<double> means;

        for (auto& cr : corners) {
            cv::Rect r = cr & cv::Rect(0, 0, w, h);
            if (face.detected && (r & face_roi).area() > r.area() / 3) continue;
            means.push_back(cv::mean(gray(r)).val[0]);
        }

        if (means.size() < 2) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "无法测量背景均匀度";
            return result;
        }

        double sum = 0, sum_sq = 0;
        for (double m : means) { sum += m; sum_sq += m * m; }
        double sd = std::sqrt(sum_sq / means.size() - (sum / means.size()) * (sum / means.size()));

        // 四角最大偏差：捕获渐变/单侧阴影（stddev 对平缓变化不敏感）
        double max_mean = *std::max_element(means.begin(), means.end());
        double min_mean = *std::min_element(means.begin(), means.end());
        double corner_range = max_mean - min_mean;

        constexpr double kBgUniformitySdMax = 80.0;
        constexpr double kCornerRangeWarn = 30.0;

        result.actual_value = std::max(sd, corner_range);
        result.max_threshold = kBgUniformitySdMax;

        // 背景均匀度仅作 WARNING：自然灯光渐变在 pass 图片中也普遍存在
        if (sd > kBgUniformitySdMax) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "背景不均匀: 标准差="
                           + std::to_string(static_cast<int>(sd))
                           + " (四角差=" + std::to_string(static_cast<int>(corner_range)) + ")";
        } else if (corner_range > kCornerRangeWarn) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "背景存在渐变/阴影: 四角差值="
                           + std::to_string(static_cast<int>(corner_range))
                           + " (均值范围 " + std::to_string(static_cast<int>(min_mean))
                           + "~" + std::to_string(static_cast<int>(max_mean)) + ")";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "背景均匀: 标准差="
                           + std::to_string(static_cast<int>(sd))
                           + " 四角差=" + std::to_string(static_cast<int>(corner_range));
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, BackgroundUniformityChecker, "background_uniformity_check")

} // namespace photo
