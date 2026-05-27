#include "photo/plugin/ibeauty_effect.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class SkinSmoothingEffect : public IBeautyEffect {
public:
    const char* name() const override { return "skin_smoothing"; }
    const char* version() const override { return "2.0.0"; }

    std::map<std::string, ParamDef> defaultParams() const override {
        return {
            {"strength", {0.3f, 0.0f, 1.0f, "Smoothing strength"}},
            {"texture",  {0.6f, 0.0f, 1.0f, "Skin texture preservation"}}
        };
    }

    bool apply(cv::Mat& image, const FaceInfo& face,
               const ParamMap& params) override {
        if (!face.detected) return false;

        float strength = getParam(params, "strength", 0.3f);
        float texture = getParam(params, "texture", 0.6f);
        if (strength <= 0.001f) return true;

        // 肤色掩膜
        cv::Mat hsv;
        cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
        cv::Mat skin;
        cv::inRange(hsv, cv::Scalar(0, 15, 40), cv::Scalar(25, 160, 255), skin);

        // 扩大掩膜覆盖范围
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
        cv::dilate(skin, skin, kernel);
        cv::GaussianBlur(skin, skin, cv::Size(31, 31), 0);

        cv::Rect roi = face.bbox & cv::Rect(0, 0, image.cols, image.rows);
        if (roi.area() <= 0) return false;

        cv::Mat face_roi = image(roi);
        cv::Mat mask_roi = skin(roi);

        // 频率分离：base(大面积色块) + detail(纹理细纹)
        cv::Mat face_f;
        face_roi.convertTo(face_f, CV_32F);
        cv::Mat base;
        int kernel_sz = std::max(5, static_cast<int>(roi.width * 0.03));
        if (kernel_sz % 2 == 0) kernel_sz++;
        cv::blur(face_f, base, cv::Size(kernel_sz, kernel_sz));
        cv::Mat detail = face_f - base;

        // 对 base 层做表面模糊（保边）
        cv::Mat base_u8;
        base.convertTo(base_u8, CV_8U);
        cv::Mat base_smooth;
        int d = std::max(3, kernel_sz / 2);
        if (d % 2 == 0) d++;
        double sigma_c = 20.0 + strength * 40.0;  // 20~60
        double sigma_s = 4.0 + strength * 8.0;     // 4~12
        cv::bilateralFilter(base_u8, base_smooth, d, sigma_c, sigma_s);
        cv::Mat base_f2;
        base_smooth.convertTo(base_f2, CV_32F);

        // 重建：平滑base + 保留部分detail
        float detail_gain = 1.0f - texture * strength;
        detail_gain = std::max(0.0f, detail_gain);
        cv::Mat result_f = base_f2 + detail * detail_gain;

        // 按掩膜强度混合
        cv::Mat mask_f;
        mask_roi.convertTo(mask_f, CV_32F, 1.0 / 255.0);
        cv::Mat blend_f;
        std::vector<cv::Mat> ch_orig(3), ch_res(3), ch_blend(3);
        cv::split(face_f, ch_orig);
        cv::split(result_f, ch_res);
        for (int c = 0; c < 3; c++) {
            ch_blend[c] = ch_orig[c].mul(1.0f - mask_f * strength) + ch_res[c].mul(mask_f * strength);
            ch_blend[c].convertTo(ch_blend[c], CV_8U);
        }
        cv::merge(ch_blend, face_roi);
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
