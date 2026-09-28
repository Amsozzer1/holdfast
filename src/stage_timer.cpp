#include "holdfast/stage_timer.hpp"

#include <algorithm>
#include <format>
#include <numeric>

namespace holdfast {

void StageTimer::add(const std::string& stage, double ms) {
    auto [it, inserted] = samples_.try_emplace(stage);
    if (inserted) order_.push_back(stage);
    it->second.push_back(ms);
}

std::vector<StageTimer::Summary> StageTimer::summary() const {
    std::vector<Summary> out;
    for (const auto& name : order_) {
        auto v = samples_.at(name);
        std::sort(v.begin(), v.end());
        auto pct = [&](double p) { return v[std::min(v.size() - 1, static_cast<size_t>(p * (v.size() - 1) + 0.5))]; };
        out.push_back({name, pct(0.50), pct(0.95), std::accumulate(v.begin(), v.end(), 0.0) / v.size(), v.size()});
    }
    return out;
}

std::string StageTimer::table() const {
    std::string s = std::format("{:<12} {:>9} {:>9} {:>9} {:>7}\n", "stage", "p50 ms", "p95 ms", "mean ms", "n");
    for (const auto& r : summary()) {
        s += std::format("{:<12} {:>9.2f} {:>9.2f} {:>9.2f} {:>7}\n", r.stage, r.p50, r.p95, r.mean, r.n);
    }
    return s;
}

}  // namespace holdfast
