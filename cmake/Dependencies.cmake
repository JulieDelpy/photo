include(FetchContent)

# OpenCV
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs objdetect dnn calib3d face)

# nlohmann/json (header-only, local copy)
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_SOURCE_DIR}/include/nlohmann)
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)

# cpp-httplib (header-only HTTP/HTTPS server)
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_COMPILE ON CACHE BOOL "" FORCE)
FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.5
)
FetchContent_MakeAvailable(httplib)

# Google Test
if(PHOTO_BUILD_TESTS)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
endif()
