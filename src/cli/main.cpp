#include "photo/core/face_analyzer.hpp"
#include "photo/core/image.hpp"
#include "photo/plugin/plugin_manager.hpp"
#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/ibeauty_effect.hpp"
#include "photo/result/check_mode.hpp"
#include "photo/result/quality_report.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;
using json = nlohmann::json;

// ============================================================
// All quality checker plugins are registered via REGISTER_PLUGIN
// macros in their .cpp files. Since we link statically, the
// static constructors fire before main().
// ============================================================

void printUsage(const char* prog) {
    std::cout << "Photo Processing Tool v1.0.0\n\n"
              << "Usage:\n"
              << "  " << prog << " --check <image> [--standard <json_config>] [--check-mode raw|final]\n"
              << "      Run quality evaluation on a single image.\n\n"
              << "  " << prog << " --beautify <image> --effect <name> [--params <k=v,...>] [--standard <json>]\n"
              << "      Apply a beauty effect to an image.\n\n"
              << "  " << prog << " --batch <directory> [--standard <json_config>] [--output <dir>] [--check-mode raw|final]\n"
              << "      Batch process all images in a directory.\n\n"
              << "  " << prog << " --list-checkers\n"
              << "      List all registered quality checkers.\n\n"
              << "  " << prog << " --list-effects\n"
              << "      List all registered beauty effects.\n\n"
              << "Options:\n"
              << "  --standard <json>  Path to ID photo standard config (default: configs/id_card_cn.json)\n"
              << "  --effect <name>    Beauty effect name\n"
              << "  --params <k=v,..>  Comma-separated key=value parameters\n"
              << "  --output <dir>     Output directory for batch processing\n"
              << "  --check-mode <m>   final=strict ID photo acceptance, raw=source photo precheck\n"
              << std::endl;
}

photo::IDPhotoStandard loadStandard(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Warning: Cannot open config '" << path
                  << "', using default standard\n";
        photo::IDPhotoStandard s;
        s.photo_type = "default";
        s.display_name = "Default Standard";
        return s;
    }
    json j;
    f >> j;
    return j.get<photo::IDPhotoStandard>();
}

photo::ParamMap parseParams(const std::string& param_str) {
    photo::ParamMap params;
    if (param_str.empty()) return params;

    std::istringstream ss(param_str);
    std::string token;
    while (std::getline(ss, token, ',')) {
        auto eq = token.find('=');
        if (eq != std::string::npos) {
            std::string key = token.substr(0, eq);
            float value = std::stof(token.substr(eq + 1));
            params[key] = value;
        }
    }
    return params;
}

std::vector<std::string> configuredCheckerNames(
    const photo::IDPhotoStandard& standard,
    const photo::PluginManager<photo::IQualityChecker>& mgr) {
    if (standard.checks.empty()) {
        return mgr.availablePlugins();
    }

    std::vector<std::string> names;
    names.reserve(standard.checks.size());
    for (const auto& name : standard.checks) {
        if (mgr.hasPlugin(name)) {
            names.push_back(name);
        } else {
            std::cerr << "Warning: configured checker not registered: "
                      << name << "\n";
        }
    }
    return names;
}

