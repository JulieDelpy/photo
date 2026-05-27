#include "photo/plugin/ibeauty_effect.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>
#include <cstdint>

namespace photo {

class SkinSmoothingEffect : public IBeautyEffect {
public:
    const char* name() const override { return "skin_smoothing"; }
    const char* version() const override { return "2.0.2"; }

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

        // ---- 肤色掩膜 (CV_8UC1 → CV_32FC1, 0.0~1.0) ----
        cv::Mat hsv;
        cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
        cv::Mat skin_u8;
        cv::inRange(hsv, cv::Scalar(0, 15, 40), cv::Scalar(25, 160, 255), skin_u8);
        cv::dilate(skin_u8, skin_u8,
                   cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7)));
        cv::GaussianBlur(skin_u8, skin_u8, cv::Size(31, 31), 0);

        cv::Rect roi = face.bbox & cv::Rect(0, 0, image.cols, image.rows);
        if (roi.area() <= 0) return false;

        cv::Mat roi_u8 = image(roi);                 // CV_8UC3 引用原图
        cv::Mat mask_u8 = skin_u8(roi);              // CV_8UC1

        // ---- 全部转入 CV_32F ----
        cv::Mat src;    roi_u8.convertTo(src,   CV_32F);  // 原图 float
        cv::Mat mask;   mask_u8.convertTo(mask, CV_32F, 1.0 / 255.0);  // 掩膜 0~1

        // ---- 频率分离 (全部 CV_32F) ----
        int ks = std::max(5, static_cast<int>(roi.width * 0.03));
        if (ks % 2 == 0) ks++;
        cv::Mat base;
        cv::GaussianBlur(src, base, cv::Size(ks, ks), 0);  // base 层
        cv::Mat detail = src - base;                        // detail 层 (=高频纹理)

        // ---- 对 base 层做保边平滑 (8U 双边滤波) ----
        cv::Mat base_u8;
        base.convertTo(base_u8, CV_8U);
        cv::Mat base_sm_u8;
        int d = std::max(3, ks / 2);
        if (d % 2 == 0) d++;
        cv::bilateralFilter(base_u8, base_sm_u8, d,
                            20.0 + strength * 40.0,
                             4.0 + strength * 8.0);
        cv::Mat base_sm;
        base_sm_u8.convertTo(base_sm, CV_32F);

        // ---- 重建: 平滑 base + 保留部分 detail ----
        float detail_gain = 1.0f - texture * strength;  // 1.0 ~ 0.0
        cv::Mat result = base_sm + detail * detail_gain;  // CV_32F

        // ---- ROI 边缘羽化掩膜 ----
        cv::Mat feather(roi.size(), CV_32F, 1.0f);
        float fr = static_cast<float>(roi.height) * 0.10f;  // 上下各 10% 羽化
        for (int y = 0; y < roi.height; y++) {
            float dy = std::min({static_cast<float>(y),
                                 static_cast<float>(roi.height - 1 - y)}) / fr;
            if (dy < 1.0f) {
                float s = dy * dy * (3.0f - 2.0f * dy);  // smoothstep
                for (int x = 0; x < roi.width; x++)
                    feather.at<float>(y, x) = s;
            }
        }

        // ---- 按掩膜混合原图与平滑结果 ----
        cv::Mat blend_mask = mask.mul(feather) * strength;  // 肤色掩膜 × 羽化
        std::vector<cv::Mat> sch(3), rch(3), och(3);
        cv::split(src,    sch);
        cv::split(result, rch);

        for (int c = 0; c < 3; c++) {
            och[c] = sch[c].mul(1.0f - blend_mask) + rch[c].mul(blend_mask);
        }

        // ---- 安全转回 CV_8U (saturate_cast) ----
        cv::Mat merged;
        cv::merge(och, merged);
        merged.convertTo(roi_u8, CV_8U);  // saturate_cast<uchar> 内置

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
