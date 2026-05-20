#include "photo/core/face_analyzer.hpp"
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/objdetect.hpp>       // cv::FaceDetectorYN
#include <opencv2/face.hpp>            // cv::face::FacemarkLBF
#include <cmath>
#include <iostream>

namespace photo {

FaceAnalyzer::FaceAnalyzer(const std::string& yunet_model, const std::string& lbf_model)
    : yunet_model_path_(yunet_model), lbf_model_path_(lbf_model)
{
    // --- YuNet face detector ---
    if (!yunet_model_path_.empty()) {
        try {
            yunet_ = cv::FaceDetectorYN::create(
                yunet_model_path_, "", cv::Size(320, 320),
                yunet_score_, yunet_nms_, yunet_top_k_
            );
            if (yunet_) {
                yunet_loaded_ = true;
                std::cout << "[FaceAnalyzer] YuNet loaded: " << yunet_model_path_ << std::endl;
            }
        } catch (const cv::Exception& e) {
            std::cerr << "[FaceAnalyzer] WARNING: YuNet failed: " << e.what() << std::endl;
        }
    }

    // --- LBF 68-point landmark model ---
    if (!lbf_model_path_.empty()) {
        try {
            facemark_ = cv::face::FacemarkLBF::create();
            facemark_->loadModel(lbf_model_path_);
            lbf_loaded_ = true;
            std::cout << "[FaceAnalyzer] LBF loaded: " << lbf_model_path_ << std::endl;
        } catch (const cv::Exception& e) {
            std::cerr << "[FaceAnalyzer] WARNING: LBF failed: " << e.what() << std::endl;
        }
    }

    // --- Haar Cascade fallback ---
    const char* cascade_paths[] = {
        "haarcascade_frontalface_default.xml",
        "/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",
        "/usr/local/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",
    };
    for (const auto& path : cascade_paths) {
        if (face_cascade_.load(path)) {
            cascade_loaded_ = true;
            break;
        }
    }
    if (!cascade_loaded_) {
        std::cerr << "[FaceAnalyzer] WARNING: Haar cascade not found" << std::endl;
    }

    // --- 3D model for solvePnP ---
    // Y and Z are negated from the canonical face model so that the model
    // coordinate frame matches the OpenCV image frame (x→right, y↓down, z→into scene).
    // Without this, solvePnP inserts a ~180° X rotation that leaks into roll.
    model_points_ = {
        {  0.0f,    0.0f,    0.0f},   // Nose tip      (lm[30])
        {  0.0f,  330.0f,   65.0f},   // Chin          (lm[8])
        {-225.0f, -170.0f,  135.0f},  // Left eye l.c. (lm[36])
        { 225.0f, -170.0f,  135.0f},  // Right eye r.c.(lm[45])
        {-150.0f,  150.0f,  125.0f},  // Left mouth    (lm[48])
        { 150.0f,  150.0f,  125.0f}   // Right mouth   (lm[54])
    };
}

// ============================================================
// Main detect()
// ============================================================
FaceInfo FaceAnalyzer::detect(const cv::Mat& image)
{
    // Tier 1: YuNet
    if (yunet_loaded_) {
        FaceInfo info = detectYuNet(image);
        if (info.detected) {
            if (lbf_loaded_) fitLandmarksLBF(image, info.bbox, info.landmarks);
            if (info.landmarks.size() >= 68) {
                info.ear_left  = computeEAR(info.landmarks, 36);
                info.ear_right = computeEAR(info.landmarks, 42);
                info.mar       = computeMAR(info.landmarks);
                estimateHeadPose(info, image.cols, image.rows);
            } else {
                generateSyntheticLandmarks(info);
            }
            return info;
        }
    }

    // Tier 2: Haar fallback
    FaceInfo info = detectHaar(image);
    if (info.detected) {
        if (lbf_loaded_) fitLandmarksLBF(image, info.bbox, info.landmarks);
        if (info.landmarks.size() >= 68) {
            info.ear_left  = computeEAR(info.landmarks, 36);
            info.ear_right = computeEAR(info.landmarks, 42);
            info.mar       = computeMAR(info.landmarks);
            estimateHeadPose(info, image.cols, image.rows);
        } else {
            generateSyntheticLandmarks(info);
        }
    }
    return info;
}

// ============================================================
// YuNet detection via cv::FaceDetectorYN
// ============================================================
FaceInfo FaceAnalyzer::detectYuNet(const cv::Mat& image)
{
    FaceInfo info;
    int w = image.cols, h = image.rows;

    // Resize input if dimensions changed (FaceDetectorYN caches input size)
    cv::Size cur_size = yunet_->getInputSize();
    if (cur_size.width != w || cur_size.height != h) {
        yunet_->setInputSize(cv::Size(w, h));
    }

    cv::Mat faces;                    // N x 15: [x,y,w,h, 5*landmark_xy, conf]
    yunet_->detect(image, faces);

    if (faces.rows == 0) {
        info.detected = false;
        return info;
    }

    // Pick highest-confidence face
    int best_row = -1;
    float best_score = 0.0f;
    for (int r = 0; r < faces.rows; r++) {
        float score = faces.at<float>(r, 14);
        if (score > best_score) {
            best_score = score;
            best_row   = r;
        }
    }

    if (best_row < 0 || best_score < 0.3f) {
        info.detected = false;
        return info;
    }

    int x  = static_cast<int>(faces.at<float>(best_row, 0));
    int y  = static_cast<int>(faces.at<float>(best_row, 1));
    int bw = static_cast<int>(faces.at<float>(best_row, 2));
    int bh = static_cast<int>(faces.at<float>(best_row, 3));

    info.detected   = true;
    info.confidence = best_score;
    info.bbox       = cv::Rect(x, y, bw, bh) & cv::Rect(0, 0, w, h);
    info.landmarks.clear();

    return info;
}

// ============================================================
// Haar Cascade fallback
// ============================================================
FaceInfo FaceAnalyzer::detectHaar(const cv::Mat& image)
{
    FaceInfo info;
    if (!cascade_loaded_) { info.detected = false; return info; }

    cv::Mat gray;
    if (image.channels() == 3)
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    else
        gray = image;

    cv::equalizeHist(gray, gray);

    std::vector<cv::Rect> faces;
    face_cascade_.detectMultiScale(gray, faces, 1.1, 3, 0,
                                    cv::Size(60, 60), cv::Size(image.cols, image.rows));
    if (faces.empty()) { info.detected = false; return info; }

    auto largest = std::max_element(faces.begin(), faces.end(),
        [](const cv::Rect& a, const cv::Rect& b) { return a.area() < b.area(); });

    info.detected   = true;
    info.confidence = 0.7f;
    info.bbox       = *largest;
    info.landmarks.clear();
    return info;
}

// ============================================================
// LBF – fit 68 landmarks to face ROI
// ============================================================
bool FaceAnalyzer::fitLandmarksLBF(const cv::Mat& image, const cv::Rect& face_roi,
                                    std::vector<cv::Point2f>& landmarks)
{
    if (!lbf_loaded_) return false;
    try {
        std::vector<cv::Rect> faces = { face_roi };
        std::vector<std::vector<cv::Point2f>> results;
        facemark_->fit(image, faces, results);
        if (!results.empty() && results[0].size() >= 68) {
            landmarks = std::move(results[0]);
            return true;
        }
    } catch (const cv::Exception&) {}
    return false;
}

// ============================================================
// EAR – Eye Aspect Ratio (6-point eye contour)
//   lm[eye_start..eye_start+5]:
//     0=left_corner, 1=top_left, 2=top, 3=top_right, 4=right_corner, 5=bottom
//   EAR = (‖p1-p5‖ + ‖p2-p4‖) / (2·‖p0-p3‖)
// ============================================================
float FaceAnalyzer::computeEAR(const std::vector<cv::Point2f>& lm, int es) const
{
    if (es + 6 > static_cast<int>(lm.size())) return 1.0f;
    float v1 = static_cast<float>(cv::norm(lm[es+1] - lm[es+5]));
    float v2 = static_cast<float>(cv::norm(lm[es+2] - lm[es+4]));
    float h  = static_cast<float>(cv::norm(lm[es]   - lm[es+3]));
    if (h < 1e-6f) return 1.0f;
    return (v1 + v2) / (2.0f * h);
}

// ============================================================
// MAR – Mouth Aspect Ratio
//   Uses inner lip: vertical=‖62-66‖, horizontal=‖60-64‖
// ============================================================
float FaceAnalyzer::computeMAR(const std::vector<cv::Point2f>& lm) const
{
    if (lm.size() < 68) return 0.0f;
    float v = static_cast<float>(cv::norm(lm[62] - lm[66]));
    float h = static_cast<float>(cv::norm(lm[60] - lm[64]));
    if (h < 1e-6f) return 0.0f;
    return v / h;
}

// ============================================================
// Head pose (yaw/pitch/roll) via solvePnP
// ============================================================
void FaceAnalyzer::estimateHeadPose(FaceInfo& info, int img_width, int img_height)
{
    if (info.landmarks.size() < 68) {
        info.yaw = info.pitch = info.roll = 0.0f;
        return;
    }

    std::vector<cv::Point2f> img_pts = {
        info.landmarks[30],  // Nose tip
        info.landmarks[8],   // Chin
        info.landmarks[36],  // Left eye left corner
        info.landmarks[45],  // Right eye right corner
        info.landmarks[48],  // Left mouth corner
        info.landmarks[54]   // Right mouth corner
    };

    // Adaptive camera intrinsics from image dimensions.
    // fx = fy = max(w,h) is a robust focal-length proxy for typical
    // consumer cameras, avoiding bbox-size-dependent distortion.
    double f  = static_cast<double>(std::max(img_width, img_height));
    double cx = img_width / 2.0;
    double cy = img_height / 2.0;

    cv::Mat K = (cv::Mat_<double>(3,3) << f, 0, cx, 0, f, cy, 0, 0, 1);
    cv::Mat D = cv::Mat::zeros(4, 1, CV_64F);
    cv::Mat rvec, tvec;

    if (cv::solvePnP(model_points_, img_pts, K, D, rvec, tvec)) {
        cv::Mat R;
        cv::Rodrigues(rvec, R);
        double sy = std::sqrt(R.at<double>(0,0)*R.at<double>(0,0)
                            + R.at<double>(1,0)*R.at<double>(1,0));
        if (sy > 1e-6) {
            info.yaw   = static_cast<float>(std::atan2( R.at<double>(1,0), R.at<double>(0,0)) * 180.0/CV_PI);
            info.pitch = static_cast<float>(std::atan2(-R.at<double>(2,0), sy) * 180.0/CV_PI);
            info.roll  = static_cast<float>(std::atan2( R.at<double>(2,1), R.at<double>(2,2)) * 180.0/CV_PI);
        }
    }
}

// ============================================================
// Synthetic landmarks (fallback — bbox proportional)
// ============================================================
void FaceAnalyzer::generateSyntheticLandmarks(FaceInfo& info)
{
    int cx = info.bbox.x + info.bbox.width  / 2;
    int cy = info.bbox.y + info.bbox.height / 2;
    int w  = info.bbox.width;
    int h  = info.bbox.height;

    info.landmarks.resize(68);
    for (int i = 0; i <= 16; i++) {
        float t = i / 16.0f;
        info.landmarks[i] = cv::Point2f(
            info.bbox.x + w*(0.1f + 0.8f*t),
            info.bbox.y + h*(0.75f + 0.25f*std::sin(t*CV_PI)));
    }
    for (int i = 17; i <= 26; i++) {
        float t = (i-17)/9.0f;
        info.landmarks[i] = cv::Point2f(
            info.bbox.x + w*(0.2f+0.6f*t),
            info.bbox.y + h*(0.3f-0.05f*std::sin(t*CV_PI)));
    }
    for (int i = 27; i <= 35; i++) {
        float t = (i-27)/8.0f;
        info.landmarks[i] = cv::Point2f(cx, info.bbox.y + h*(0.3f+0.35f*t));
    }
    for (int i = 36; i <= 41; i++) {
        float a = (i-36)*CV_PI/5.0f;
        info.landmarks[i] = cv::Point2f(cx-w*0.15f+w*0.08f*cos(a), cy-h*0.12f+h*0.06f*sin(a));
    }
    for (int i = 42; i <= 47; i++) {
        float a = (i-42)*CV_PI/5.0f;
        info.landmarks[i] = cv::Point2f(cx+w*0.15f+w*0.08f*cos(a), cy-h*0.12f+h*0.06f*sin(a));
    }
    for (int i = 48; i <= 67; i++) {
        float a = (i-48)*2.0f*CV_PI/20.0f;
        info.landmarks[i] = cv::Point2f(cx+w*0.18f*cos(a), cy+h*0.25f+h*0.1f*sin(a));
    }
    info.ear_left  = computeEAR(info.landmarks, 36);
    info.ear_right = computeEAR(info.landmarks, 42);
    info.mar       = computeMAR(info.landmarks);
    info.yaw = info.pitch = info.roll = 0.0f;
}

} // namespace photo
