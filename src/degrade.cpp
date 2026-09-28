#include "holdfast/degrade.hpp"

#include <algorithm>

#include "holdfast/rng.hpp"

namespace holdfast {

std::optional<DegradeConfig> DegradeConfig::preset(const std::string& name) {
    DegradeConfig c;
    c.name = name;
    if (name == "clean") return c;
    if (name == "light") {
        c.blackout_every_s = 8; c.blackout_min_frames = 5; c.blackout_max_frames = 10;
        c.dropout = 0.10;
        c.jitter_ms = 3;
        return c;
    }
    if (name == "heavy") {
        c.blackout_every_s = 5; c.blackout_min_frames = 10; c.blackout_max_frames = 30;
        c.freeze_every_s = 7; c.freeze_min_frames = 5; c.freeze_max_frames = 15;
        c.dropout = 0.30;
        c.jitter_ms = 8;
        c.blur_sigma = 1.5;
        return c;
    }
    if (name == "blackout") {
        c.blackout_every_s = 4; c.blackout_min_frames = 15; c.blackout_max_frames = 30;
        return c;
    }
    if (name == "freeze") {
        c.freeze_every_s = 4; c.freeze_min_frames = 10; c.freeze_max_frames = 20;
        return c;
    }
    return std::nullopt;
}

std::vector<std::string> DegradeConfig::preset_names() { return {"clean", "light", "heavy", "blackout", "freeze"}; }

Schedule plan_schedule(int num_frames, double fps, const DegradeConfig& cfg, std::uint64_t seed) {
    Rng rng(seed ^ 0x5eedb1acULL);
    Schedule s;
    auto next_at = [&](double every) { return every > 0 ? every * rng.uniform(0.5, 1.5) : 1e18; };
    double next_blackout = next_at(cfg.blackout_every_s);
    double next_freeze = next_at(cfg.freeze_every_s);
    int blackout_left = 0, freeze_left = 0, frozen_content = 0;
    double last_t = -1.0;

    for (int f = 1; f <= num_frames; ++f) {
        const double t_true = (f - 1) / fps;
        if (blackout_left == 0 && freeze_left == 0 && t_true >= next_blackout && f > 1) {
            blackout_left = rng.uniform_int(cfg.blackout_min_frames, cfg.blackout_max_frames);
            next_blackout = t_true + cfg.blackout_every_s * rng.uniform(0.7, 1.3);
        }
        if (blackout_left == 0 && freeze_left == 0 && t_true >= next_freeze && f > 1) {
            freeze_left = rng.uniform_int(cfg.freeze_min_frames, cfg.freeze_max_frames);
            frozen_content = s.delivered.empty() ? 1 : s.delivered.back().content_frame;
            next_freeze = t_true + cfg.freeze_every_s * rng.uniform(0.7, 1.3);
        }

        double jitter = cfg.jitter_ms > 0 ? rng.normal(0.0, cfg.jitter_ms / 1000.0) : 0.0;
        if (blackout_left > 0) {
            --blackout_left;
            s.blacked_out.push_back(f);
            continue;
        }
        Delivery d;
        d.true_frame = f;
        d.content_frame = f;
        if (freeze_left > 0) {
            --freeze_left;
            d.content_frame = frozen_content;
        }
        d.t = std::max(t_true + jitter, last_t + 1e-4);  // timestamps never go backwards
        last_t = d.t;
        s.delivered.push_back(d);
    }
    return s;
}

DetectionDropout::DetectionDropout(double p, std::uint64_t seed) : p_(p), state_(seed ^ 0xd50b0u) {}

std::vector<Detection> DetectionDropout::apply(std::vector<Detection> dets) {
    if (p_ <= 0) return dets;
    Rng rng(state_);
    state_ = state_ * 6364136223846793005ULL + 1442695040888963407ULL;  // next stream per frame
    std::erase_if(dets, [&](const Detection&) { return rng.bernoulli(p_); });
    return dets;
}

}  // namespace holdfast
