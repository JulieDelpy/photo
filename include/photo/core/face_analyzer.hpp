#pragma once

#include "common.hpp"
#include <opencv2/core.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/face/facemarkLBF.hpp>
#include <string>
#include <vector>

namespace photo {

// High-precision face detector + 68-point landmark extractor.
//
// Pipeline:
//   YuNet (cv::FaceDetectorYN)  →  face bbox + confidence
//   FacemarkLBF                 →  68 precise facial landmarks
//   Haar Cascade                →  fallback if ONNX model unavailable
//
// Model paths are relative to working directory for cross-machine portability.
class FaceAnalyzer {
public:
    // Model paths relative to cwd. Empty strings skip loading that model.
    explicit FaceAnalyzer(
        const std::string& yunet_model = "models/face_detection_yunet_2023mar.onnx",
        const std::string& lbf_model   = "models/lbfmodel.yaml"
    );

    // Returns true once models are loaded and high-precision pipeline is active.
    bool hasHighPrecision() const { return yunet_loaded_ && lbf_loaded_; }

    // Detect primary face + 68 landmarks. Falls back gracefully.
    FaceInfo detect(const cv::Mat& image);

private:
    FaceInfo detectYuNet(const cv::Mat& image);
    FaceInfo detectHaar (const cv::Mat& image);

    bool fitLandmarksLBF(const cv::Mat& image, const cv::Rect& face_roi,
                         std::vector<cv::Point2f>& landmarks);

    float computeEAR(const std::vector<cv::Point2f>& lm, int eye_start) const;
    float computeMAR(const std::vector<cv::Point2f>& lm) const;
    void  estimateHeadPose(FaceInfo& info);
    void  generateSyntheticLandmarks(FaceInfo& info);

    // Model objects
    cv::Ptr<cv::FaceDetectorYN>     yunet_;         // YuNet ONNX detector
    cv::Ptr<cv::face::FacemarkLBF>  facemark_;      // LBF 68-pt landmark fitter
    cv::CascadeClassifier           face_cascade_;  // Haar fallback

    std::string yunet_model_path_;
    std::string lbf_model_path_;

    bool yunet_loaded_   = false;
    bool lbf_loaded_     = false;
    bool cascade_loaded_ = false;

    float yunet_score_ = 0.7f;
    float yunet_nms_   = 0.3f;
    int   yunet_top_k_ = 5000;

    // 3D model for solvePnP
    std::vector<cv::Point3f> model_points_;
};

} // namespace photo
