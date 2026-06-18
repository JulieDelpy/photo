#include "photo/plugin/ibeauty_effect.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <algorithm>

namespace photo {

// ---- 平滑阶跃函数: t ∈ [0,1], 值域 [0,1], 在 t=0 和 t=1 处一阶导数为 0 ----
inline float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

class FaceSlimmingEffect : public IBeautyEffect {
public:
    const char* name() const override { return "face_slimming"; }
    const char* version() const override { return "3.2.1"; }

    std::map<std::string, ParamDef> defaultParams() const override {
        return {
            {"jaw_strength",   {0.35f, 0.0f, 0.7f, "Jaw slimming strength"}},
            {"cheek_strength", {0.20f, 0.0f, 0.5f, "Cheek slimming strength"}}
        };
    }

    bool apply(cv::Mat& image, const FaceInfo& face,
               const ParamMap& params) override {
        if (!face.detected) return false;

        float jaw_s   = getParam(params, "jaw_strength",   0.35f);
        float cheek_s = getParam(params, "cheek_strength", 0.20f);
        if (jaw_s <= 0.001f && cheek_s <= 0.001f) return true;

        // ---- 面部中心（鼻尖 landmark[30]） ----
        float nose_x, nose_y;
        bool  has_68 = (face.landmarks.size() >= 68);
        if (has_68) {
            nose_x = face.landmarks[30].x;
            nose_y = face.landmarks[30].y;
        } else {
            nose_x = face.bbox.x + face.bbox.width  / 2.0f;
            nose_y = face.bbox.y + face.bbox.height / 2.0f;
        }

        // ---- 作用半径 & ROI ----
        float R  = std::max(15.0f, face.bbox.width * 0.22f);
        float R2 = R * R;

        cv::Rect roi = face.bbox;
        int margin = static_cast<int>(R * 1.6f);
        roi.x      = std::max(0, roi.x - margin);
        roi.y      = std::max(0, roi.y - margin / 2);
        roi.width  = std::min(image.cols - roi.x, roi.width  + 2 * margin);
        roi.height = std::min(image.rows - roi.y, roi.height + margin);
        roi &= cv::Rect(0, 0, image.cols, image.rows);
        if (roi.area() <= 0) return false;

        // ============================================================
        // 1. 作用点（下颌轮廓）：方向 → 鼻尖
        // ============================================================
        struct ActionPt {
            float x, y;         // 世界坐标
            float dir_x, dir_y; // 指向鼻尖的单位向量
            float max_disp;     // d=0 处最大位移 = R * 0.15 * strength
        };
        std::vector<ActionPt> action_pts;

        auto add_side = [&](int i0, int i1) {
            int n = i1 - i0;
            for (int i = i0; i <= i1 && i < (int)face.landmarks.size(); ++i) {
                float t = static_cast<float>(i - i0) / std::max(1, n);
                float s = cheek_s + (jaw_s - cheek_s) * t;

                float dx = nose_x - face.landmarks[i].x;
                float dy = nose_y - face.landmarks[i].y;
                float len = std::sqrt(dx * dx + dy * dy);
                if (len < 0.5f) { dx = 0; dy = 0; }
                else            { dx /= len; dy /= len; }

                action_pts.push_back({face.landmarks[i].x,
                                      face.landmarks[i].y,
                                      dx, dy,
                                      R * 0.15f * s});
            }
        };

        if (face.landmarks.size() >= 17) {
            add_side(3, 7);    // 左侧脸颊→下颌
            add_side(9, 13);   // 右侧下颌→脸颊
        }

        // 降级：bbox 两侧
        if (action_pts.empty()) {
            float bl = static_cast<float>(face.bbox.x);
            float br = static_cast<float>(face.bbox.x + face.bbox.width);
            float my = static_cast<float>(face.bbox.y + face.bbox.height * 0.35f);
            float ly = static_cast<float>(face.bbox.y + face.bbox.height * 0.75f);
            auto add_fb = [&](float px, float py, float s) {
                float dx = nose_x - px, dy = nose_y - py;
                float len = std::sqrt(dx*dx + dy*dy);
                if (len > 0.5f) { dx /= len; dy /= len; }
                action_pts.push_back({px, py, dx, dy, R * 0.15f * s});
            };
            add_fb(bl, my, cheek_s);  add_fb(bl, ly, jaw_s);
            add_fb(br, my, cheek_s);  add_fb(br, ly, jaw_s);
        }

        // ============================================================
        // 2. 锚点（五官保护）：位移 = 0，极高权重 → 钉住五官
        //    眼角 36/39/42/45  嘴角 48/54  鼻尖 30  嘴中 51/57
        // ============================================================
        struct AnchorPt {
            float x, y;
        };
        static const int kAnchorIndices[] = {
            30,                               // 鼻尖
            36, 39, 42, 45,                   // 眼角
            48, 51, 54, 57                    // 嘴角 + 嘴中
        };
        constexpr int kNumAnchors = sizeof(kAnchorIndices) / sizeof(kAnchorIndices[0]);
        constexpr float kAnchorWeight = 5.0f;  // 锚点权重倍数

        float R_a  = face.bbox.width * 0.05f;  // 锚点作用半径（小）
        float R_a2 = R_a * R_a;

        std::vector<AnchorPt> anchor_pts;
        if (has_68) {
            for (int idx : kAnchorIndices)
                anchor_pts.push_back({face.landmarks[idx].x, face.landmarks[idx].y});
        }

        // ============================================================
        // 3. 第一步：计算原始位移场 → offset_x, offset_y
        // ============================================================
        cv::Mat offset_x(roi.size(), CV_32F, 0.0f);
        cv::Mat offset_y(roi.size(), CV_32F, 0.0f);

        const size_t n_action = action_pts.size();
        const size_t n_anchor = anchor_pts.size();

        for (int y = 0; y < roi.height; ++y) {
            float* ox_row = offset_x.ptr<float>(y);
            float* oy_row = offset_y.ptr<float>(y);
            float  gy     = static_cast<float>(y + roi.y);

            for (int x = 0; x < roi.width; ++x) {
                float gx = static_cast<float>(x + roi.x);

                // --- IDW: 作用点 ---
                float sum_w      = 0.0f;
                float sum_disp_x = 0.0f;
                float sum_disp_y = 0.0f;

                for (size_t i = 0; i < n_action; ++i) {
                    const auto& pt = action_pts[i];
                    float dx = gx - pt.x;
                    float dy = gy - pt.y;
                    float d2 = dx * dx + dy * dy;

                    if (d2 < R2) {
                        // Smoothstep 衰减: t = 1 - d/R, W = smoothstep(t)
                        // 保证在 d=R 处 W=0 且 dW/dd=0（C1 连续）
                        float nd = std::sqrt(d2) / R;           // d/R ∈ [0, 1)
                        float t  = 1.0f - nd;                   // 1 → 0
                        float w  = smoothstep(t);               // 1 → 0, C1

                        sum_w      += w;
                        sum_disp_x += w * pt.dir_x * pt.max_disp;
                        sum_disp_y += w * pt.dir_y * pt.max_disp;
                    }
                }

                // --- IDW: 锚点（位移 = 0, 极高权重 → 拉回零点） ---
                if (sum_w > 0.0f) {
                    for (size_t j = 0; j < n_anchor; ++j) {
                        const auto& ap = anchor_pts[j];
                        float adx = gx - ap.x;
                        float ady = gy - ap.y;
                        float ad2 = adx * adx + ady * ady;

                        if (ad2 < R_a2) {
                            float nd = std::sqrt(ad2) / R_a;
                            float t  = 1.0f - nd;
                            float w  = kAnchorWeight * smoothstep(t);
                            sum_w += w;
                            // 锚点位移 = 0, 不累加到 sum_disp
                        }
                    }
                }

                // --- 存入原始位移 ---
                if (sum_w > 0.0f) {
                    float inv_w = 1.0f / sum_w;
                    ox_row[x] = sum_disp_x * inv_w;
                    oy_row[x] = sum_disp_y * inv_w;
                }
                // else: 保持 0（已在初始化时置零）
            }
        }

        // ============================================================
        // 4. 核心步骤：对位移场做高斯平滑 → 消除断层/切割
        //    核大小根据作用半径 R 动态计算
        // ============================================================
        int blur_ks = static_cast<int>(R * 0.55f);
        blur_ks = std::max(15, std::min(31, blur_ks));
        if (blur_ks % 2 == 0) ++blur_ks;

        // 先将 offset 边界向外反射，避免高斯核在 ROI 边缘产生黑边
        cv::GaussianBlur(offset_x, offset_x, cv::Size(blur_ks, blur_ks),
                         0, 0, cv::BORDER_REFLECT);
        cv::GaussianBlur(offset_y, offset_y, cv::Size(blur_ks, blur_ks),
                         0, 0, cv::BORDER_REFLECT);

        // ============================================================
        // 5. 从平滑后的前向位移场构建 remap 采样矩阵
        //    offset 表示脸部轮廓希望移动的方向；remap 需要的是
        //    目标像素从源图哪里采样，所以符号必须取反。
        // ============================================================
        cv::Mat map_x(roi.size(), CV_32F);
        cv::Mat map_y(roi.size(), CV_32F);

        for (int y = 0; y < roi.height; ++y) {
            float* mx_row = map_x.ptr<float>(y);
            float* my_row = map_y.ptr<float>(y);
            float* ox_row = offset_x.ptr<float>(y);
            float* oy_row = offset_y.ptr<float>(y);
            float  gy     = static_cast<float>(y + roi.y);

            for (int x = 0; x < roi.width; ++x) {
                mx_row[x] = static_cast<float>(x + roi.x) - ox_row[x];
                my_row[x] = gy - oy_row[x];
            }
        }

        // ============================================================
        // 6. remap + ROI 边缘羽化
        // ============================================================
        cv::Mat warped;
        cv::remap(image, warped, map_x, map_y,
                  cv::INTER_LINEAR, cv::BORDER_REPLICATE);

        // ROI 边界 smoothstep 羽化（10% 边缘）
        cv::Mat feather(roi.size(), CV_32F, 1.0f);
        float fr = static_cast<float>(std::min(roi.width, roi.height)) * 0.10f;
        fr = std::max(fr, 12.0f);
        for (int y = 0; y < roi.height; ++y) {
            float dy = std::min({static_cast<float>(y),
                                 static_cast<float>(roi.height - 1 - y)}) / fr;
            dy = std::min(1.0f, dy);
            float sy = smoothstep(dy);
            for (int x = 0; x < roi.width; ++x) {
                float dx = std::min({static_cast<float>(x),
                                     static_cast<float>(roi.width - 1 - x)}) / fr;
                dx = std::min(1.0f, dx);
                float sx = smoothstep(dx);
                feather.at<float>(y, x) = std::min(sx, sy);
            }
        }

        cv::Mat orig_f, warp_f;
        image(roi).convertTo(orig_f, CV_32F);
        warped.convertTo(warp_f, CV_32F);
        std::vector<cv::Mat> och(3), wch(3);
        cv::split(orig_f, och);
        cv::split(warp_f, wch);
        for (int c = 0; c < 3; ++c) {
            cv::Mat alpha = feather * 0.90f;
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
