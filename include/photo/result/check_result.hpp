#pragma once

#include <string>
#include <vector>
#include "photo/core/common.hpp"

namespace photo {

// Single check result from one quality checker plugin
struct CheckResult {
    std::string checker_name;       // Plugin name, e.g. "blur_check"
    std::string category;           // "face", "state", "quality", "composition", "background", "file"
    bool passed = false;
    Severity severity = Severity::PASS;

    double actual_value = 0.0;      // Measured value
    double min_threshold = 0.0;     // Minimum acceptable
    double max_threshold = 0.0;     // Maximum acceptable
    std::string message;            // Human-readable result summary
    std::string detail;             // Extended diagnostic info
};

// Full quality report aggregating all check results
struct QualityReport {
    std::string photo_type;         // e.g. "id_card_cn"
    std::string photo_display_name;
    std::string check_mode = "final"; // "raw" precheck or "final" strict acceptance
    bool overall_pass = false;

    int total_checks = 0;
    int passed_checks = 0;
    int warning_checks = 0;
    int failed_checks = 0;

    FaceInfo face_info;
    std::vector<CheckResult> results;
    std::string summary;

    // File metadata
    std::string file_path;
    int image_width = 0;
    int image_height = 0;
    size_t file_size_bytes = 0;
    std::string file_format;
};

} // namespace photo
