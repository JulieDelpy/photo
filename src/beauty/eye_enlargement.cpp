#include "photo/plugin/ibeauty_effect.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>
#include <cmath>

namespace photo {

class EyeEnlargementEffect : public IBeautyEffect {
public:
    const char* name() const override { return "eye_enlargement"; }
    const char* version() const override { return "2.0.0"; }

    std::map<std::string, ParamDef> defaultParams() const override {
        return {
            {"factor", {0.10f, 0.0f, 0.25f, "Enlargement factor"}}
        };
    }

    bool apply(cv::Mat& image, const FaceInfo& face,
               const ParamMap& params) override {
        if (!face.detected) return false;

        float factor = getParam(params, "factor", 0.10f);
        if (factor <= 0.0f) return true;

        if (face.landmarks.size() >= 68) {
            cv::Point2f left_c(0, 0), right_c(0, 0);
            for (int i = 36; i <= 41; i++) left_c += face.landmarks[i];
            left_c *= (1.0f / 6.0f);
            for (int i = 42; i <= 47; i++) right_c += face.landmarks[i];
            right_c *= (1.0f / 6.0f);

            float lr = std::max(cv::norm(face.landmarks[36] - face.landmarks[39]),
                                cv::norm(face.landmarks[37] - face.landmarks[41])) * 1.3f;
            float rr = std::max(cv::norm(face.landmarks[42] - face.landmarks[45]),
                                cv::norm(face.landmarks[43] - face.landmarks[47])) * 1.3f;

            enlarge(image, left_c, lr, factor);
            enlarge(image, right_c, rr, factor);
        } else {
            // 无 landmark 时用 bbox 估算
            int ey = face.bbox.y + face.bbox.height / 4;
            int lx = face.bbox.x + face.bbox.width / 4;
            int rx = face.bbox.x + face.bbox.width * 3 / 4;
            float r = face.bbox.width / 8.0f;
            enlarge(image, cv::Point2f(lx, ey), r, factor);
            enlarge(image, cv::Point2f(rx, ey), r, factor);
        }
        return true;
    }

private:
    // 球面变形：中心区域向外膨胀，边缘平滑过渡
    void enlarge(cv::Mat& image, cv::Point2f center, float radius, float factor) {
        int pad = static_cast<int>(radius * 1.6f);
        cv::Rect roi(static_cast<int>(center.x) - pad,
                     static_cast<int>(center.y) - pad,
                     pad * 2, pad * 2);
        roi &= cv::Rect(0, 0, image.cols, image.rows);
        if (roi.area() <= 0) return;

        cv::Point2f lc(center.x - roi.x, center.y - roi.y);
        float r2 = radius * radius;
        float inv_r = 1.0f / radius;

        cv::Mat map_x(roi.size(), CV_32F);
        cv::Mat map_y(roi.size(), CV_32F);

        for (int y = 0; y < roi.height; y++) {
            for (int x = 0; x < roi.width; x++) {
                float dx = x - lc.x;
                float dy = y - lc.y;
                float d2 = dx * dx + dy * dy;
                float d = std::sqrt(d2);

                float tx, ty;
                if (d < radius) {
                    // smoothstep 过渡：中心强，边缘弱
                    float t = d * inv_r;           // 0~1
                    float w = 1.0f - t * t * (3.0f - 2.0f * t);  // smoothstep 反向
                    float s = 1.0f - factor * w;
                    tx = lc.x + dx * s + roi.x;
                    ty = lc.y + dy * s + roi.y;
                } else if (d < radius * 1.5f) {
                    // 过渡带：轻微回缩避免硬边
                    float t = (d - radius) / (radius * 0.5f);
                    float w = (1.0f - t) * (1.0f - t) * factor * 0.3f;
                    tx = lc.x + dx * (1.0f + w) + roi.x;
                    ty = lc.y + dy * (1.0f + w) + roi.y;
                } else {
                    tx = x + roi.x;
                    ty = y + roi.y;
                }
                map_x.at<float>(y, x) = tx;
                map_y.at<float>(y, x) = ty;
            }
        }

        cv::Mat warped;
        cv::remap(image, warped, map_x, map_y, cv::INTER_CUBIC, cv::BORDER_REPLICATE);
        warped.copyTo(image(roi));
    }

    float getParam(const ParamMap& params, const std::string& key, float def) const {
        auto it = params.find(key);
        return (it != params.end()) ? it->second : def;
    }
};

REGISTER_PLUGIN(IBeautyEffect, EyeEnlargementEffect, "eye_enlargement")

} // namespace photo
