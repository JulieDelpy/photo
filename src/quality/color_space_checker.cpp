#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/plugin_macros.hpp"

namespace photo {

class ColorSpaceChecker : public IQualityChecker {
public:
    const char* name() const override { return "color_space_check"; }
    const char* version() const override { return "1.0.0"; }

    CheckResult check(const cv::Mat& image, const FaceInfo&,
                      const IDPhotoStandard&) override {
        CheckResult result;
        result.checker_name = name();
        result.category = "file";

        int channels = image.channels();
        result.actual_value = static_cast<double>(channels);

        if (channels == 3) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Color space: RGB (3 channels)";
        } else if (channels == 1) {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Image is grayscale; color photo required";
        } else if (channels == 4) {
            result.passed = true;
            result.severity = Severity::PASS;
            result.message = "Color space: RGBA (4 channels)";
        } else {
            result.passed = false;
            result.severity = Severity::WARNING;
            result.message = "Unknown color space: " + std::to_string(channels) + " channels";
        }
        return result;
    }
};

REGISTER_PLUGIN(IQualityChecker, ColorSpaceChecker, "color_space_check")

} // namespace photo
