#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class BlurChecker : public IQualityChecker {
public:
    const char* name() const override { return "blur_check"; }
    const char* version() const override { return "1.1.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "quality";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "No face detected; cannot check face blur";
            return result;
        }

        // Extract face ROI
        cv::Rect roi = face.bbox & cv::Rect(0, 0, image.cols, image.rows);
        if (roi.area() <= 0) {
            result.passed = false;
            result.severity = Severity::WARNING;
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

        // Laplacian variance for blur detection
        cv::Mat laplacian;
        cv::Laplacian(gray, laplacian, CV_64F);
        cv::Scalar mean, stddev;
        cv::meanStdDev(laplacian, mean, stddev);
        double laplacian_var = stddev.val[0] * stddev.val[0];

        // Face-only sharpness; use the configured ID-photo threshold.
        double kMinSharpness = std.min_sharpness > 0 ? std.min_sharpness : 120.0;

        result.actual_value = laplacian_var;
        result.min_threshold = kMinSharpness;
        result.max_threshold = 0.0;

        if (laplacian_var < kMinSharpness) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "面部清晰度不足: Laplacian方差="
                           + std::to_string(static_cast<int>(laplacian_var))
                           + " < " + std::to_string(static_cast<int>(kMinSharpness));
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "面部清晰度正常: Laplacian方差="
                           + std::to_string(static_cast<int>(laplacian_var));
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, BlurChecker, "blur_check")

} // namespace photo