void runCheck(const std::string& image_path, const photo::IDPhotoStandard& standard,
              photo::CheckMode check_mode) {
    // Load image
    photo::Image img(image_path);
    if (img.empty()) {
        std::cerr << "Error: Cannot load image: " << image_path << std::endl;
        return;
    }

    // Face detection
    photo::FaceAnalyzer analyzer;
    photo::FaceInfo face = analyzer.detect(img.mat());

    // Run configured quality checkers in config order.
    photo::QualityReport report;
    report.photo_type = standard.photo_type;
    report.photo_display_name = standard.display_name;
    report.check_mode = photo::toString(check_mode);
    report.file_path = image_path;
    report.image_width = img.width();
    report.image_height = img.height();
    report.file_size_bytes = img.fileSize();
    report.face_info = face;

    auto& mgr = photo::PluginManager<photo::IQualityChecker>::instance();
    auto checker_names = configuredCheckerNames(standard, mgr);

    std::cout << "Running " << checker_names.size() << " quality checks on: "
              << image_path << "\n"
              << "Check mode: " << report.check_mode << "\n\n";

    for (const auto& name : checker_names) {
        auto checker = mgr.tryCreate(name);
        if (!checker) continue;

        auto result = checker->check(img.mat(), face, standard);
        photo::applyCheckMode(result, check_mode);
        report.results.push_back(result);

        std::string status;
        if (result.passed) status = "[PASS]";
        else if (result.severity == photo::Severity::WARNING) status = "[WARN]";
        else status = "[FAIL]";
        std::cout << status << " " << result.checker_name << ": "
                  << result.message << std::endl;

        if (result.passed) report.passed_checks++;
        else if (result.severity == photo::Severity::WARNING) report.warning_checks++;
        else report.failed_checks++;
    }

    report.total_checks = static_cast<int>(report.results.size());
    report.overall_pass = (report.failed_checks == 0);

    std::cout << "\n--- Summary ---\n"
              << "Total: " << report.total_checks
              << " | Passed: " << report.passed_checks
              << " | Warnings: " << report.warning_checks
              << " | Failed: " << report.failed_checks << "\n"
              << "Overall: " << (report.overall_pass ? "PASS" : "FAIL") << "\n";

    // Output JSON
    json j = report;
    std::string json_file = image_path + ".report.json";
    std::ofstream of(json_file);
    of << j.dump(2) << std::endl;
    std::cout << "\nReport saved to: " << json_file << std::endl;
}

void runBeautify(const std::string& image_path, const std::string& effect_name,
                 const photo::ParamMap& params, const photo::IDPhotoStandard&) {
    auto& mgr = photo::PluginManager<photo::IBeautyEffect>::instance();
    auto effect = mgr.tryCreate(effect_name);
    if (!effect) {
        std::cerr << "Error: Unknown effect '" << effect_name << "'\n";
        std::cerr << "Available: ";
        for (const auto& n : mgr.availablePlugins()) {
            std::cerr << n << " ";
        }
        std::cerr << std::endl;
        return;
    }

    photo::Image img(image_path);
    if (img.empty()) {
        std::cerr << "Error: Cannot load image: " << image_path << std::endl;
        return;
    }

    // Face detection
    photo::FaceAnalyzer analyzer;
    photo::FaceInfo face = analyzer.detect(img.mat());
    if (!face.detected) {
        std::cerr << "Error: No face detected in: " << image_path << std::endl;
        return;
    }

    // Apply beauty effect
    std::cout << "Applying effect '" << effect_name << "'...\n";
    if (effect->apply(img.mat(), face, params)) {
        std::string out_path = image_path + ".beautified.jpg";
        img.save(out_path);
        std::cout << "Saved to: " << out_path << std::endl;
    } else {
        std::cerr << "Effect application failed." << std::endl;
    }
}

void runBatch(const std::string& dir_path, const photo::IDPhotoStandard& standard,
              const std::string& output_dir, photo::CheckMode check_mode) {
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        std::cerr << "Error: Not a directory: " << dir_path << std::endl;
        return;
    }

    std::vector<std::string> images;
    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            for (auto& c : ext) c = static_cast<char>(std::tolower(c));
            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp") {
                images.push_back(entry.path().string());
            }
        }
    }

    std::cout << "Found " << images.size() << " images in " << dir_path << "\n";
    std::cout << "Check mode: " << photo::toString(check_mode) << "\n";

    json all_reports = json::array();
    photo::FaceAnalyzer analyzer;
    auto& mgr = photo::PluginManager<photo::IQualityChecker>::instance();
    auto checker_names = configuredCheckerNames(standard, mgr);

    for (size_t i = 0; i < images.size(); i++) {
        const auto& img_path = images[i];
        std::cout << "[" << (i + 1) << "/" << images.size() << "] "
                  << fs::path(img_path).filename().string() << " ... ";

        photo::Image img(img_path);
        if (img.empty()) {
            std::cout << "SKIP (cannot load)\n";
            continue;
        }

        photo::FaceInfo face = analyzer.detect(img.mat());

        photo::QualityReport report;
        report.photo_type = standard.photo_type;
        report.photo_display_name = standard.display_name;
        report.check_mode = photo::toString(check_mode);
        report.file_path = img_path;
        report.image_width = img.width();
        report.image_height = img.height();
        report.file_size_bytes = img.fileSize();
        report.face_info = face;

        for (const auto& name : checker_names) {
            auto checker = mgr.tryCreate(name);
            if (!checker) continue;
            auto result = checker->check(img.mat(), face, standard);
            photo::applyCheckMode(result, check_mode);
            report.results.push_back(result);
            if (result.passed) report.passed_checks++;
            else if (result.severity == photo::Severity::WARNING) report.warning_checks++;
            else report.failed_checks++;
        }

        report.total_checks = static_cast<int>(report.results.size());
        report.overall_pass = (report.failed_checks == 0);

        std::cout << (report.overall_pass ? "PASS" : "FAIL")
                  << " (" << report.passed_checks << "/" << report.total_checks << ")\n";

        all_reports.push_back(json(report));
    }

    // Save batch report
    std::string out_path = output_dir.empty() ? dir_path + "/batch_report.json"
                                              : output_dir + "/batch_report.json";
    if (!output_dir.empty()) {
        fs::create_directories(output_dir);
    }
    std::ofstream of(out_path);
    of << all_reports.dump(2) << std::endl;
    std::cout << "\nBatch report saved to: " << out_path << std::endl;
}

