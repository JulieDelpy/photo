#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <stdexcept>
#include <functional>

namespace photo {

// Thread-safe singleton plugin registry.
// PluginInterface must inherit from IPlugin.
//
// Usage:
//   auto& mgr = PluginManager<IQualityChecker>::instance();
//   mgr.registerPlugin("blur_check", []() { return std::make_unique<BlurChecker>(); });
//   auto checker = mgr.create("blur_check");
template <typename PluginInterface>
class PluginManager {
public:
    using FactoryFn = std::function<std::unique_ptr<PluginInterface>()>;

    static PluginManager& instance() {
        static PluginManager mgr;
        return mgr;
    }

    // Register a plugin factory under a given name.
    // Thread-safe: may be called from static constructors before main().
    void registerPlugin(const std::string& name, FactoryFn factory) {
        std::lock_guard<std::mutex> lock(mutex_);
        factories_[name] = std::move(factory);
    }

    // Create a plugin instance by name.
    // Throws std::runtime_error if the name is not registered.
    std::unique_ptr<PluginInterface> create(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = factories_.find(name);
        if (it == factories_.end()) {
            throw std::runtime_error("Plugin not found: " + name);
        }
        return it->second();
    }

    // Create if registered, return nullptr otherwise (no-throw version).
    std::unique_ptr<PluginInterface> tryCreate(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = factories_.find(name);
        if (it != factories_.end()) {
            return it->second();
        }
        return nullptr;
    }

    // List all registered plugin names.
    std::vector<std::string> availablePlugins() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(factories_.size());
        for (const auto& [name, _] : factories_) {
            names.push_back(name);
        }
        return names;
    }

    // Check if a plugin is registered.
    bool hasPlugin(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return factories_.find(name) != factories_.end();
    }

    // Remove a plugin (useful for testing or hot-reload).
    void unregisterPlugin(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        factories_.erase(name);
    }

    // Number of registered plugins.
    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return factories_.size();
    }

private:
    PluginManager() = default;
    mutable std::mutex mutex_;
    std::map<std::string, FactoryFn> factories_;
};

} // namespace photo
