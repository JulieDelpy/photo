#include "photo/plugin/ibeauty_effect.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <algorithm>

namespace photo {

class FaceSlimmingEffect : public IBeautyEffect {
public:
    const char* name() const override { return "face_slimming"; }
    const char* version() const override { return "2.0.1"; }

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

        float face_cx = face.bbox.x + face.bbox.width / 2.0f - roi.x;
        float jaw_top = roi.height * 0.45f;

        cv::Mat map_x(roi.size(), CV_32F);
        cv::Mat map_y(roi.size(), CV_32F);

        for (int y = 0; y < roi.height; y++) {
            float ry = static_cast<float>(y);
            float jaw_t = (ry - jaw_top) / (roi.height - jaw_top);
            jaw_t = std::max(0.0f, std::min(1.0f, jaw_t));

            for (int x = 0; x < roi.width; x++) {
                float rx = static_cast<float>(x);
                float dx = rx - face_cx;
                float abs_dx = std::abs(dx);

                float half_w = face.bbox.width * 0.55f;
                float nd = abs_dx / std::max(half_w, 1.0f);

                float edge_w = nd < 0.5f ? 0.0f
                    : nd > 1.2f ? 1.0f
                    : (nd - 0.5f) / 0.7f;
                edge_w = edge_w * edge_w * (3.0f - 2.0f * edge_w);

                float strength = cheek_s + (jaw_s - cheek_s) * jaw_t;
                float shift = dx * edge_w * strength * 0.12f;

                map_x.at<float>(y, x) = rx + shift + roi.x;
                map_y.at<float>(y, x) = ry + roi.y;
            }
        }

        cv::Mat warped;
        cv::remap(image, warped, map_x, map_y, cv::INTER_CUBIC, cv::BORDER_REPLICATE);

        // ---- ROI 边缘羽化掩膜：避免边界割裂 ----
        cv::Mat feather(roi.size(), CV_32F);
        float fr = static_cast<float>(std::min(roi.width, roi.height)) * 0.12f;
        fr = std::max(fr, 8.0f);
        for (int y = 0; y < roi.height; y++) {
            float dy = std::min({static_cast<float>(y),
                                 static_cast<float>(roi.height - 1 - y)}) / fr;
            dy = std::min(1.0f, dy);
            float sy = dy * dy * (3.0f - 2.0f * dy);  // smoothstep
            for (int x = 0; x < roi.width; x++) {
                float dx = std::min({static_cast<float>(x),
                                     static_cast<float>(roi.width - 1 - x)}) / fr;
                dx = std::min(1.0f, dx);
                float sx = dx * dx * (3.0f - 2.0f * dx);
                feather.at<float>(y, x) = std::min(sx, sy);
            }
        }

        // 用羽化掩膜逐通道混合
        float blend = 0.88f;
        cv::Mat orig_f, warp_f;
        image(roi).convertTo(orig_f, CV_32F);
        warped.convertTo(warp_f, CV_32F);
        std::vector<cv::Mat> och(3), wch(3);
        cv::split(orig_f, och);
        cv::split(warp_f, wch);
        for (int c = 0; c < 3; c++) {
            cv::Mat alpha = feather * blend;
            och[c] = wch[c].mul(alpha) + och[c].mul(1.0f - alpha);
        }
        cv::Mat merged;
        cv::merge(och, merged);
        merged.convertTo(image(roi), CV_8U);

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
