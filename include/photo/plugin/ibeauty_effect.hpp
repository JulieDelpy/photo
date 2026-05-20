#pragma once

#include "iplugin.hpp"
#include "photo/core/common.hpp"
#include <map>

namespace photo {

// Interface for all beauty effect plugins.
// Each concrete effect transforms the face region in the image
// (e.g., skin smoothing, whitening, eye enlargement, face slimming).
class IBeautyEffect : public IPlugin {
public:
    const char* category() const override { return "beauty"; }

    // Apply the beauty effect to the image.
    //   image  - input/output image (modified in place)
    //   face   - pre-detected face info (bbox, landmarks)
    //   params - effect-specific parameters (strength, radius, etc.)
    // Returns true on success.
    virtual bool apply(cv::Mat& image,
                       const FaceInfo& face,
                       const ParamMap& params) = 0;

    // Return the default parameter set for this effect,
    // including valid ranges and descriptions.
    virtual std::map<std::string, ParamDef> defaultParams() const = 0;
};

} // namespace photo
