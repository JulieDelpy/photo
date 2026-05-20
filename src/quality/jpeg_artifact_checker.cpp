#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class JpegArtifactChecker : public IQualityChecker {
public:
    const char* name() const override { return "jpeg_artifact_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo&,
                      const IDPhotoStandard&) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "quality";

        cv::Mat gray;
        if (image.channels() == 3) {
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = image;
        }

        // Detect JPEG blocking artifacts:
        // Compute horizontal difference and check for periodic 8-pixel patterns
        cv::Mat diff;
        cv::Mat kernel = (cv::Mat_<float>(1, 2) << 1.0, -1.0);
        cv::filter2D(gray, diff, CV_32F, kernel);

        // Sample along rows at 8-pixel intervals to detect blocking
        double block_energy = 0.0;
        double total_energy = 0.0;
        int count_block = 0;
        int count_total = 0;

        for (int y = 0; y < diff.rows; y++) {
            const float* row = diff.ptr<float>(y);
            for (int x = 8; x < diff.cols - 1; x++) {
                total_energy += std::abs(row[x]);
                count_total++;
                if (x % 8 == 0) {
                    block_energy += std::abs(row[x]);
                    count_block++;
                }
            }
        }

        double ratio = (count_total > 0 && count_block > 0)
                       ? (block_energy / count_block) / (total_energy / count_total)
                       : 1.0;

        result.actual_value = ratio;
        result.max_threshold = 2.0;

        if (ratio > 2.0) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "JPEG blocking artifacts detected: ratio = "
                           + std::to_string(ratio);
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "No significant JPEG artifacts: ratio = "
                           + std::to_string(ratio);
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, JpegArtifactChecker, "jpeg_artifact_check")

} // namespace photo
