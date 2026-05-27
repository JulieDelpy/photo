#include "photo/plugin/ibeauty_effect.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>
#include <cmath>

namespace photo {

class SkinWhiteningEffect : public IBeautyEffect {
public:
    const char* name() const override { return "skin_whitening"; }
    const char* version() const override { return "2.0.0"; }

    std::map<std::string, ParamDef> defaultParams() const override {
        return {
            {"strength", {0.3f, 0.0f, 1.0f, "Whitening strength"}},
            {"warmth",   {0.4f, 0.0f, 1.0f, "Skin warmth preservation (0=cold, 1=warm)"}}
        };
    }

    bool apply(cv::Mat& image, const FaceInfo& face,
               const ParamMap& params) override {
        if (!face.detected) return false;

        float strength = getParam(params, "strength", 0.3f);
        float warmth = getParam(params, "warmth", 0.4f);
        if (strength <= 0.001f) return true;

        // 肤色掩膜
        cv::Mat hsv;
        cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
        cv::Mat skin;
        cv::inRange(hsv, cv::Scalar(0, 15, 40), cv::Scalar(25, 160, 255), skin);
        cv::GaussianBlur(skin, skin, cv::Size(35, 35), 0);

        // 转为 LAB 空间处理：L 通道提亮（感知亮度），A/B 通道不位移
        cv::Mat lab;
        cv::cvtColor(image, lab, cv::COLOR_BGR2Lab);
        std::vector<cv::Mat> lab_ch;
        cv::split(lab, lab_ch);

        cv::Mat l_f;
        lab_ch[0].convertTo(l_f, CV_32F);

        // log 曲线提亮：暗部多提、亮部少提，保留高光过渡自然
        cv::Mat mask_f;
        skin.convertTo(mask_f, CV_32F, 1.0 / 255.0);

        // 基于当前 L 的提亮量：L_new = L + (255-L) * (L/255)^0.4 * mask * strength * 0.35
        cv::Mat l_ratio;
        cv::divide(l_f, 255.0f, l_ratio);
        cv::Mat l_pow;
        cv::pow(l_ratio, 0.4f, l_pow);  // 暗部 0.4 次方 → 多提
        cv::Mat boost = (255.0f - l_f).mul(l_pow).mul(mask_f) * strength * 0.35f;
        lab_ch[0] = l_f + boost;
        cv::min(lab_ch[0], 255.0f, lab_ch[0]);

        // A/B 通道微调：保留暖色调
        if (warmth > 0.01f) {
            for (int c = 1; c <= 2; c++) {
                cv::Mat ch_f;
                lab_ch[c].convertTo(ch_f, CV_32F);
                // 向 128(中性色) 微调，程度由 warmth 控制
                float pull = strength * (1.0f - warmth) * 0.15f;
                cv::Mat adj = (128.0f - ch_f) * pull;
                lab_ch[c] = ch_f + adj.mul(mask_f);
            }
        }
        lab_ch[0].convertTo(lab_ch[0], CV_8U);
        lab_ch[1].convertTo(lab_ch[1], CV_8U);
        lab_ch[2].convertTo(lab_ch[2], CV_8U);
        cv::merge(lab_ch, lab);
        cv::cvtColor(lab, image, cv::COLOR_Lab2BGR);

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
