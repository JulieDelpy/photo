#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class BackgroundEdgeChecker : public IQualityChecker {
public:
    const char* name() const override { return "background_edge_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& std) override {
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
        // noise from being misidentified as background edges / clutter.
        cv::GaussianBlur(gray, blurred, cv::Size(3, 3), 0);

        cv::Mat edges;
        cv::Canny(blurred, edges, 50, 150);

        // Sample corners
        int w = image.cols, h = image.rows;
        int sw = std::max(w / 6, 20), sh = std::max(h / 6, 20);

        cv::Rect corners[4] = {
            {0, 0, sw, sh}, {w - sw, 0, sw, sh},
            {0, h - sh, sw, sh}, {w - sw, h - sh, sw, sh}
        };

        cv::Rect face_roi = face.detected ? face.bbox : cv::Rect(0,0,0,0);
        int edge_pixels = 0;
        int total_pixels = 0;

        for (auto& cr : corners) {
            cv::Rect r = cr & cv::Rect(0, 0, w, h);
            if (face.detected && (r & face_roi).area() > r.area() / 3) continue;
            edge_pixels += cv::countNonZero(edges(r));
            total_pixels += r.area();
        }

        if (total_pixels == 0) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Cannot measure background edge density";
            return result;
        }

        double density = static_cast<double>(edge_pixels) / total_pixels;

        constexpr double kBgEdgeDensityMax = 0.15;
        result.actual_value = density;
        result.max_threshold = kBgEdgeDensityMax;

        if (density > kBgEdgeDensityMax) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Background has too many edges/objects: density = "
                           + std::to_string(static_cast<int>(density * 100)) + "%";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Background edge density OK: "
                           + std::to_string(static_cast<int>(density * 100)) + "%";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, BackgroundEdgeChecker, "background_edge_check")

} // namespace photo
