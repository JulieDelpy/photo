#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

namespace photo {

class VisualDefectChecker : public IQualityChecker {
public:
    const char* name() const override { return "visual_defect_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard&) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "quality";

        if (image.empty()) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "无图像内容";
            return result;
        }

        cv::Mat hsv, gray;
        if (image.channels() == 3) {
            cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = image;
            cv::Mat bgr;
            cv::cvtColor(gray, bgr, cv::COLOR_GRAY2BGR);
            cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
        }

        double red_eye_score = face.detected && face.landmarks.size() >= 48
                             ? detectRedEye(hsv, face)
                             : 0.0;
        double hat_score = face.detected ? detectHatOrHeadAccessory(hsv, face, image.size()) : 0.0;
        double object_score = face.detected ? detectForegroundObject(hsv, gray, face, image.size()) : 0.0;
        double scratch_score = detectLongScratch(gray);

        result.actual_value = std::max(std::max(red_eye_score, hat_score),
                                       std::max(object_score, scratch_score));
        result.max_threshold = 1.0;
        result.detail = "red_eye=" + std::to_string(red_eye_score).substr(0, 5)
                      + " hat=" + std::to_string(hat_score).substr(0, 5)
                      + " object=" + std::to_string(object_score).substr(0, 5)
                      + " scratch=" + std::to_string(scratch_score).substr(0, 5);

        if (red_eye_score >= 1.0) {
            fail(result, "检测到明显红眼");
        } else if (hat_score >= 1.0) {
            fail(result, "检测到帽子/头部饰物遮挡");
        } else if (object_score >= 1.0) {
            fail(result, "检测到明显物体/划痕干扰");
        } else if (scratch_score >= 1.0) {
            fail(result, "检测到明显划痕/扫描线");
        } else if (result.actual_value >= 0.65) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "存在可疑可视缺陷，建议人工复核";
        } else {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "未发现明显红眼/遮挡/划痕";
        }
        return result;
    }

