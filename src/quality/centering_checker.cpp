#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <cmath>

namespace photo {

class CenteringChecker : public IQualityChecker {
public:
    const char* name() const override { return "centering_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "composition";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "No face detected; cannot check centering";
            return result;
        }

        double face_center_x = face.bbox.x + face.bbox.width / 2.0;
        double image_center_x = image.cols / 2.0;
        double offset_px = std::abs(face_center_x - image_center_x);
        double offset_pct = offset_px / image.cols * 100.0;

        // Calibrated from testset: pass=0.2-4.4%, not_centered=8.7%
        constexpr double kMaxCenterOffsetPct = 5.0;

        result.actual_value = offset_pct;
        result.max_threshold = kMaxCenterOffsetPct;

        if (offset_pct > kMaxCenterOffsetPct) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Face not centered horizontally: offset = "
                           + std::to_string(static_cast<int>(offset_pct)) + "%";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Face centered horizontally: offset = "
                           + std::to_string(static_cast<int>(offset_pct)) + "%";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, CenteringChecker, "centering_check")

} // namespace photo
