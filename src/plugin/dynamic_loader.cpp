#include <string>
#include <vector>
#include <iostream>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace photo {

// RAII wrapper around a dynamic library handle.
class DynamicLibrary {
public:
    explicit DynamicLibrary(const std::string& path) {
#ifdef _WIN32
        handle_ = LoadLibraryA(path.c_str());
#else
        handle_ = dlopen(path.c_str(), RTLD_NOW);
#endif
        if (!handle_) {
            std::cerr << "[PluginLoader] Failed to load: " << path << std::endl;
        }
    }

    ~DynamicLibrary() {
        if (handle_) {
#ifdef _WIN32
            FreeLibrary((HMODULE)handle_);
#else
            dlclose(handle_);
#endif
        }
    }

    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;
    DynamicLibrary(DynamicLibrary&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    DynamicLibrary& operator=(DynamicLibrary&& other) noexcept {
        if (this != &other) {
            if (handle_) {
#ifdef _WIN32
                FreeLibrary((HMODULE)handle_);
#else
                dlclose(handle_);
#endif
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    bool loaded() const { return handle_ != nullptr; }

    template <typename Func>
    Func getSymbol(const std::string& name) {
#ifdef _WIN32
        return reinterpret_cast<Func>(GetProcAddress((HMODULE)handle_, name.c_str()));
#else
        return reinterpret_cast<Func>(dlsym(handle_, name.c_str()));
#endif
    }

private:
    void* handle_ = nullptr;
};

} // namespace photo
