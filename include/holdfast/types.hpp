#pragma once

#include <array>
#include <cstdint>

namespace holdfast {

// Axis-aligned box in pixels, top-left + size (MOT convention).
struct BBox {
    float x = 0, y = 0, w = 0, h = 0;

    [[nodiscard]] float cx() const { return x + 0.5f * w; }
    [[nodiscard]] float cy() const { return y + 0.5f * h; }
    [[nodiscard]] float area() const { return w * h; }
    static BBox from_center(float cx, float cy, float w, float h) {
        return {cx - 0.5f * w, cy - 0.5f * h, w, h};
    }
};

[[nodiscard]] float iou(const BBox& a, const BBox& b);

struct Detection {
    BBox box;
    float score = 1.0f;
    int cls = 0;
    // Ground-truth identity, filled only by GroundTruthDetector. The tracker never reads it;
    // it exists so recovery events can be scored as correct or not.
    int gt_id = -1;
};

// 2x3 affine map from the previous received frame to the current one:
// [x'; y'] = [a b; c d] [x; y] + [tx; ty]
struct Affine2 {
    std::array<double, 6> m{1, 0, 0, 0, 1, 0};  // row-major: a b tx / c d ty

    static Affine2 identity() { return {}; }
    [[nodiscard]] bool is_identity() const {
        return m == std::array<double, 6>{1, 0, 0, 0, 1, 0};
    }
};

}  // namespace holdfast
