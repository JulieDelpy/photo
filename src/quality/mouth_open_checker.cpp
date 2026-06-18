#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>

namespace photo {

class MouthOpenChecker : public IQualityChecker {
public:
    const char* name() const override { return "mouth_open_check"; }
    const char* version() const override { return "1.5.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard&) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "state";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "未检测到人脸，无法检查嘴部状态";
            return result;
        }

        constexpr double kMarFail = 0.30;
        constexpr double kMarWarn = 0.25;
        constexpr double kMarLow = 0.13;
        constexpr int kInnerMouthVDark = 80;
        constexpr int kInnerMouthVBright = 160;

        result.actual_value = face.mar;
        result.max_threshold = kMarFail;

        double oral_v = 0.0;
        double oral_s = 0.0;
        double teeth_ratio = 0.0;
        int teeth_pixels = 0;

        if (face.landmarks.size() >= 68) {
            std::vector<cv::Point> inner_pts;
            for (int i = 61; i <= 67; i++) {
                inner_pts.push_back(face.landmarks[i]);
            }
            if (!inner_pts.empty()) {
                cv::Rect inner_rect = cv::boundingRect(inner_pts);
                inner_rect &= cv::Rect(0, 0, image.cols, image.rows);
                if (inner_rect.area() > 0) {
                    cv::Mat roi = image(inner_rect);
                    cv::Mat hsv;
                    cv::cvtColor(roi, hsv, cv::COLOR_BGR2HSV);
                    std::vector<cv::Mat> hsv_channels;
                    cv::split(hsv, hsv_channels);
                    oral_s = cv::mean(hsv_channels[1]).val[0];
                    oral_v = cv::mean(hsv_channels[2]).val[0];
                }
            }

            std::vector<cv::Point> mouth_pts;
            for (int i = 48; i <= 67; i++) {
                mouth_pts.push_back(face.landmarks[i]);
            }
            cv::Rect mouth_rect = cv::boundingRect(mouth_pts);
            float cx = mouth_rect.x + mouth_rect.width * 0.5f;
            float cy = mouth_rect.y + mouth_rect.height * 0.5f;
            float half_w = std::max(mouth_rect.width * 0.58f, face.bbox.width * 0.10f);
            float half_h = std::max(mouth_rect.height * 0.90f, face.bbox.height * 0.035f);
            cv::Rect teeth_rect(static_cast<int>(cx - half_w),
                                static_cast<int>(cy - half_h),
                                static_cast<int>(half_w * 2.0f),
                                static_cast<int>(half_h * 2.0f));
            teeth_rect &= cv::Rect(0, 0, image.cols, image.rows);
            if (teeth_rect.area() > 0) {
                cv::Mat roi = image(teeth_rect);
                cv::Mat hsv;
                cv::cvtColor(roi, hsv, cv::COLOR_BGR2HSV);
                std::vector<cv::Mat> hsv_channels;
                cv::split(hsv, hsv_channels);
                for (int y = 0; y < hsv.rows; ++y) {
                    const uchar* s_row = hsv_channels[1].ptr<uchar>(y);
                    const uchar* v_row = hsv_channels[2].ptr<uchar>(y);
                    for (int x = 0; x < hsv.cols; ++x) {
                        if (v_row[x] > 175 && s_row[x] < 85) {
                            teeth_pixels++;
                        }
                    }
                }
                teeth_ratio = static_cast<double>(teeth_pixels) / teeth_rect.area();
            }
        }

        bool mouth_dark = (oral_v < kInnerMouthVDark);
        bool mouth_bright = (oral_v > kInnerMouthVBright);
        bool low_mar_teeth_like = (face.mar > 0.045) && (face.mar < kMarLow)
                               && (oral_v > 180) && (oral_s < 85);
        bool teeth_hard = mouth_bright && (oral_s < 70) && (oral_v > 180);
        bool visible_teeth = mouth_bright && (teeth_ratio > 0.50) && (oral_s < 80)
                          && ((face.mar > 0.18) || low_mar_teeth_like);

        bool fail_dark = (face.mar > kMarWarn) && mouth_dark;
        bool high_mar_open = (face.mar > kMarFail) && mouth_bright
                          && ((oral_v > 190) || (oral_s > 100 && teeth_ratio > 0.35));
        bool fail_bright = high_mar_open || visible_teeth;
        bool warn_bright = (face.mar > 0.22 && mouth_bright)
                        || teeth_hard
                        || low_mar_teeth_like;

        result.detail = "oral_v=" + std::to_string(static_cast<int>(oral_v))
                      + " oral_s=" + std::to_string(static_cast<int>(oral_s))
                      + " teeth_px=" + std::to_string(teeth_pixels)
                      + " teeth_ratio=" + std::to_string(teeth_ratio).substr(0, 5)
                      + " low_mar_teeth=" + std::to_string(low_mar_teeth_like ? 1 : 0);

        if (fail_dark) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "张嘴: MAR=" + std::to_string(face.mar).substr(0, 4)
                           + " 口腔V=" + std::to_string(static_cast<int>(oral_v));
        } else if (fail_bright) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "露齿: MAR=" + std::to_string(face.mar).substr(0, 4)
                           + " 口腔V=" + std::to_string(static_cast<int>(oral_v))
                           + " S=" + std::to_string(static_cast<int>(oral_s));
        } else if (warn_bright || face.mar > kMarWarn) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "嘴部边缘状态: MAR=" + std::to_string(face.mar).substr(0, 4)
                           + " 口腔V=" + std::to_string(static_cast<int>(oral_v))
                           + " S=" + std::to_string(static_cast<int>(oral_s));
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "嘴部状态正常: MAR=" + std::to_string(face.mar).substr(0, 4);
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, MouthOpenChecker, "mouth_open_check")

} // namespace photo
