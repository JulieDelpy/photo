#pragma once

#include "plugin_manager.hpp"

// Compile-time plugin registration macro.
//
// Place at file scope in a .cpp file:
//   REGISTER_PLUGIN(IQualityChecker, BlurChecker, "blur_check")
//
// This creates a static object whose constructor calls
// PluginManager<InterfaceType>::instance().registerPlugin(...)
// before main() runs.
//
// The anonymous namespace + static variable pattern ensures
// each translation unit gets its own registrar without ODR conflicts.

#define REGISTER_PLUGIN(InterfaceType, PluginClass, PluginName)        \
    namespace {                                                         \
        struct Register_##PluginClass {                                 \
            Register_##PluginClass() {                                  \
                ::photo::PluginManager<InterfaceType>::instance()        \
                    .registerPlugin(PluginName, []() {                  \
                        return std::make_unique<PluginClass>();          \
                    });                                                  \
            }                                                            \
        };                                                               \
        static Register_##PluginClass g_register_##PluginClass;          \
    }

// ============================================================
// Dynamic plugin export macros (for .so/.dll plugins)
// ============================================================

#if defined(_WIN32) || defined(_WIN64)
    #define PHOTO_PLUGIN_API __declspec(dllexport)
#else
    #define PHOTO_PLUGIN_API __attribute__((visibility("default")))
#endif

// Place in a plugin .cpp to export the init function.
// Usage: PHOTO_PLUGIN_INIT { return 0; }
#define PHOTO_PLUGIN_INIT                                \
    extern "C" PHOTO_PLUGIN_API int photo_plugin_init()
