#include "photo/core/face_analyzer.hpp"
#include "photo/plugin/plugin_manager.hpp"
#include "photo/plugin/iquality_checker.hpp"
#include "photo/plugin/ibeauty_effect.hpp"
#include "photo/result/quality_report.hpp"
#include <nlohmann/json.hpp>
#include <httplib.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <mutex>

namespace fs = std::filesystem;
using json = nlohmann::json;

// ============================================================
// Config loading (same as CLI)
// ============================================================
photo::IDPhotoStandard loadStandard(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[web_server] WARNING: Cannot open config '" << path
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

// ============================================================
// Thread-safe FaceAnalyzer wrapper
// ============================================================
class SafeAnalyzer {
public:
    explicit SafeAnalyzer(photo::FaceAnalyzer&& a) : analyzer_(std::move(a)) {}

    photo::FaceInfo detect(const cv::Mat& img) {
        std::lock_guard<std::mutex> lock(mtx_);
        return analyzer_.detect(img);
    }

private:
    photo::FaceAnalyzer analyzer_;
    std::mutex mtx_;
};

// ============================================================
// Helpers: in-memory image decode/encode
// ============================================================
cv::Mat decodeImage(const std::string& data) {
    std::vector<uchar> buf(data.begin(), data.end());
    return cv::imdecode(buf, cv::IMREAD_COLOR);
}

std::string encodeJPEG(const cv::Mat& img, int quality = 92) {
    std::vector<uchar> buf;
    std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, quality };
    cv::imencode(".jpg", img, buf, params);
    return { reinterpret_cast<const char*>(buf.data()), buf.size() };
}

