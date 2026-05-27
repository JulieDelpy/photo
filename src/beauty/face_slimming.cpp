#include "photo/plugin/ibeauty_effect.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <algorithm>

namespace photo {

class FaceSlimmingEffect : public IBeautyEffect {
public:
    const char* name() const override { return "face_slimming"; }
    const char* version() const override { return "2.0.0"; }

    std::map<std::string, ParamDef> defaultParams() const override {
        return {
            {"jaw_strength", {0.35f, 0.0f, 0.7f, "Jaw slimming strength"}},
            {"cheek_strength", {0.20f, 0.0f, 0.5f, "Cheek slimming strength"}}
        };
    }

    bool apply(cv::Mat& image, const FaceInfo& face,
               const ParamMap& params) override {
        if (!face.detected) return false;

        float jaw_s = getParam(params, "jaw_strength", 0.35f);
        float cheek_s = getParam(params, "cheek_strength", 0.20f);
        if (jaw_s <= 0.0f && cheek_s <= 0.0f) return true;

        cv::Rect roi = face.bbox;
        int margin = static_cast<int>(roi.width * 0.25);
        roi.x = std::max(0, roi.x - margin);
        roi.y = std::max(0, roi.y - margin / 3);
        roi.width = std::min(image.cols - roi.x, roi.width + 2 * margin);
        roi.height = std::min(image.rows - roi.y, roi.height + margin);
        roi &= cv::Rect(0, 0, image.cols, image.rows);
        if (roi.area() <= 0) return false;

        float cx = roi.width / 2.0f;
        float face_cx = face.bbox.x + face.bbox.width / 2.0f - roi.x;
        float jaw_top = roi.height * 0.45f;

        cv::Mat map_x(roi.size(), CV_32F);
        cv::Mat map_y(roi.size(), CV_32F);

        for (int y = 0; y < roi.height; y++) {
            float ry = static_cast<float>(y);
            // 颚部以下逐渐增强
            float jaw_t = (ry - jaw_top) / (roi.height - jaw_top);
            jaw_t = std::max(0.0f, std::min(1.0f, jaw_t));

            for (int x = 0; x < roi.width; x++) {
                float rx = static_cast<float>(x);
                float dx = rx - face_cx;
                float abs_dx = std::abs(dx);

                // 基于人脸宽度的归一化距离
                float half_w = face.bbox.width * 0.55f;
                float nd = abs_dx / std::max(half_w, 1.0f);  // 0 在中心, >1 在边缘外

                // smoothstep 衰减：边缘强，中心弱
                float edge_w = nd < 0.5f ? 0.0f
                    : nd > 1.2f ? 1.0f
                    : (nd - 0.5f) / 0.7f;
                edge_w = edge_w * edge_w * (3.0f - 2.0f * edge_w);  // smoothstep

                // 上半脸用 cheek 强度，下半脸用 jaw 强度
                float strength = cheek_s + (jaw_s - cheek_s) * jaw_t;
                float shift = dx * edge_w * strength * 0.12f;

                map_x.at<float>(y, x) = rx - shift + roi.x;
                map_y.at<float>(y, x) = ry + roi.y;
            }
        }

        cv::Mat warped;
        cv::remap(image, warped, map_x, map_y, cv::INTER_CUBIC, cv::BORDER_REPLICATE);

        // 混合到原图 ROI
        cv::Mat result_roi = image(roi).clone();
        cv::addWeighted(warped, 0.85f, result_roi, 0.15f, 0, result_roi);
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
