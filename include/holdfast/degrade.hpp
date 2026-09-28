#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "holdfast/types.hpp"

namespace holdfast {

// How the input breaks. All randomness comes from the seed, so a preset + seed always
// produces the same broken stream.
struct DegradeConfig {
    std::string name = "clean";
    // Blackout: bursts of frames that never arrive.
    double blackout_every_s = 0;  // 0 = off
    int blackout_min_frames = 0, blackout_max_frames = 0;
    // Freeze: the source keeps re-sending an old frame with fresh timestamps.
    double freeze_every_s = 0;
    int freeze_min_frames = 0, freeze_max_frames = 0;
    // Detection dropout: each detection independently lost.
    double dropout = 0;
    // Timestamp jitter, standard deviation.
    double jitter_ms = 0;
    // Gaussian blur sigma in pixels (only matters for an image-based detector).
    double blur_sigma = 0;

    static std::optional<DegradeConfig> preset(const std::string& name);
    static std::vector<std::string> preset_names();
};

// One frame that actually reaches the tracker.
struct Delivery {
    int true_frame = 0;     // 1-based frame index of the real world at this moment (for scoring)
    int content_frame = 0;  // 1-based frame whose pixels/detections were delivered
    double t = 0;           // timestamp in seconds, as the tracker sees it
    [[nodiscard]] bool frozen() const { return content_frame != true_frame; }
};

struct Schedule {
    std::vector<Delivery> delivered;
    std::vector<int> blacked_out;  // true frames that never arrived
};

Schedule plan_schedule(int num_frames, double fps, const DegradeConfig& cfg, std::uint64_t seed);

// Drops each detection with probability p.
class DetectionDropout {
public:
    DetectionDropout(double p, std::uint64_t seed);
    std::vector<Detection> apply(std::vector<Detection> dets);

private:
    double p_;
    std::uint64_t state_;
};

}  // namespace holdfast
