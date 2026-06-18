#include <gtest/gtest.h>
#include <fstream>
#include "photo/plugin/plugin_manager.hpp"
#include "photo/core/face_analyzer.hpp"
#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/ibeauty_effect.hpp"
#include "photo/core/common.hpp"
#include "photo/result/check_result.hpp"
#include "photo/result/quality_report.hpp"

// Test that PluginManager singleton works
TEST(PluginManagerTest, SingletonReturnsSameInstance) {
    auto& mgr1 = photo::PluginManager<photo::IQualityChecker>::instance();
    auto& mgr2 = photo::PluginManager<photo::IQualityChecker>::instance();
    EXPECT_EQ(&mgr1, &mgr2);
}

// Test that plugins are auto-registered from static constructors
TEST(PluginManagerTest, QualityCheckersAreRegistered) {
    auto& mgr = photo::PluginManager<photo::IQualityChecker>::instance();
    auto names = mgr.availablePlugins();

    EXPECT_GT(names.size(), 0) << "At least one quality checker should be registered";

    // Verify some known plugins
    EXPECT_TRUE(mgr.hasPlugin("blur_check"));
    EXPECT_TRUE(mgr.hasPlugin("exposure_check"));
    EXPECT_TRUE(mgr.hasPlugin("face_detect_check"));
}

// Test that beauty effects are registered
TEST(PluginManagerTest, BeautyEffectsAreRegistered) {
    auto& mgr = photo::PluginManager<photo::IBeautyEffect>::instance();
    auto names = mgr.availablePlugins();

    EXPECT_GT(names.size(), 0) << "At least one beauty effect should be registered";

    EXPECT_TRUE(mgr.hasPlugin("skin_smoothing"));
    EXPECT_TRUE(mgr.hasPlugin("skin_whitening"));
    EXPECT_TRUE(mgr.hasPlugin("eye_enlargement"));
    EXPECT_TRUE(mgr.hasPlugin("face_slimming"));
}

// Test plugin creation
TEST(PluginManagerTest, CanCreateQualityChecker) {
    auto& mgr = photo::PluginManager<photo::IQualityChecker>::instance();
    auto checker = mgr.create("blur_check");

    EXPECT_NE(checker, nullptr);
    EXPECT_STREQ(checker->name(), "blur_check");
    EXPECT_STREQ(checker->version(), "1.0.0");
    EXPECT_STREQ(checker->category(), "quality");
}

// Test plugin creation for beauty effects
TEST(PluginManagerTest, CanCreateBeautyEffect) {
    auto& mgr = photo::PluginManager<photo::IBeautyEffect>::instance();
    auto effect = mgr.create("skin_smoothing");

    EXPECT_NE(effect, nullptr);
    EXPECT_STREQ(effect->name(), "skin_smoothing");
    EXPECT_STREQ(effect->category(), "beauty");

    auto params = effect->defaultParams();
    EXPECT_GE(params.size(), 1);
}

// Test that unknown plugin throws
TEST(PluginManagerTest, UnknownPluginThrows) {
    auto& mgr = photo::PluginManager<photo::IQualityChecker>::instance();
    EXPECT_THROW(mgr.create("nonexistent_plugin"), std::runtime_error);
}

// Test tryCreate returns nullptr for unknown
TEST(PluginManagerTest, TryCreateReturnsNullForUnknown) {
    auto& mgr = photo::PluginManager<photo::IQualityChecker>::instance();
    auto checker = mgr.tryCreate("nonexistent_plugin");
    EXPECT_EQ(checker, nullptr);
}

// Test QualityReport JSON serialization
TEST(ResultTest, QualityReportToJson) {
    photo::QualityReport report;
    report.photo_type = "id_card_cn";
    report.photo_display_name = "Test";
    report.overall_pass = true;
    report.total_checks = 2;
    report.passed_checks = 2;

    photo::CheckResult r;
    r.checker_name = "blur_check";
    r.passed = true;
    r.severity = photo::Severity::PASS;
    r.actual_value = 150.0;
    r.message = "OK";
    report.results.push_back(r);

    nlohmann::json j = report;
    EXPECT_EQ(j["photo_type"], "id_card_cn");
    EXPECT_EQ(j["total_checks"], 2);
    EXPECT_EQ(j["results"].size(), 1);
}

// Test IDPhotoStandard JSON deserialization
TEST(ConfigTest, ParseIDCardStandard) {
    std::ifstream f("configs/id_card_cn.json");
    if (!f.is_open()) {
        GTEST_SKIP() << "Config file not found";
        return;
    }

    nlohmann::json j;
    f >> j;
    auto standard = j.get<photo::IDPhotoStandard>();

    EXPECT_EQ(standard.photo_type, "id_card_cn");
    EXPECT_EQ(standard.expected_width, 358);
    EXPECT_EQ(standard.expected_height, 441);
    EXPECT_GT(standard.checks.size(), 10);
}

// Test face analyzer on a dummy image
TEST(FaceAnalyzerTest, EmptyImageReturnsNoFace) {
    cv::Mat empty;
    photo::FaceAnalyzer analyzer;
    photo::FaceInfo face = analyzer.detect(empty);
    EXPECT_FALSE(face.detected);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
