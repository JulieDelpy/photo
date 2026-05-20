#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class MouthOpenChecker : public IQualityChecker {
public:
    const char* name() const override { return "mouth_open_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "state";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "No face detected; cannot check mouth state";
            return result;
        }

        constexpr double kMarMax = 0.25;
        constexpr int    kInnerMouthVThreshold = 80;  // V < 80 → dark oral cavity

        result.actual_value = face.mar;
        result.max_threshold = kMarMax;

        if (face.mar > kMarMax) {
            // Differentiate thick lips from a genuinely open mouth:
            // sample the inner-oral-cavity region (landmarks 61-67) in
            // HSV V-channel.  Only flag as "open" if the interior is
            // actually dark (oral shadow), not skin-coloured (thick lips).
            bool mouth_interior_dark = false;
            if (face.landmarks.size() >= 68) {
                std::vector<cv::Point> inner_pts;
                for (int i = 61; i <= 67; i++)
                    inner_pts.push_back(face.landmarks[i]);
                if (!inner_pts.empty()) {
                    cv::Rect inner_rect = cv::boundingRect(inner_pts);
                    inner_rect &= cv::Rect(0, 0, image.cols, image.rows);
                    if (inner_rect.area() > 0) {
                        cv::Mat roi = image(inner_rect);
                        cv::Mat hsv, v_channel;
                        cv::cvtColor(roi, hsv, cv::COLOR_BGR2HSV);
                        cv::extractChannel(hsv, v_channel, 2);
                        double mean_v = cv::mean(v_channel).val[0];
                        mouth_interior_dark = (mean_v < kInnerMouthVThreshold);
                    }
                }
            }

            if (mouth_interior_dark) {
                result.passed = false;
                result.severity = Severity::FAIL;
                result.message = "Mouth open: MAR = "
                               + std::to_string(face.mar).substr(0, 4)
                               + " > " + std::to_string(kMarMax)
                               + " (oral cavity dark, genuine open mouth)";
            } else {
                result.passed = true;
                result.severity = Severity::PASS;
                result.message = "Mouth MAR elevated but oral cavity not dark"
                               " (likely thick lips, not open mouth): MAR = "
                               + std::to_string(face.mar).substr(0, 4);
            }
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Mouth closed: MAR = " + std::to_string(face.mar).substr(0, 4);
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, MouthOpenChecker, "mouth_open_check")

} // namespace photo
