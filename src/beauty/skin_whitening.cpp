#include "photo/plugin/ibeauty_effect.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class SkinWhiteningEffect : public IBeautyEffect {
public:
    const char* name() const override { return "skin_whitening"; }
    const char* version() const override { return "1.0.0"; }

    std::map<std::string, ParamDef> defaultParams() const override {
        return {
            {"strength", {0.5f, 0.0f, 1.0f, "Whitening strength (0=none, 1=max)"}}
        };
    }

    bool apply(cv::Mat& image, const FaceInfo& face,
               const ParamMap& params) override {
        if (!face.detected) return false;

        float strength = getParam(params, "strength", 0.5f);

        // Skin mask
        cv::Mat hsv;
        cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);

        cv::Mat skin_mask;
        cv::inRange(hsv,
                    cv::Scalar(0, 20, 40),
                    cv::Scalar(25, 150, 255),
                    skin_mask);

        cv::GaussianBlur(skin_mask, skin_mask, cv::Size(31, 31), 0);
        cv::Mat mask_float;
        skin_mask.convertTo(mask_float, CV_32F, 1.0 / 255.0);
        mask_float *= strength;

        // Convert to HSV and increase V channel on skin regions
        std::vector<cv::Mat> hsv_channels;
        cv::split(hsv, hsv_channels);

        hsv_channels[2].convertTo(hsv_channels[2], CV_32F);
        // Apply gamma-like brightening: V_new = V + (255-V) * mask * strength
        cv::Mat v_boost = (255.0f - hsv_channels[2]).mul(mask_float) * 0.5f;
        hsv_channels[2] += v_boost;
        hsv_channels[2] = cv::min(hsv_channels[2], 255.0f);
        hsv_channels[2].convertTo(hsv_channels[2], CV_8U);

        // Reduce saturation slightly for natural look
        hsv_channels[1].convertTo(hsv_channels[1], CV_32F);
        cv::Mat s_reduce = hsv_channels[1].mul(mask_float) * 0.3f;
        hsv_channels[1] -= s_reduce;
        hsv_channels[1] = cv::max(hsv_channels[1], 0.0f);
        hsv_channels[1].convertTo(hsv_channels[1], CV_8U);

        cv::merge(hsv_channels, hsv);
        cv::cvtColor(hsv, image, cv::COLOR_HSV2BGR);

        return true;
    }

private:
    float getParam(const ParamMap& params, const std::string& key, float def) const {
        auto it = params.find(key);
        return (it != params.end()) ? it->second : def;
    }
};

REGISTER_PLUGIN(IBeautyEffect, SkinWhiteningEffect, "skin_whitening")

} // namespace photo
