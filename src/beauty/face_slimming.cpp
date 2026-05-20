#include "photo/plugin/ibeauty_effect.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>
#include <cmath>

namespace photo {

class FaceSlimmingEffect : public IBeautyEffect {
public:
    const char* name() const override { return "face_slimming"; }
    const char* version() const override { return "1.0.0"; }

    std::map<std::string, ParamDef> defaultParams() const override {
        return {
            {"jaw_strength", {0.5f, 0.0f, 1.0f, "Jaw slimming strength (0=none, 1=max)"}},
            {"cheek_strength", {0.3f, 0.0f, 0.7f, "Cheek slimming strength (0=none, 0.7=max)"}}
        };
    }

    bool apply(cv::Mat& image, const FaceInfo& face,
               const ParamMap& params) override {
        if (!face.detected) return false;

        float jaw_strength = getParam(params, "jaw_strength", 0.5f);
        float cheek_strength = getParam(params, "cheek_strength", 0.3f);

        if (jaw_strength <= 0.0f && cheek_strength <= 0.0f) return true;

        cv::Rect roi = face.bbox;
        // Expand ROI for deformation margin
        int margin = static_cast<int>(roi.width * 0.3);
        roi.x = std::max(0, roi.x - margin);
        roi.y = std::max(0, roi.y - margin / 2);
        roi.width = std::min(image.cols - roi.x, roi.width + 2 * margin);
        roi.height = std::min(image.rows - roi.y, roi.height + margin);
        roi &= cv::Rect(0, 0, image.cols, image.rows);

        if (roi.area() <= 0) return false;

        // Build deformation map using MLS-like approach
        // Simplified: horizontal compression in lower half of face
        cv::Mat map_x(roi.size(), CV_32F);
        cv::Mat map_y(roi.size(), CV_32F);

        float cx = roi.width / 2.0f;
        float cy = roi.height / 2.0f;
        float jaw_y_start = roi.height * 0.5f;
        float jaw_radius = roi.width * 0.8f;

        for (int y = 0; y < roi.height; y++) {
            for (int x = 0; x < roi.width; x++) {
                float dx = static_cast<float>(x) - cx;
                float dy = static_cast<float>(y) - jaw_y_start;

                float dist = std::sqrt(dx * dx + dy * dy);
                float influence;

                if (y < jaw_y_start) {
                    // Upper face: cheek area (weaker effect)
                    influence = cheek_strength * std::exp(-dist * dist / (2.0f * jaw_radius * jaw_radius));
                    influence *= static_cast<float>(y) / jaw_y_start; // Fade to zero at top
                } else {
                    // Lower face: jaw area (stronger effect)
                    float jaw_dist = std::sqrt(dx * dx + (dy * 0.8f) * (dy * 0.8f));
                    float jaw_influence = std::exp(-jaw_dist * jaw_dist / (2.0f * jaw_radius * jaw_radius * 0.25f));
                    influence = jaw_strength * jaw_influence;
                }

                float new_x = static_cast<float>(x) + dx * influence * 0.15f;
                map_x.at<float>(y, x) = new_x + roi.x;
                map_y.at<float>(y, x) = static_cast<float>(y) + roi.y;
            }
        }

        cv::Mat roi_img = image(roi).clone();
        cv::Mat warped;
        cv::remap(image, warped, map_x, map_y, cv::INTER_LINEAR, cv::BORDER_REPLICATE);
        cv::Mat warped_roi = warped(roi);

        // Smooth feathering on edges of modified region
        cv::Mat result_roi = image(roi).clone();
        float blend_strength = 0.8f;
        cv::addWeighted(warped_roi, blend_strength, result_roi, 1.0f - blend_strength, 0, result_roi);
        result_roi.copyTo(image(roi));

        return true;
    }

private:
    float getParam(const ParamMap& params, const std::string& key, float def) const {
        auto it = params.find(key);
        return (it != params.end()) ? it->second : def;
    }
};

REGISTER_PLUGIN(IBeautyEffect, FaceSlimmingEffect, "face_slimming")

} // namespace photo
