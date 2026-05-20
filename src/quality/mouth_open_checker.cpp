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

        constexpr double kMarTier1 = 0.25;  // definitely open → check oral cavity
        constexpr double kMarTier2 = 0.18;  // borderline   → check dark only
        constexpr int    kInnerMouthVDark  = 80;   // V < 80  → dark oral cavity
        constexpr int    kInnerMouthVBright = 160; // V > 160 → bright teeth

        result.actual_value = face.mar;
        result.max_threshold = kMarTier1;

        if (face.mar > kMarTier2) {
            bool mouth_interior_dark = false;
            bool mouth_interior_bright = false;
            double oral_v = 0;

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
                        oral_v = cv::mean(v_channel).val[0];
                        mouth_interior_dark = (oral_v < kInnerMouthVDark);
                        mouth_interior_bright = (oral_v > kInnerMouthVBright);
                    }
                }
            }

            // Tier 1: definitely open → dark cavity OR bright teeth both fail
            // Tier 2: borderline → only dark cavity fails (avoids false positives on thick lips)
            bool tier1 = (face.mar > kMarTier1);
            bool fail_dark = mouth_interior_dark;
            bool fail_bright = tier1 && mouth_interior_bright;

            if (fail_dark) {
                result.passed = false;
                result.severity = Severity::FAIL;
                result.message = "Mouth open: MAR = "
                               + std::to_string(face.mar).substr(0, 4)
                               + " (oral cavity dark V=" + std::to_string(static_cast<int>(oral_v)) + ")";
            } else if (fail_bright) {
                result.passed = false;
                result.severity = Severity::FAIL;
                result.message = "Teeth showing / exaggerated expression: MAR = "
                               + std::to_string(face.mar).substr(0, 4)
                               + " (oral cavity bright V=" + std::to_string(static_cast<int>(oral_v)) + ")";
            } else {
                result.passed = true;
                result.severity = Severity::PASS;
                result.message = "Mouth state OK: MAR = " + std::to_string(face.mar).substr(0, 4);
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
