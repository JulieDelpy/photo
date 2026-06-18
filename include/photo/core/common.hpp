#pragma once

#include <string>
#include <vector>
#include <map>
#include <opencv2/core.hpp>

namespace photo {

// ============================================================
// Plugin categories
// ============================================================
enum class PluginCategory {
    QUALITY_CHECKER,
    BEAUTY_EFFECT
};

// ============================================================
// Check severity enum
// ============================================================
enum class Severity {
    PASS,
    WARNING,
    FAIL
};

// ============================================================
// Face detection info
// ============================================================
struct FaceInfo {
    bool detected = false;
    float confidence = 0.0f;

    // Bounding box (pixel coordinates)
    cv::Rect bbox{};

    // 68 facial landmarks (or empty if detector doesn't provide them)
    std::vector<cv::Point2f> landmarks;

    // Head pose angles (degrees) — from solvePnP
    float yaw = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;

    // Direct landmark-based measurements (more reliable than solvePnP roll/pitch)
    float eye_tilt = 0.0f;         // 两眼连线与水平线夹角(度)，正值=左眼高(逆时针)
    float nose_offset_ratio = 0.0f; // 鼻尖相对两眼中心的水平偏移比（用于判断偏头）
    float eye_rel_y = 0.0f;        // 眼睛在 face bbox 中的相对Y位置
    float chin_eye_ratio = 0.0f;   // (下巴Y-眼睛Y)/bbox.height
    float pitch_metric = 0.0f;     // (鼻尖→下巴)/(鼻梁→鼻尖), 抬头>3.0, 低头<1.5
    float eye_mouth_ratio = 0.0f;  // (嘴→眼)/(下巴→眼), 抬头<0.52, 低头>0.72

    // Eye Aspect Ratio
    float ear_left = 1.0f;
    float ear_right = 1.0f;

    // Mouth Aspect Ratio
    float mar = 0.0f;
};

// ============================================================
// Quality threshold for a single check item
// ============================================================
struct QualityThreshold {
    std::string name;
    double min_value = 0.0;
    double max_value = 0.0;
    bool has_min = false;
    bool has_max = false;
};

// ============================================================
// Configuration for a specific ID photo type
// ============================================================
struct IDPhotoStandard {
    std::string photo_type;        // "id_card_cn", "passport_cn", etc.
    std::string display_name;      // Human-readable name

    // File spec
    int expected_width = 0;
    int expected_height = 0;
    int expected_dpi = 300;
    std::vector<std::string> allowed_formats;
    int file_size_min_kb = 0;
    int file_size_max_kb = 0;

    // Face detection thresholds
    int min_face_count = 1;
    int max_face_count = 1;
    float min_face_confidence = 0.8f;
    float min_face_area_ratio = 0.1f;   // face_bbox_area / image_area
    float max_face_area_ratio = 0.45f;

    // Face state thresholds
    float ear_min = 0.2f;               // Eye Aspect Ratio minimum
    float mar_max = 0.6f;               // Mouth Aspect Ratio maximum
    float max_head_yaw = 10.0f;         // degrees
    float max_head_pitch = 10.0f;
    float max_head_roll = 5.0f;

    // Image quality thresholds
    float min_sharpness = 100.0f;        // Laplacian variance
    float exposure_min = 80.0f;
    float exposure_max = 220.0f;
    float min_rms_contrast = 30.0f;
    float max_shadow_ratio = 0.3f;
    float max_noise_stddev = 10.0f;
    int min_face_pixels = 120;
    int max_face_pixels = 0;

    // Composition thresholds
    float max_center_offset_pct = 5.0f;  // percentage
    float face_height_ratio_min = 0.44f;
    float face_height_ratio_max = 0.65f;
    float eye_position_ratio_min = 0.42f; // eye_y / image_height
    float top_margin_ratio_min = 0.15f;
    float top_margin_ratio_max = 0.40f;
    int bottom_margin_min_px = 7;
    int side_margin_min_px = 10;
    float max_tilt_angle = 5.0f;
    float face_aspect_ratio_min = 0.7f;
    float face_aspect_ratio_max = 1.0f;

    // Background thresholds
    int bg_expected_h_min = 0;           // HSV, 0-180
    int bg_expected_h_max = 180;
    int bg_expected_s_max = 30;          // Saturation max
    int bg_expected_v_min = 200;         // Value min
    float bg_uniformity_max = 15.0f;     // Standard deviation of color
    float bg_edge_density_max = 0.05f;

    // Active check list
    std::vector<std::string> checks;
};

// ============================================================
// Beauty effect parameter definition
// ============================================================
struct ParamDef {
    float default_value = 0.5f;
    float min_value = 0.0f;
    float max_value = 1.0f;
    std::string description;
};

// ============================================================
// Convenience: list of photo type names
// ============================================================
using ParamMap = std::map<std::string, float>;

// ============================================================
// Factory function type for plugin creation
// ============================================================
template <typename T>
using PluginFactory = std::function<std::unique_ptr<T>()>;

} // namespace photo
