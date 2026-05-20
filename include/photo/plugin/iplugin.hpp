#pragma once

#include <string>

namespace photo {

// Base class for all plugins in the system.
// Every quality checker and beauty effect inherits from this.
class IPlugin {
public:
    virtual ~IPlugin() = default;

    // Unique name used for registration and lookup (e.g. "blur_check", "skin_smoothing")
    virtual const char* name() const = 0;

    // Semantic version string
    virtual const char* version() const = 0;

    // Plugin category for grouping
    virtual const char* category() const = 0;  // "quality" or "beauty"
};

} // namespace photo
