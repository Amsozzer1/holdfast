#include "holdfast/app/onnx_detector.hpp"

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <opencv2/imgproc.hpp>

namespace holdfast {

namespace {

// COCO ids kept: person, bicycle, car, motorcycle, bus, truck.
bool keep_class(int c) { return c == 0 || c == 1 || c == 2 || c == 3 || c == 5 || c == 7; }

std::vector<Detection> nms(std::vector<Detection> dets, float iou_thr) {
    std::sort(dets.begin(), dets.end(), [](const auto& a, const auto& b) { return a.score > b.score; });
    std::vector<Detection> out;
    std::vector<char> dead(dets.size(), 0);
    for (size_t i = 0; i < dets.size(); ++i) {
        if (dead[i]) continue;
        out.push_back(dets[i]);
        for (size_t j = i + 1; j < dets.size(); ++j) {
            if (!dead[j] && iou(dets[i].box, dets[j].box) > iou_thr) dead[j] = 1;
        }
    }
    return out;
}

}  // namespace

struct OnnxDetector::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "holdfast"};
    Ort::Session session{nullptr};
    std::string input_name, output_name;
    int size = 416;
    Params p;
    std::vector<float> blob;
};

OnnxDetector::OnnxDetector(const Params& p) : impl_(std::make_unique<Impl>()) {
    impl_->p = p;
    Ort::SessionOptions so;
    so.SetIntraOpNumThreads(p.threads);
    so.SetInterOpNumThreads(1);
    so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    impl_->session = Ort::Session(impl_->env, p.model_path.c_str(), so);
    Ort::AllocatorWithDefaultOptions alloc;
    impl_->input_name = impl_->session.GetInputNameAllocated(0, alloc).get();
    impl_->output_name = impl_->session.GetOutputNameAllocated(0, alloc).get();
    const auto shape = impl_->session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    if (shape.size() == 4 && shape[2] > 0) impl_->size = static_cast<int>(shape[2]);
    impl_->blob.resize(3 * impl_->size * impl_->size);
}

OnnxDetector::~OnnxDetector() = default;

int OnnxDetector::input_size() const { return impl_->size; }

std::vector<Detection> OnnxDetector::detect(const cv::Mat& bgr) {
    const int S = impl_->size;
    // Letterbox: scale to fit, pad bottom/right with 114 (YOLOX convention).
    const float r = std::min(static_cast<float>(S) / bgr.rows, static_cast<float>(S) / bgr.cols);
    cv::Mat resized, canvas(S, S, CV_8UC3, cv::Scalar(114, 114, 114));
    cv::resize(bgr, resized, cv::Size(static_cast<int>(bgr.cols * r), static_cast<int>(bgr.rows * r)), 0, 0, cv::INTER_LINEAR);
    resized.copyTo(canvas(cv::Rect(0, 0, resized.cols, resized.rows)));

    // The released YOLOX ONNX exports take raw BGR in [0, 255], CHW, no mean/std
    // (checked empirically: normalized RGB input gives max objectness ~0.01).
    float* blob = impl_->blob.data();
    for (int y = 0; y < S; ++y) {
        const auto* row = canvas.ptr<cv::Vec3b>(y);
        for (int x = 0; x < S; ++x) {
            for (int c = 0; c < 3; ++c) blob[c * S * S + y * S + x] = static_cast<float>(row[x][c]);
        }
    }

    const std::array<int64_t, 4> in_shape{1, 3, S, S};
    auto mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input = Ort::Value::CreateTensor<float>(mem, blob, impl_->blob.size(), in_shape.data(), in_shape.size());
    const char* in_names[] = {impl_->input_name.c_str()};
    const char* out_names[] = {impl_->output_name.c_str()};
    auto outputs = impl_->session.Run(Ort::RunOptions{nullptr}, in_names, &input, 1, out_names, 1);

    const auto oshape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();  // [1, N, 5 + classes]
    const int n = static_cast<int>(oshape[1]), stride_len = static_cast<int>(oshape[2]);
    const float* out = outputs[0].GetTensorData<float>();

    // Grid decode over strides 8, 16, 32.
    std::vector<Detection> dets;
    int idx = 0;
    for (int stride : {8, 16, 32}) {
        const int g = S / stride;
        for (int gy = 0; gy < g; ++gy) {
            for (int gx = 0; gx < g; ++gx, ++idx) {
                if (idx >= n) break;
                const float* o = out + static_cast<size_t>(idx) * stride_len;
                const float obj = o[4];
                if (obj < impl_->p.score_threshold) continue;
                const float* cls = o + 5;
                const int best = static_cast<int>(std::max_element(cls, cls + (stride_len - 5)) - cls);
                const float score = obj * cls[best];
                if (score < impl_->p.score_threshold || !keep_class(best)) continue;
                const float cx = (o[0] + gx) * stride / r, cy = (o[1] + gy) * stride / r;
                const float w = std::exp(o[2]) * stride / r, h = std::exp(o[3]) * stride / r;
                Detection d;
                d.box = BBox::from_center(cx, cy, w, h);
                d.score = score;
                d.cls = best;
                dets.push_back(d);
            }
        }
    }
    return nms(std::move(dets), impl_->p.nms_iou);
}

}  // namespace holdfast
