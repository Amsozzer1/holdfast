#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "holdfast/live_stats.hpp"
#include "holdfast/sim.hpp"
#include "holdfast/tracker.hpp"

namespace holdfast {

// Two trackers fed the byte-identical input stream from one Simulator, for side-by-side
// comparison. This is everything the browser playground runs; the WebAssembly bindings are a
// thin wrapper around it.
class Playground {
public:
    struct Side {
        std::string config;
        std::vector<TrackOutput> tracks;  // every live track (or predictions during a blackout)
        int id_switches = 0;
        int recoveries = 0;
        int recoveries_correct = 0;
        double update_ms = 0;             // wall time of the last Tracker::update
    };
    struct Frame {
        SimFrame sim;
        std::array<Side, 2> sides;
    };

    Playground(std::uint64_t seed, const std::string& left, const std::string& right, SimParams params = {});

    Frame step();
    void set_config(int side, const std::string& config);  // restarts that side's tracker
    Simulator& sim() { return sim_; }

private:
    struct State {
        std::string config;
        Tracker tracker;
        LiveIdStats live;
        int recoveries = 0, recoveries_correct = 0;
    };
    static State make_state(const std::string& config, double fps);

    Simulator sim_;
    std::array<State, 2> state_;
};

}  // namespace holdfast
