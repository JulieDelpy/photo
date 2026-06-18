#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <string>
#include <vector>

namespace photo {

class BorderChecker : public IQualityChecker {
public:
    const char* name() const override { return "border_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard&) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "background";

        cv::Mat gray;
        if (image.channels() == 3)
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        else
            gray = image;

        int w = gray.cols, h = gray.rows;
        constexpr int kBandW = 10;
        constexpr int kInnerGap = 25;
        constexpr int kSegs = 5;
        constexpr double kShadowDiff = 22.0;

        // 背景参考亮度：顶部中央纯背景区域
        double bg_ref = 200;
        {
            int ref_w = w / 3;
            cv::Rect ref((w - ref_w) / 2, 4, ref_w, 8);
            ref &= cv::Rect(0, 0, w, h);
            if (ref.area() > 0) bg_ref = cv::mean(gray(ref))[0];
        }

        // 检测下界：下巴以上为纯背景区
        int bottom_limit = h;
        if (face.detected && face.landmarks.size() >= 9) {
            bottom_limit = static_cast<int>(face.landmarks[8].y) - h / 20;
            bottom_limit = std::max(bottom_limit, h / 3);
        }

        auto sample = [&](cv::Rect r) -> double {
            r &= cv::Rect(0, 0, w, h);
            if (r.area() <= 0) return bg_ref;
            if (face.detected && (r & face.bbox).area() > r.area() / 3)
                return bg_ref;
            return cv::mean(gray(r))[0];
        };

        int shadow_sides = 0;
        double max_diff = 0;
        std::string sides;

        // 沿边对比外缘与内移条带：外暗内亮 = 边缘局部阴影
        // 人物身体/衣服内外均暗 → 差小 → 不触发
        auto checkEdge = [&](int x_outer, int y_outer, int x_inner, int y_inner,
                              bool horizontal) -> double {
            double local_max = 0;
            int length = horizontal ? w : std::min(bottom_limit, h);
            int seg_len = length / kSegs;
            if (seg_len <= 0) return 0;

            for (int i = 0; i < kSegs; i++) {
                cv::Rect outer_r = horizontal
                    ? cv::Rect(i * seg_len, y_outer, seg_len, kBandW)
                    : cv::Rect(x_outer, i * seg_len, kBandW, seg_len);
                cv::Rect inner_r = horizontal
                    ? cv::Rect(i * seg_len, y_inner, seg_len, kBandW)
                    : cv::Rect(x_inner, i * seg_len, kBandW, seg_len);

                double ov = sample(outer_r);
                double iv = sample(inner_r);
                double diff = iv - ov;
                if (diff > kShadowDiff && diff > local_max)
                    local_max = diff;
            }
            return local_max;
        };

        double r;

        // Top
        r = checkEdge(0, 0, 0, kInnerGap, true);
        if (r > 0) { shadow_sides++; sides += "上"; if (r > max_diff) max_diff = r; }

        // Left（仅上半）
        r = checkEdge(0, 0, kInnerGap, 0, false);
        if (r > 0) { shadow_sides++; sides += "左"; if (r > max_diff) max_diff = r; }

        // Right（仅上半）
        r = checkEdge(w - kBandW, 0, w - kInnerGap, 0, false);
        if (r > 0) { shadow_sides++; sides += "右"; if (r > max_diff) max_diff = r; }

        result.actual_value = max_diff;
        result.max_threshold = kShadowDiff;

        if (shadow_sides >= 1) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "背景边缘存在阴影/不均: " + sides
                           + "侧 差值=" + std::to_string(static_cast<int>(max_diff));
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "背景边缘均匀: 差值="
                           + std::to_string(static_cast<int>(max_diff));
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, BorderChecker, "border_check")

} // namespace photo
