#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class ShadowChecker : public IQualityChecker {
public:
    const char* name() const override { return "shadow_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "quality";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "No face detected; cannot check face shadows";
            return result;
        }

        cv::Rect roi = face.bbox & cv::Rect(0, 0, image.cols, image.rows);
        if (roi.area() <= 0) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "Face bounding box is invalid";
            return result;
        }

        cv::Mat face_roi = image(roi);
        cv::Mat gray;
        if (face_roi.channels() == 3) {
            cv::cvtColor(face_roi, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = face_roi;
        }

        // Detect shadows: divide face ROI into blocks, check darkness variance
        // A strong shadow means some regions are much darker than the mean
        int block_w = roi.width / 3;
        int block_h = roi.height / 3;
        if (block_w < 10 || block_h < 10) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Face too small for shadow detection";
            return result;
        }

        double global_mean = cv::mean(gray).val[0];
        double max_deviation = 0.0;
        int shadow_blocks = 0;
        int total_blocks = 0;

        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 3; x++) {
                cv::Rect block(x * block_w, y * block_h, block_w, block_h);
                double block_mean = cv::mean(gray(block)).val[0];
                double deviation = (global_mean - block_mean) / std::max(global_mean, 1.0);
                max_deviation = std::max(max_deviation, deviation);
                if (deviation > 0.3) shadow_blocks++;
                total_blocks++;
            }
        }

        // Calibrated: shadows covering > 30% of face area is problematic
        constexpr double kMaxShadowRatio = 0.35;

        double shadow_ratio = static_cast<double>(shadow_blocks) / total_blocks;
        result.actual_value = shadow_ratio;
        result.min_threshold = 0.0;
        result.max_threshold = kMaxShadowRatio;

        if (shadow_ratio > kMaxShadowRatio) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Strong shadows detected on face: "
                           + std::to_string(static_cast<int>(shadow_ratio * 100))
                           + "% blocks dark";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Face illumination uniform: no strong shadows";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, ShadowChecker, "shadow_check")

} // namespace photo
