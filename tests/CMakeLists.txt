cmake_minimum_required(VERSION 3.16)

add_executable(test_plugins
    test_plugins.cpp
)
target_include_directories(test_plugins PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_plugins PRIVATE
    photo_core
    photo_quality
    photo_beauty
    GTest::gtest_main
)

add_test(NAME test_plugins COMMAND test_plugins)