void listCheckers() {
    auto& mgr = photo::PluginManager<photo::IQualityChecker>::instance();
    auto names = mgr.availablePlugins();
    std::cout << "Registered quality checkers (" << names.size() << "):\n";
    for (const auto& name : names) {
        auto c = mgr.create(name);
        std::cout << "  " << name << " (v" << c->version() << ")\n";
    }
}

void listEffects() {
    auto& mgr = photo::PluginManager<photo::IBeautyEffect>::instance();
    auto names = mgr.availablePlugins();
    std::cout << "Registered beauty effects (" << names.size() << "):\n";
    for (const auto& name : names) {
        auto e = mgr.create(name);
        std::cout << "  " << name << " (v" << e->version() << ")\n";
        for (const auto& [k, v] : e->defaultParams()) {
            std::cout << "    --" << k << ": " << v.description
                      << " (default=" << v.default_value
                      << ", range=[" << v.min_value << "," << v.max_value << "])\n";
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string mode;
    std::string image_path;
    std::string effect_name;
    std::string param_str;
    std::string standard_path = "configs/id_card_cn.json";
    std::string output_dir;
    std::string check_mode_value = "final";

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--check") {
            mode = "check";
            if (i + 1 < argc) image_path = argv[++i];
        } else if (arg == "--beautify") {
            mode = "beautify";
            if (i + 1 < argc) image_path = argv[++i];
        } else if (arg == "--batch") {
            mode = "batch";
            if (i + 1 < argc) image_path = argv[++i];
        } else if (arg == "--list-checkers") {
            mode = "list-checkers";
        } else if (arg == "--list-effects") {
            mode = "list-effects";
        } else if (arg == "--effect") {
            if (i + 1 < argc) effect_name = argv[++i];
        } else if (arg == "--params") {
            if (i + 1 < argc) param_str = argv[++i];
        } else if (arg == "--standard") {
            if (i + 1 < argc) standard_path = argv[++i];
        } else if (arg == "--output") {
            if (i + 1 < argc) output_dir = argv[++i];
        } else if (arg == "--check-mode") {
            if (i + 1 < argc) check_mode_value = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
    }

    auto standard = loadStandard(standard_path);
    auto check_mode = photo::parseCheckMode(check_mode_value);

    if (mode == "check") {
        if (image_path.empty()) {
            std::cerr << "Error: --check requires an image path\n";
            return 1;
        }
        runCheck(image_path, standard, check_mode);
    } else if (mode == "beautify") {
        if (image_path.empty()) {
            std::cerr << "Error: --beautify requires an image path\n";
            return 1;
        }
        if (effect_name.empty()) {
            std::cerr << "Error: --beautify requires --effect <name>\n";
            return 1;
        }
        auto params = parseParams(param_str);
        runBeautify(image_path, effect_name, params, standard);
    } else if (mode == "batch") {
        if (image_path.empty()) {
            std::cerr << "Error: --batch requires a directory path\n";
            return 1;
        }
        runBatch(image_path, standard, output_dir, check_mode);
    } else if (mode == "list-checkers") {
        listCheckers();
    } else if (mode == "list-effects") {
        listEffects();
    } else {
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}
