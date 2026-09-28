#include "holdfast/playground.hpp"

#include <chrono>
#include <stdexcept>

namespace holdfast {

Playground::State Playground::make_state(const std::string& config, double fps) {
    auto cfg = TrackerConfig::by_name(config);
    if (!cfg) throw std::invalid_argument("unknown tracker config: " + config);
    cfg->nominal_dt = 1.0 / fps;
    return State{config, Tracker(*cfg), {}, 0, 0};
}

Playground::Playground(std::uint64_t seed, const std::string& left, const std::string& right, SimParams params)
    : sim_(params, seed),
      state_{make_state(left, params.fps), make_state(right, params.fps)} {}

void Playground::set_config(int side, const std::string& config) {
    state_.at(static_cast<size_t>(side)) = make_state(config, sim_.params().fps);
}

Playground::Frame Playground::step() {
    Frame out;
    out.sim = sim_.step();
    const auto& f = out.sim;
    for (size_t k = 0; k < state_.size(); ++k) {
        auto& st = state_[k];
        auto& side = out.sides[k];
        side.config = st.config;
        if (!f.delivered) {
            side.tracks = st.tracker.preview(f.t_true);
        } else {
            const auto t0 = std::chrono::steady_clock::now();
            const auto reported = st.tracker.update({f.t_stamp, f.dets, f.camera_motion, f.stale});
            side.update_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            st.live.observe(reported);
            for (const auto& e : st.tracker.events()) {
                if (e.kind != TrackEvent::Kind::Recovered) continue;
                ++st.recoveries;
                if (e.correct) ++st.recoveries_correct;
            }
            st.tracker.clear_events();
            side.tracks = st.tracker.all_tracks();
        }
        side.id_switches = st.live.switches;
        side.recoveries = st.recoveries;
        side.recoveries_correct = st.recoveries_correct;
    }
    return out;
}

}  // namespace holdfast
