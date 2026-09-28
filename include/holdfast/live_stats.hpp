#pragma once

#include <map>
#include <vector>

#include "holdfast/tracker.hpp"

namespace holdfast {

// Live identity-switch estimate for display: a ground-truth object matched under a different
// track id than last time. TrackEval computes the real numbers; this is only for overlays.
struct LiveIdStats {
    std::map<int, int> gt_to_track;
    int switches = 0;

    void observe(const std::vector<TrackOutput>& out) {
        for (const auto& t : out) {
            if (!t.matched_this_frame || t.last_gt_id < 0) continue;
            auto [it, inserted] = gt_to_track.try_emplace(t.last_gt_id, t.id);
            if (!inserted && it->second != t.id) {
                ++switches;
                it->second = t.id;
            }
        }
    }
};

}  // namespace holdfast