// ============================================================
// CORS / common response headers
// ============================================================
void setCORS(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

// ============================================================
// main()
// ============================================================
int main(int argc, char* argv[]) {
    std::string standard_path = "configs/id_card_cn.json";
    std::string web_root      = "web";
    int         port          = 9876;

    // Arg parsing
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if ((a == "--standard" || a == "-s") && i + 1 < argc)
            standard_path = argv[++i];
        else if ((a == "--port" || a == "-p") && i + 1 < argc)
            port = std::stoi(argv[++i]);
        else if ((a == "--web-root" || a == "-w") && i + 1 < argc)
            web_root = argv[++i];
        else if (a == "--help" || a == "-h") {
            std::cout << "photo_web_server v1.0.0\n\n"
                      << "Usage: " << argv[0] << " [options]\n"
                      << "  -s, --standard <json>  ID photo config (default: configs/id_card_cn.json)\n"
                      << "  -p, --port <int>       Listen port (default: 9876)\n"
                      << "  -w, --web-root <dir>   Static files directory (default: web)\n"
                      << "  -h, --help             Show this help\n";
            return 0;
        }
    }

    // --- Load config ---
    auto standard = loadStandard(standard_path);
    std::cout << "[web_server] Standard: " << standard.display_name
              << " (" << standard.photo_type << ")\n";

    // --- Face analyzer ---
    photo::FaceAnalyzer rawAnalyzer;
    bool highPrecision = rawAnalyzer.hasHighPrecision();
    SafeAnalyzer analyzer(std::move(rawAnalyzer));
    std::cout << "[web_server] FaceAnalyzer: "
              << (highPrecision ? "YuNet+LBF (high precision)" : "Haar+fallback")
              << "\n";

    // --- HTTP server ---
    httplib::Server srv;

    // ========================================================
    // CORS preflight
    // ========================================================
    srv.Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& res) {
        setCORS(res);
        res.status = 204;
    });

    // ========================================================
    // POST /api/v1/check  —  quality evaluation
    // ========================================================
    srv.Post("/api/v1/check", [&](const httplib::Request& req, httplib::Response& res) {
        setCORS(res);

        // --- Extract image from multipart form ---
        if (!req.has_file("image")) {
            res.status = 400;
            res.set_content(R"({"error":"missing 'image' field"})", "application/json");
            return;
        }

        auto img_file = req.get_file_value("image");
        cv::Mat image = decodeImage(img_file.content);
        if (image.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"cannot decode image"})", "application/json");
            return;
        }

        // --- 智能预处理：大图等比缩放+居中裁剪至标准 358x441，小图不做放大 ---
        constexpr int kTargetW = 358;
        constexpr int kTargetH = 441;
        int srcW = image.cols, srcH = image.rows;
        bool needResize = (srcW > kTargetW || srcH > kTargetH);
        if (needResize) {
            double srcAspect = static_cast<double>(srcW) / srcH;
            double dstAspect = static_cast<double>(kTargetW) / kTargetH;
            int newW, newH;
            if (srcAspect > dstAspect) {
                newH = kTargetH;
                newW = static_cast<int>(kTargetH * srcAspect);
            } else {
                newW = kTargetW;
                newH = static_cast<int>(kTargetW / srcAspect);
            }
            cv::Mat scaled;
            cv::resize(image, scaled, cv::Size(newW, newH), 0, 0, cv::INTER_AREA);
            int cx = (newW - kTargetW) / 2;
            int cy = (newH - kTargetH) / 2;
            cv::Rect roi(cx, cy, kTargetW, kTargetH);
            roi &= cv::Rect(0, 0, newW, newH);
            image = scaled(roi).clone();
        }

        // --- Face detection ---
        photo::FaceInfo face = analyzer.detect(image);

        // --- Run all quality checkers ---
        photo::QualityReport report;
        report.photo_type = standard.photo_type;
        report.photo_display_name = standard.display_name;
        report.file_path = img_file.filename;
        report.image_width = image.cols;
        report.image_height = image.rows;
        report.file_size_bytes = img_file.content.size();
        report.face_info = face;

        auto& mgr = photo::PluginManager<photo::IQualityChecker>::instance();
        auto checker_names = mgr.availablePlugins();

        for (const auto& name : checker_names) {
            auto checker = mgr.create(name);
            if (!checker) continue;

            auto result = checker->check(image, face, standard);
            report.results.push_back(result);

            if (result.passed) report.passed_checks++;
            else if (result.severity == photo::Severity::WARNING) report.warning_checks++;
            else report.failed_checks++;
        }

        report.total_checks = static_cast<int>(report.results.size());
        report.overall_pass = (report.failed_checks == 0);

        // Build summary
        {
            std::ostringstream ss;
            ss << report.passed_checks << "/" << report.total_checks
               << (report.overall_pass ? " PASS" : " FAIL");
            report.summary = ss.str();
        }

        json j = report;
        res.set_content(j.dump(2), "application/json");
    });

    // ========================================================
    // POST /api/v1/beautify  —  in-memory beauty processing
    // ========================================================
    srv.Post("/api/v1/beautify", [&](const httplib::Request& req, httplib::Response& res) {
        setCORS(res);

        if (!req.has_file("image")) {
            res.status = 400;
            res.set_content(R"({"error":"missing 'image' field"})", "application/json");
            return;
        }

        // --- Extract image ---
        auto img_file = req.get_file_value("image");
        cv::Mat image = decodeImage(img_file.content);
        if (image.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"cannot decode image"})", "application/json");
            return;
        }

        // --- Extract effect name ---
        std::string effect_name = "skin_smoothing";
        if (req.has_param("effect")) {
            effect_name = req.get_param_value("effect");
        } else if (req.has_file("effect")) {
            effect_name = req.get_file_value("effect").content;
        }

        // --- Extract params ---
        photo::ParamMap params;
        std::string params_raw;
        if (req.has_param("params")) {
            params_raw = req.get_param_value("params");
        } else if (req.has_file("params")) {
            params_raw = req.get_file_value("params").content;
        }

        if (!params_raw.empty()) {
            try {
                auto jp = json::parse(params_raw);
                for (auto& [k, v] : jp.items()) {
                    params[k] = v.get<float>();
                }
            } catch (...) {
                // If params parse fails, use defaults
            }
        }

        // --- Face detection (needed by beauty effects) ---
        photo::FaceInfo face = analyzer.detect(image);
        if (!face.detected) {
            res.status = 422;
            res.set_content(R"({"error":"no face detected in image"})", "application/json");
            return;
        }

        // --- Apply beauty effect ---
        auto& beautyMgr = photo::PluginManager<photo::IBeautyEffect>::instance();
        auto effect = beautyMgr.create(effect_name);
        if (!effect) {
            res.status = 400;
            std::string err = R"({"error":"unknown effect: ')" + effect_name + "'\"}";
            res.set_content(err, "application/json");
            return;
        }

        if (!effect->apply(image, face, params)) {
            res.status = 500;
            res.set_content(R"({"error":"beauty effect application failed"})", "application/json");
            return;
        }

        // --- Encode result as JPEG in memory, stream back ---
        std::string jpeg = encodeJPEG(image);
        res.set_content(std::move(jpeg), "image/jpeg");
    });

    // ========================================================
    // Health check
    // ========================================================
    srv.Get("/api/v1/health", [&](const httplib::Request&, httplib::Response& res) {
        setCORS(res);
        json j;
        j["status"] = "ok";
        j["high_precision"] = highPrecision;
        j["standard"] = standard.photo_type;
        res.set_content(j.dump(), "application/json");
    });

    // ========================================================
    // Static files — serve web/ directory
    // ========================================================
    if (fs::exists(web_root) && fs::is_directory(web_root)) {
        srv.set_mount_point("/", web_root);
        std::cout << "[web_server] Serving static files from: "
                  << fs::absolute(web_root).string() << "\n";
    } else {
        std::cerr << "[web_server] WARNING: web root not found: " << web_root << "\n";
    }

    // ========================================================
    // Start
    // ========================================================
    std::cout << "[web_server] Listening on http://localhost:" << port << "\n";
    std::cout << "[web_server] Open http://localhost:" << port << " in your browser\n";
    std::cout << "[web_server] API endpoints:\n"
              << "  POST /api/v1/check    — quality evaluation (multipart: image)\n"
              << "  POST /api/v1/beautify  — beauty processing   (multipart: image, effect, params)\n"
              << "  GET  /api/v1/health    — server status\n";

    if (!srv.listen("0.0.0.0", port)) {
        std::cerr << "[web_server] ERROR: Failed to bind to port " << port << "\n";
        return 1;
    }

    return 0;
}
