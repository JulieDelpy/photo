cmake_minimum_required(VERSION 3.16)
project(photo LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(PHOTO_BUILD_TESTS "Build unit tests" ON)
option(PHOTO_BUILD_PLUGINS_AS_SHARED "Build plugins as shared libraries" OFF)

include(cmake/Dependencies.cmake)

# Core library (types, plugin framework, pipeline)
add_library(photo_core STATIC
    src/core/image.cpp
    src/core/face_analyzer.cpp
    src/plugin/dynamic_loader.cpp
)
target_include_directories(photo_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(photo_core PUBLIC
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    opencv_objdetect
    opencv_dnn
    opencv_calib3d
    opencv_face
    nlohmann_json::nlohmann_json
)

# Quality checker plugins (OBJECT library preserves static constructors)
add_library(photo_quality OBJECT
    src/quality/blur_checker.cpp
    src/quality/exposure_checker.cpp
    src/quality/contrast_checker.cpp
    src/quality/shadow_checker.cpp
    src/quality/noise_checker.cpp
    src/quality/resolution_checker.cpp
    src/quality/overexposure_checker.cpp
    src/quality/underexposure_checker.cpp
    src/quality/sharpness_checker.cpp
    src/quality/jpeg_artifact_checker.cpp
    src/quality/image_blur_checker.cpp
    src/quality/image_exposure_checker.cpp
    src/quality/image_contrast_checker.cpp
    src/quality/image_noise_checker.cpp
    src/quality/face_detect_checker.cpp
    src/quality/face_count_checker.cpp
    src/quality/face_confidence_checker.cpp
    src/quality/face_size_checker.cpp
    src/quality/face_integrity_checker.cpp
    src/quality/eye_closure_checker.cpp
    src/quality/mouth_open_checker.cpp
    src/quality/head_pose_checker.cpp
    src/quality/centering_checker.cpp
    src/quality/face_ratio_checker.cpp
    src/quality/eye_position_checker.cpp
    src/quality/head_margin_checker.cpp
    src/quality/chin_margin_checker.cpp
    src/quality/side_margin_checker.cpp
    src/quality/head_tilt_checker.cpp
    src/quality/face_aspect_ratio_checker.cpp
    src/quality/background_color_checker.cpp
    src/quality/background_uniformity_checker.cpp
    src/quality/background_texture_checker.cpp
    src/quality/background_edge_checker.cpp
    src/quality/file_dimension_checker.cpp
    src/quality/file_aspect_ratio_checker.cpp
    src/quality/file_size_checker.cpp
    src/quality/file_format_checker.cpp
    src/quality/color_space_checker.cpp
    src/quality/dpi_checker.cpp
)
target_include_directories(photo_quality PUBLIC ${CMAKE_SOURCE_DIR}/include)
# Propagate OpenCV/photocore include dirs to OBJECT library sources
target_link_libraries(photo_quality PUBLIC photo_core opencv_core opencv_imgproc)

# Beauty effect plugins (OBJECT library preserves static constructors)
add_library(photo_beauty OBJECT
    src/beauty/skin_smoothing.cpp
    src/beauty/skin_whitening.cpp
    src/beauty/eye_enlargement.cpp
    src/beauty/face_slimming.cpp
)
target_include_directories(photo_beauty PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(photo_beauty PUBLIC photo_core opencv_core opencv_imgproc)

# CLI executable - includes object files directly to preserve static registrations
add_executable(photo_tool
    src/cli/main.cpp
    $<TARGET_OBJECTS:photo_quality>
    $<TARGET_OBJECTS:photo_beauty>
)
target_link_libraries(photo_tool PRIVATE photo_core)

# Tests
if(PHOTO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
