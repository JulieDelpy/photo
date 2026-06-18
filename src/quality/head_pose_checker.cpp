#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>

namespace photo {

class HeadPoseChecker : public IQualityChecker {
public:
    const char* name() const override { return "head_pose_check"; }
    const char* version() const override { return "1.3.1"; }

    CheckResult check(const cv::Mat& image, const FaceInfo& face,
                      const IDPhotoStandard& cfg) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "state";

        if (!face.detected) {
            result.passed = false;
            result.severity = Severity::FAIL;
            result.message = "未检测到人脸，无法检查头部姿态";
            return result;
        }

        double max_yaw   = cfg.max_head_yaw > 0 ? cfg.max_head_yaw : 10.0;
        double max_pitch = cfg.max_head_pitch > 0 ? cfg.max_head_pitch : 9.0;
        double max_roll  = cfg.max_head_roll > 0 ? cfg.max_head_roll : 7.5;

        // Roll: eye_tilt（两眼连线角度），比 solvePnP roll 可靠
        double tilt = std::abs(face.eye_tilt);
        bool roll_ok = (tilt <= max_roll);

        // Yaw: solvePnP yaw + 鼻尖水平偏移比
        double yaw_abs = std::abs(face.yaw);
        double nose_off = std::abs(face.nose_offset_ratio);
        bool yaw_solver_reliable = (yaw_abs <= 90.0) || (nose_off > 0.20);
        bool yaw_ok = (nose_off <= 0.20)
                   && (!yaw_solver_reliable || yaw_abs <= max_yaw);

        // 检测下巴是否接近图像底边或被裁切，此时 pm/emr 不可靠
        bool chin_cropped = false;
        if (face.landmarks.size() >= 9) {
            float chin_y = face.landmarks[8].y;
            double chin_ratio = static_cast<double>(chin_y) / image.rows;
            chin_cropped = (chin_ratio > 0.92);
        }
        // bbox 宽高比：抬头时面部被纵向压缩，w/h 偏高
        double bbox_ar = face.bbox.height > 0
            ? static_cast<double>(face.bbox.width) / face.bbox.height : 1.0;
        // 鼻子压缩比：抬头时鼻梁→鼻尖在 2D 上被透视缩短
        double nose_ratio = 0;
        if (face.landmarks.size() >= 31 && face.bbox.height > 0) {
            double nose_len = cv::norm(face.landmarks[30] - face.landmarks[27]);
            nose_ratio = nose_len / face.bbox.height;
        }
        bool face_squashed = (bbox_ar > 0.78 && nose_ratio < 0.20);

        // Pitch: solvePnP pitch + 2D 比例
        double pitch_abs = std::abs(face.pitch);
        double pm  = face.pitch_metric;
        double emr = face.eye_mouth_ratio;
        bool chin_up_2d = (pm >= 2.35) && (face.eye_rel_y <= 0.42) && (emr >= 0.55);
        double eff_max_pitch = max_pitch;
        if (!chin_cropped && !face_squashed
            && !chin_up_2d
            && pm >= 1.55 && pm <= 2.70 && emr >= 0.52 && emr <= 0.70)
            eff_max_pitch = 999.0;
        bool pitch_ok;
        if (chin_up_2d) {
            pitch_ok = false;
        } else if (chin_cropped || face_squashed) {
            pitch_ok = (pitch_abs <= max_pitch);
        } else {
            pitch_ok = (pitch_abs <= eff_max_pitch)
                     && (pm >= 1.40) && (pm <= 3.20)
                     && (emr >= 0.48) && (emr <= 0.74);
        }

        double max_angle = std::max({yaw_abs, pitch_abs, tilt});
        result.actual_value = max_angle;
        result.max_threshold = std::max({max_yaw, max_pitch, max_roll});
        result.detail = "yaw=" + f(yaw_abs) + " pitch=" + f(pitch_abs)
                      + " tilt=" + f(tilt)
                      + " pm=" + f(pm) + " emr=" + f(emr)
                      + " noff=" + f(nose_off)
                      + " yaw_reliable=" + std::to_string(yaw_solver_reliable ? 1 : 0)
                      + " eye_y=" + f(face.eye_rel_y)
                      + " chin_eye=" + f(face.chin_eye_ratio)
                      + " chin_up=" + std::to_string(chin_up_2d ? 1 : 0);

        if (yaw_ok && pitch_ok && roll_ok) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "头部姿态正常: yaw=" + f(yaw_abs)
                           + " pitch=" + f(pitch_abs)
                           + " tilt=" + f(tilt);
        } else {
            result.passed = false;
            result.severity = Severity::FAIL;
            if (!yaw_ok)
                result.message = "头部左右偏转过大 (yaw=" + f(yaw_abs)
                               + " noff=" + f(nose_off) + ")";
            else if (!pitch_ok)
                result.message = "头部俯仰过大 (pitch=" + f(pitch_abs)
                               + " pm=" + f(pm) + " emr=" + f(emr) + ")";
            else
                result.message = "头部倾斜 (tilt=" + f(tilt) + ")";
        }
        return result;
    }

private:
    static std::string f(double v) {
        return std::to_string(static_cast<int>(v * 10) / 10.0).substr(0, 5);
    }
};

// v2: eye_tilt + nose_offset_ratio + pitch_metric + eye_mouth_ratio
REGISTER_PLUGIN(IQualityChecker, HeadPoseChecker, "head_pose_check")

} // namespace photo