private:
    static void fail(CheckResult& result, const std::string& message) {
        result.passed = false;
        result.severity = Severity::FAIL;
        result.message = message;
    }

    static cv::Rect clampRect(const cv::Rect& r, const cv::Size& size) {
        return r & cv::Rect(0, 0, size.width, size.height);
    }

    static double redRatio(const cv::Mat& hsv_roi) {
        if (hsv_roi.empty()) return 0.0;
        int red = 0;
        for (int y = 0; y < hsv_roi.rows; ++y) {
            const cv::Vec3b* row = hsv_roi.ptr<cv::Vec3b>(y);
            for (int x = 0; x < hsv_roi.cols; ++x) {
                int h = row[x][0], s = row[x][1], v = row[x][2];
                if ((h < 10 || h > 170) && s > 70 && v > 70) red++;
            }
        }
        return static_cast<double>(red) / hsv_roi.total();
    }

    static double detectRedEye(const cv::Mat& hsv, const FaceInfo& face) {
        cv::Rect face_roi = clampRect(face.bbox, hsv.size());
        double face_red = face_roi.area() > 0 ? redRatio(hsv(face_roi)) : 0.0;
        auto eyeScore = [&](int start) -> double {
            float min_x = face.landmarks[start].x, max_x = face.landmarks[start].x;
            float min_y = face.landmarks[start].y, max_y = face.landmarks[start].y;
            for (int i = 1; i < 6; ++i) {
                min_x = std::min(min_x, face.landmarks[start + i].x);
                max_x = std::max(max_x, face.landmarks[start + i].x);
                min_y = std::min(min_y, face.landmarks[start + i].y);
                max_y = std::max(max_y, face.landmarks[start + i].y);
            }
            int ew = std::max(4, static_cast<int>(max_x - min_x));
            int eh = std::max(4, static_cast<int>(max_y - min_y));
            float cx = (min_x + max_x) * 0.5f;
            float cy = (min_y + max_y) * 0.5f;
            cv::Rect roi(static_cast<int>(cx - ew * 0.28f),
                         static_cast<int>(cy - eh * 0.55f),
                         std::max(4, static_cast<int>(ew * 0.56f)),
                         std::max(4, static_cast<int>(eh * 1.10f)));
            roi = clampRect(roi, hsv.size());
            if (roi.area() <= 0) return 0.0;
            double ratio = redRatio(hsv(roi));
            bool localized = ratio > face_red * 3.5 + 0.018;
            if (ratio > 0.080 && localized) return 1.0;
            return std::min(localized ? 0.80 : 0.40, ratio / 0.080);
        };
        double left = eyeScore(36);
        double right = eyeScore(42);
        if (left >= 1.0 && right >= 1.0) return 1.0;
        return std::max(left, right) * 0.80;
    }

    static double detectHatOrHeadAccessory(const cv::Mat& hsv, const FaceInfo& face, const cv::Size& size) {
        cv::Rect b = clampRect(face.bbox, size);
        if (b.area() <= 0) return 0.0;
        cv::Rect cap(b.x + b.width / 8,
                     std::max(0, b.y - b.height / 12),
                     b.width * 3 / 4,
                     b.height / 5);
        cap = clampRect(cap, size);
        if (cap.area() <= 0) return 0.0;

        cv::Mat roi = hsv(cap);
        int colored = 0;
        double s_sum = 0.0;
        for (int y = 0; y < roi.rows; ++y) {
            const cv::Vec3b* row = roi.ptr<cv::Vec3b>(y);
            for (int x = 0; x < roi.cols; ++x) {
                int h = row[x][0], s = row[x][1], v = row[x][2];
                s_sum += s;
                bool non_skin_color = ((h < 5 || h > 145) || (h > 35 && h < 130));
                if (s > 75 && v > 80 && non_skin_color) colored++;
            }
        }
        double colored_ratio = static_cast<double>(colored) / roi.total();
        double mean_s = s_sum / roi.total();
        if (colored_ratio > 0.24 && mean_s > 60.0) return 1.0;
        return std::min(0.95, colored_ratio / 0.24);
    }

    static double detectForegroundObject(const cv::Mat& hsv, const cv::Mat& gray,
                                         const FaceInfo& face, const cv::Size& size) {
        cv::Rect b = clampRect(face.bbox, size);
        if (b.area() <= 0) return 0.0;
        cv::Rect lower;
        if (face.landmarks.size() >= 68) {
            int x0 = static_cast<int>(std::min(face.landmarks[3].x, face.landmarks[48].x));
            int x1 = static_cast<int>(std::max(face.landmarks[13].x, face.landmarks[54].x));
            int y0 = static_cast<int>(std::min(face.landmarks[33].y, face.landmarks[51].y));
            int y1 = static_cast<int>(face.landmarks[8].y);
            lower = cv::Rect(x0, y0, x1 - x0, y1 - y0);
        } else {
            lower = cv::Rect(b.x + b.width / 5, b.y + b.height / 2,
                             b.width * 3 / 5, b.height * 2 / 5);
        }
        lower = clampRect(lower, size);
        if (lower.area() <= 0) return 0.0;

        cv::Mat roi_hsv = hsv(lower);
        int saturated = 0;
        for (int y = 0; y < roi_hsv.rows; ++y) {
            const cv::Vec3b* row = roi_hsv.ptr<cv::Vec3b>(y);
            for (int x = 0; x < roi_hsv.cols; ++x) {
                int h = row[x][0], s = row[x][1], v = row[x][2];
                bool blue_green = (h > 45 && h < 135);
                bool vivid_orange = (h > 20 && h < 35 && s > 130);
                if (s > 85 && v > 70 && (blue_green || vivid_orange)) saturated++;
            }
        }
        double color_ratio = static_cast<double>(saturated) / roi_hsv.total();

        cv::Mat edges;
        cv::Canny(gray(lower), edges, 50, 150);
        double edge_ratio = static_cast<double>(cv::countNonZero(edges)) / lower.area();
        if (color_ratio > 0.16 && edge_ratio > 0.035) return 1.0;
        return std::min(0.95, std::max(color_ratio / 0.16, edge_ratio / 0.100));
    }

    static double detectLongScratch(const cv::Mat& gray) {
        cv::Mat edges;
        cv::Canny(gray, edges, 60, 160);
        std::vector<cv::Vec4i> lines;
        cv::HoughLinesP(edges, lines, 1, CV_PI / 180, 45,
                        std::max(40, gray.rows / 3), 8);
        double best = 0.0;
        for (const auto& l : lines) {
            double dx = l[2] - l[0], dy = l[3] - l[1];
            double len = std::sqrt(dx * dx + dy * dy);
            double angle = std::abs(std::atan2(dy, dx) * 180.0 / CV_PI);
            angle = std::min(angle, 180.0 - angle);
            bool near_vertical = angle > 75.0;
            bool near_horizontal = angle < 15.0;
            double mx = (l[0] + l[2]) / 2.0;
            double my = (l[1] + l[3]) / 2.0;
            bool away_from_border = mx > gray.cols * 0.03 && mx < gray.cols * 0.97
                                 && my > gray.rows * 0.03 && my < gray.rows * 0.97;
            bool side_scan_line = mx < gray.cols * 0.28 || mx > gray.cols * 0.72;
            if ((near_vertical || near_horizontal) && away_from_border
                && side_scan_line && len > gray.rows * 0.45) {
                best = std::max(best, len / (gray.rows * 0.55));
            }
        }
        return std::min(1.0, best);
    }
};

REGISTER_PLUGIN(IQualityChecker, VisualDefectChecker, "visual_defect_check")

} // namespace photo
