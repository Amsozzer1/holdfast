#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "holdfast/types.hpp"

namespace holdfast {

// One VisDrone MOT annotation row.
struct GtBox {
    int frame = 0, id = 0;
    BBox box;
    int category = 0;
    int truncation = 0, occlusion = 0;
    bool considered = true;
};

// VisDrone categories kept for evaluation (class-agnostic): pedestrian, car, van, truck, bus.
bool is_eval_category(int category);

// frame -> boxes, only evaluated categories.
std::map<int, std::vector<GtBox>> load_visdrone_gt(const std::string& path);

struct GtNoise {
    double center_sigma = 0.05;  // fraction of box size
    double size_sigma = 0.05;
    double miss_prob = 0.05;               // unoccluded
    double miss_prob_partial = 0.15;       // occlusion = 1
    double miss_prob_heavy = 0.40;         // occlusion = 2
    double false_positives_per_frame = 1.0;
    double score_mean = 0.80, score_sd = 0.10;
    double score_drop_partial = 0.20, score_drop_heavy = 0.40;  // occluded boxes look worse
};

// Turns ground truth into a realistic, noisy detector: jitter, misses (more when occluded),
// false positives and a score that drops with occlusion, so both ByteTrack stages are used.
// This isolates tracker quality from detector quality.
class GroundTruthDetector {
public:
    GroundTruthDetector(std::map<int, std::vector<GtBox>> gt, int image_w, int image_h, GtNoise noise,
                        std::uint64_t seed);
    // Detections for a given content frame. Deterministic per (seed, frame).
    [[nodiscard]] std::vector<Detection> detect(int frame) const;
    [[nodiscard]] int max_frame() const;

private:
    std::map<int, std::vector<GtBox>> gt_;
    int w_, h_;
    GtNoise noise_;
    std::uint64_t seed_;
};

}  // namespace holdfast
