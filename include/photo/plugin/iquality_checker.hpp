#pragma once

#include "iplugin.hpp"
#include "photo/core/common.hpp"
#include "photo/result/check_result.hpp"

namespace photo {

// Interface for all quality checker plugins.
// Each concrete checker evaluates one specific aspect of a photo
// (e.g., blur, exposure, face centering, background color).
class IQualityChecker : public IPlugin {
public:
    const char* category() const override { return "quality"; }

    // Execute the quality check.
    //   image    - the full input image (BGR)
    //   face     - pre-detected face info (bbox, landmarks, pose)
    //   standard - the ID photo standard thresholds to check against
    // Returns a CheckResult describing pass/fail with measured values.
    virtual CheckResult check(const cv::Mat& image,
                              const FaceInfo& face,
                              const IDPhotoStandard& standard) = 0;
};

} // namespace photo
