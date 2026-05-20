#pragma once

#include "check_result.hpp"
#include <nlohmann/json.hpp>

namespace photo {

// JSON serialization for CheckResult
inline void to_json(nlohmann::json& j, const CheckResult& r) {
    j = nlohmann::json{
        {"checker_name", r.checker_name},
        {"category", r.category},
        {"passed", r.passed},
        {"severity", (r.severity == Severity::PASS ? "pass" :
                      r.severity == Severity::WARNING ? "warning" : "fail")},
        {"actual_value", r.actual_value},
        {"min_threshold", r.min_threshold},
        {"max_threshold", r.max_threshold},
        {"message", r.message},
        {"detail", r.detail}
    };
}

// JSON serialization for FaceInfo
inline void to_json(nlohmann::json& j, const FaceInfo& f) {
    j = nlohmann::json{
        {"detected", f.detected},
        {"confidence", f.confidence},
        {"bbox", {{"x", f.bbox.x}, {"y", f.bbox.y},
                   {"width", f.bbox.width}, {"height", f.bbox.height}}},
        {"yaw", f.yaw},
        {"pitch", f.pitch},
        {"roll", f.roll},
        {"ear_left", f.ear_left},
        {"ear_right", f.ear_right},
        {"mar", f.mar},
        {"landmark_count", f.landmarks.size()}
    };
}

// JSON serialization for QualityReport
inline void to_json(nlohmann::json& j, const QualityReport& r) {
    j = nlohmann::json{
        {"photo_type", r.photo_type},
        {"photo_display_name", r.photo_display_name},
        {"overall_pass", r.overall_pass},
        {"total_checks", r.total_checks},
        {"passed_checks", r.passed_checks},
        {"warning_checks", r.warning_checks},
        {"failed_checks", r.failed_checks},
        {"face_info", r.face_info},
        {"results", r.results},
        {"summary", r.summary},
        {"file_info", {
            {"path", r.file_path},
            {"width", r.image_width},
            {"height", r.image_height},
            {"file_size_bytes", r.file_size_bytes},
            {"format", r.file_format}
        }}
    };
}

// JSON deserialization for IDPhotoStandard
inline void from_json(const nlohmann::json& j, IDPhotoStandard& s) {
    j.at("photo_type").get_to(s.photo_type);
    j.at("display_name").get_to(s.display_name);

    if (j.contains("file_spec")) {
        auto& fs = j["file_spec"];
        fs.at("width").get_to(s.expected_width);
        fs.at("height").get_to(s.expected_height);
        if (fs.contains("dpi")) fs.at("dpi").get_to(s.expected_dpi);
        if (fs.contains("allowed_formats")) fs.at("allowed_formats").get_to(s.allowed_formats);
        if (fs.contains("file_size_min_kb")) fs.at("file_size_min_kb").get_to(s.file_size_min_kb);
        if (fs.contains("file_size_max_kb")) fs.at("file_size_max_kb").get_to(s.file_size_max_kb);
    }

    if (j.contains("face_detection")) {
        auto& fd = j["face_detection"];
        if (fd.contains("min_face_count")) fd.at("min_face_count").get_to(s.min_face_count);
        if (fd.contains("max_face_count")) fd.at("max_face_count").get_to(s.max_face_count);
        if (fd.contains("min_confidence")) fd.at("min_confidence").get_to(s.min_face_confidence);
        if (fd.contains("min_face_area_ratio")) fd.at("min_face_area_ratio").get_to(s.min_face_area_ratio);
    }

    if (j.contains("face_state")) {
        auto& fs = j["face_state"];
        if (fs.contains("ear_min")) fs.at("ear_min").get_to(s.ear_min);
        if (fs.contains("mar_max")) fs.at("mar_max").get_to(s.mar_max);
        if (fs.contains("max_head_yaw")) fs.at("max_head_yaw").get_to(s.max_head_yaw);
        if (fs.contains("max_head_pitch")) fs.at("max_head_pitch").get_to(s.max_head_pitch);
        if (fs.contains("max_head_roll")) fs.at("max_head_roll").get_to(s.max_head_roll);
    }

    if (j.contains("quality")) {
        auto& q = j["quality"];
        if (q.contains("min_sharpness")) q.at("min_sharpness").get_to(s.min_sharpness);
        if (q.contains("exposure_min")) q.at("exposure_min").get_to(s.exposure_min);
        if (q.contains("exposure_max")) q.at("exposure_max").get_to(s.exposure_max);
        if (q.contains("min_rms_contrast")) q.at("min_rms_contrast").get_to(s.min_rms_contrast);
        if (q.contains("max_shadow_ratio")) q.at("max_shadow_ratio").get_to(s.max_shadow_ratio);
        if (q.contains("max_noise_stddev")) q.at("max_noise_stddev").get_to(s.max_noise_stddev);
        if (q.contains("min_face_pixels")) q.at("min_face_pixels").get_to(s.min_face_pixels);
    }

    if (j.contains("composition")) {
        auto& c = j["composition"];
        if (c.contains("max_center_offset_pct")) c.at("max_center_offset_pct").get_to(s.max_center_offset_pct);
        if (c.contains("face_height_ratio_min")) c.at("face_height_ratio_min").get_to(s.face_height_ratio_min);
        if (c.contains("face_height_ratio_max")) c.at("face_height_ratio_max").get_to(s.face_height_ratio_max);
        if (c.contains("eye_position_ratio_min")) c.at("eye_position_ratio_min").get_to(s.eye_position_ratio_min);
        if (c.contains("top_margin_min_px")) c.at("top_margin_min_px").get_to(s.top_margin_min_px);
        if (c.contains("top_margin_max_px")) c.at("top_margin_max_px").get_to(s.top_margin_max_px);
        if (c.contains("bottom_margin_min_px")) c.at("bottom_margin_min_px").get_to(s.bottom_margin_min_px);
        if (c.contains("side_margin_min_px")) c.at("side_margin_min_px").get_to(s.side_margin_min_px);
        if (c.contains("max_tilt_angle")) c.at("max_tilt_angle").get_to(s.max_tilt_angle);
        if (c.contains("face_aspect_ratio_min")) c.at("face_aspect_ratio_min").get_to(s.face_aspect_ratio_min);
        if (c.contains("face_aspect_ratio_max")) c.at("face_aspect_ratio_max").get_to(s.face_aspect_ratio_max);
    }

    if (j.contains("background")) {
        auto& b = j["background"];
        if (b.contains("expected_h_min")) b.at("expected_h_min").get_to(s.bg_expected_h_min);
        if (b.contains("expected_h_max")) b.at("expected_h_max").get_to(s.bg_expected_h_max);
        if (b.contains("expected_s_max")) b.at("expected_s_max").get_to(s.bg_expected_s_max);
        if (b.contains("expected_v_min")) b.at("expected_v_min").get_to(s.bg_expected_v_min);
        if (b.contains("uniformity_max")) b.at("uniformity_max").get_to(s.bg_uniformity_max);
        if (b.contains("edge_density_max")) b.at("edge_density_max").get_to(s.bg_edge_density_max);
    }

    if (j.contains("checks")) {
        j.at("checks").get_to(s.checks);
    }
}

} // namespace photo
