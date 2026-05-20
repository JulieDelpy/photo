#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class BackgroundUniformityChecker : public IQualityChecker {
public:
    const char* name() const override { return "background_uniformity_check"; }
    const char* version() const override { return "1.0.0"; }

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

        // Sample corner regions avoiding face
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
            result.severity = Severity::FAIL;
            result.message = "Cannot measure background uniformity";
            return result;
        }

        // Compute standard deviation of corner means
        double sum = 0, sum_sq = 0;
        for (double m : means) { sum += m; sum_sq += m * m; }
        double sd = std::sqrt(sum_sq / means.size() - (sum / means.size()) * (sum / means.size()));

        constexpr double kBgUniformityMax = 100.0;

        result.actual_value = sd;
        result.max_threshold = kBgUniformityMax;

        if (sd > kBgUniformityMax) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "Background is not uniform: stddev = "
                           + std::to_string(static_cast<int>(sd));
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Background uniform: stddev = "
                           + std::to_string(static_cast<int>(sd));
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, BackgroundUniformityChecker, "background_uniformity_check")

} // namespace photo
