#include "photo/plugin/ibeauty_effect.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class EyeEnlargementEffect : public IBeautyEffect {
public:
    const char* name() const override { return "eye_enlargement"; }
    const char* version() const override { return "1.0.0"; }

    std::map<std::string, ParamDef> defaultParams() const override {
        return {
            {"factor", {0.15f, 0.0f, 0.3f, "Enlargement factor (0=none, 0.3=max)"}}
        };
    }

    bool apply(cv::Mat& image, const FaceInfo& face,
               const ParamMap& params) override {
        if (!face.detected || face.landmarks.size() < 68) {
            // Fallback: use bbox-based eye region approximation
            if (!face.detected) return false;
            return applyApprox(image, face, params);
        }

        float factor = getParam(params, "factor", 0.15f);
        if (factor <= 0.0f) return true;

        // Left eye center: landmarks 36-41
        cv::Point2f left_center(0, 0);
        for (int i = 36; i <= 41; i++) left_center += face.landmarks[i];
        left_center *= (1.0f / 6.0f);

        // Right eye center: landmarks 42-47
        cv::Point2f right_center(0, 0);
        for (int i = 42; i <= 47; i++) right_center += face.landmarks[i];
        right_center *= (1.0f / 6.0f);

        // Estimate eye radius from inter-landmark distance
        float left_w = cv::norm(face.landmarks[36] - face.landmarks[39]);
        float left_h = cv::norm(face.landmarks[37] - face.landmarks[41]);
        float left_radius = std::max(left_w, left_h) * 1.5f;

        float right_w = cv::norm(face.landmarks[42] - face.landmarks[45]);
        float right_h = cv::norm(face.landmarks[43] - face.landmarks[47]);
        float right_radius = std::max(right_w, right_h) * 1.5f;

        enlargeEye(image, left_center, left_radius, factor);
        enlargeEye(image, right_center, right_radius, factor);

        return true;
    }

private:
    void enlargeEye(cv::Mat& image, cv::Point2f center, float radius, float factor) {
        int r_int = static_cast<int>(radius * 1.5f);
        cv::Rect roi(static_cast<int>(center.x) - r_int,
                     static_cast<int>(center.y) - r_int,
                     r_int * 2, r_int * 2);
        roi &= cv::Rect(0, 0, image.cols, image.rows);
        if (roi.area() <= 0) return;

        cv::Mat map_x(roi.size(), CV_32F);
        cv::Mat map_y(roi.size(), CV_32F);

        cv::Point2f local_center(center.x - roi.x, center.y - roi.y);

        for (int y = 0; y < roi.height; y++) {
            for (int x = 0; x < roi.width; x++) {
                float dx = static_cast<float>(x) - local_center.x;
                float dy = static_cast<float>(y) - local_center.y;
                float dist = std::sqrt(dx * dx + dy * dy);

                float scale;
                if (dist < radius) {
                    // Local scaling: pixels closer to center move less
                    float t = dist / radius;
                    scale = 1.0f - factor * (1.0f - t * t);
                } else {
                    scale = 1.0f;
                }

                map_x.at<float>(y, x) = local_center.x + dx * scale + roi.x;
                map_y.at<float>(y, x) = local_center.y + dy * scale + roi.y;
            }
        }

        cv::Mat roi_img = image(roi);
        cv::Mat warped;
        cv::remap(image, warped, map_x, map_y, cv::INTER_LINEAR, cv::BORDER_REPLICATE);
        warped(roi).copyTo(roi_img);
    }

    bool applyApprox(cv::Mat& image, const FaceInfo& face, const ParamMap& params) {
        // Approximate eye positions from bbox (upper 1/3 of face, left/right halves)
        float factor = getParam(params, "factor", 0.15f);
        int eye_y = face.bbox.y + face.bbox.height / 4;
        int left_x = face.bbox.x + face.bbox.width / 4;
        int right_x = face.bbox.x + face.bbox.width * 3 / 4;
        float radius = face.bbox.width / 8.0f;

        enlargeEye(image, cv::Point2f(static_cast<float>(left_x), static_cast<float>(eye_y)), radius, factor);
        enlargeEye(image, cv::Point2f(static_cast<float>(right_x), static_cast<float>(eye_y)), radius, factor);
        return true;
    }

    float getParam(const ParamMap& params, const std::string& key, float def) const {
        auto it = params.find(key);
        return (it != params.end()) ? it->second : def;
    }
};

REGISTER_PLUGIN(IBeautyEffect, EyeEnlargementEffect, "eye_enlargement")

} // namespace photo
