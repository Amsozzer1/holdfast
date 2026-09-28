#pragma once

#include <memory>
#include <opencv2/core.hpp>
#include <string>
#include <vector>

#include "holdfast/types.hpp"

namespace holdfast {

// YOLOX-Nano through ONNX Runtime (CPU): letterbox -> inference -> grid decode -> NMS.
// COCO-trained, so it is weak on tiny aerial objects; it is here for latency numbers and
// demo footage, not for the accuracy table.
class OnnxDetector {
public:
    struct Params {
        std::string model_path;
        int threads = 1;
        float score_threshold = 0.1f;  // keep low-score boxes for ByteTrack stage 2
        float nms_iou = 0.45f;
    };

    explicit OnnxDetector(const Params& p);
    ~OnnxDetector();
    OnnxDetector(const OnnxDetector&) = delete;
    OnnxDetector& operator=(const OnnxDetector&) = delete;

    std::vector<Detection> detect(const cv::Mat& bgr);
    [[nodiscard]] int input_size() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace holdfast
