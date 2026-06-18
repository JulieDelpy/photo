#pragma once

#include "check_result.hpp"
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>

namespace photo {

enum class CheckMode {
    FINAL,
    RAW
};

inline std::string toString(CheckMode mode) {
    return mode == CheckMode::RAW ? "raw" : "final";
}

inline CheckMode parseCheckMode(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (value == "raw" || value == "precheck" || value == "source") {
        return CheckMode::RAW;
    }
    return CheckMode::FINAL;
}

inline bool isRawModeSoftFail(const std::string& checker_name) {
    static const std::unordered_set<std::string> kSoftInRaw = {
        "face_size_check",
        "centering_check",
        "head_margin_check",
        "background_color_check",
        "face_skin_tone_check"
    };
    return kSoftInRaw.find(checker_name) != kSoftInRaw.end();
}

inline void applyCheckMode(CheckResult& result, CheckMode mode) {
    if (mode != CheckMode::RAW || result.passed || result.severity != Severity::FAIL) {
        return;
    }

    if (isRawModeSoftFail(result.checker_name)) {
        result.severity = Severity::WARNING;
        if (result.detail.empty()) {
            result.detail = "raw_check downgraded from fail because this item can be fixed during final crop/background processing";
        } else {
            result.detail += "; raw_check downgraded from fail because this item can be fixed during final crop/background processing";
        }
    }
}

} // namespace photo
