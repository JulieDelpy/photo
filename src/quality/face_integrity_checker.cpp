#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class FaceIntegrityChecker : public IQualityChecker {
public:
    const char* name() const override { return "face_integrity_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard&) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "face";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "No face detected; cannot check face integrity";
            return result;
        }

        // Check if face bounding box is clipped by image boundary
        bool clipped_left   = (face.bbox.x <= 1);
        bool clipped_right  = (face.bbox.x + face.bbox.width >= image.cols - 1);
        bool clipped_top    = (face.bbox.y <= 1);
        bool clipped_bottom = (face.bbox.y + face.bbox.height >= image.rows - 1);

        int clipped_edges = clipped_left + clipped_right + clipped_top + clipped_bottom;
        result.actual_value = static_cast<double>(clipped_edges);
        result.max_threshold = 0.0;

        if (clipped_edges > 0) {
            result.passed = false;
            result.severity = Severity::FAIL;
            std::string edges;
            if (clipped_left) edges += "left ";
            if (clipped_right) edges += "right ";
            if (clipped_top) edges += "top ";
            if (clipped_bottom) edges += "bottom ";
            result.message = "Face partially out of frame: clipped at " + edges;
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Face fully within image bounds";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, FaceIntegrityChecker, "face_integrity_check")

} // namespace photo
