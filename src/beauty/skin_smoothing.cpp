#include "photo/plugin/ibeauty_effect.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class SkinSmoothingEffect : public IBeautyEffect {
public:
    const char* name() const override { return "skin_smoothing"; }
    const char* version() const override { return "1.0.0"; }

    std::map<std::string, ParamDef> defaultParams() const override {
        return {
            {"strength", {0.5f, 0.0f, 1.0f, "Smoothing strength (0=none, 1=max)"}},
            {"radius",   {10.0f, 3.0f, 30.0f, "Filter radius in pixels"}}
        };
    }

    bool apply(cv::Mat& image, const FaceInfo& face,
               const ParamMap& params) override {
        if (!face.detected) return false;

        float strength = getParam(params, "strength", 0.5f);
        int radius = static_cast<int>(getParam(params, "radius", 10.0f));

        // Skin mask via HSV skin color range
        cv::Mat hsv;
        cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);

        cv::Mat skin_mask;
        cv::inRange(hsv,
                    cv::Scalar(0, 20, 40),    // Lower bound H,S,V
                    cv::Scalar(25, 150, 255), // Upper bound
                    skin_mask);

        // Dilate mask slightly for better coverage
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
        cv::dilate(skin_mask, skin_mask, kernel);
        cv::GaussianBlur(skin_mask, skin_mask, cv::Size(21, 21), 0);

        // Restrict to face ROI
        cv::Rect roi = face.bbox & cv::Rect(0, 0, image.cols, image.rows);
        if (roi.area() <= 0) return false;

        cv::Mat face_roi = image(roi);
        cv::Mat mask_roi = skin_mask(roi);

        // Bilateral filter on face region
        cv::Mat smoothed;
        int d = radius * 2 + 1;
        double sigma_color = 30.0 * strength;
        double sigma_space = radius;
        cv::bilateralFilter(face_roi, smoothed, d, sigma_color, sigma_space);

        // Blend smoothed with original using mask and strength
        cv::Mat mask_float;
        mask_roi.convertTo(mask_float, CV_32F, 1.0 / 255.0);
        mask_float *= strength;

        std::vector<cv::Mat> channels_orig, channels_smooth;
        cv::split(face_roi, channels_orig);
        cv::split(smoothed, channels_smooth);

        for (int c = 0; c < 3; c++) {
            channels_orig[c].convertTo(channels_orig[c], CV_32F);
            channels_smooth[c].convertTo(channels_smooth[c], CV_32F);
            cv::Mat blended = channels_orig[c].mul(1.0f - mask_float)
                            + channels_smooth[c].mul(mask_float);
            blended.convertTo(channels_orig[c], CV_8U);
        }

        cv::merge(channels_orig, face_roi);
        return true;
    }

private:
    float getParam(const ParamMap& params, const std::string& key, float def) const {
        auto it = params.find(key);
        return (it != params.end()) ? it->second : def;
    }
};

REGISTER_PLUGIN(IBeautyEffect, SkinSmoothingEffect, "skin_smoothing")

} // namespace photo
