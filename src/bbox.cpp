#include "holdfast/types.hpp"

#include <algorithm>

namespace holdfast {

float iou(const BBox& a, const BBox& b) {
    const float ix = std::max(0.0f, std::min(a.x + a.w, b.x + b.w) - std::max(a.x, b.x));
    const float iy = std::max(0.0f, std::min(a.y + a.h, b.y + b.h) - std::max(a.y, b.y));
    const float inter = ix * iy;
    const float uni = a.area() + b.area() - inter;
    return uni > 0.0f ? inter / uni : 0.0f;
}

}  // namespace holdfast
