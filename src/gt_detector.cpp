#include "holdfast/gt_detector.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "holdfast/rng.hpp"

namespace holdfast {

bool is_eval_category(int c) { return c == 1 || c == 4 || c == 5 || c == 6 || c == 9; }

std::map<int, std::vector<GtBox>> load_visdrone_gt(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open annotations: " + path);
    std::map<int, std::vector<GtBox>> out;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        GtBox g;
        int score = 0;
        if (!(ss >> g.frame >> g.id >> g.box.x >> g.box.y >> g.box.w >> g.box.h >> score >> g.category >> g.truncation >> g.occlusion)) continue;
        g.considered = score != 0;
        if (!g.considered || !is_eval_category(g.category) || g.box.w <= 0 || g.box.h <= 0) continue;
        out[g.frame].push_back(g);
    }
    return out;
}

GroundTruthDetector::GroundTruthDetector(std::map<int, std::vector<GtBox>> gt, int image_w, int image_h,
                                         GtNoise noise, std::uint64_t seed)
    : gt_(std::move(gt)), w_(image_w), h_(image_h), noise_(noise), seed_(seed) {}

int GroundTruthDetector::max_frame() const { return gt_.empty() ? 0 : gt_.rbegin()->first; }

std::vector<Detection> GroundTruthDetector::detect(int frame) const {
    Rng rng(seed_ * 0x9E3779B97F4A7C15ULL + static_cast<std::uint64_t>(frame));
    std::vector<Detection> out;
    const auto it = gt_.find(frame);
    if (it != gt_.end()) {
        for (const auto& g : it->second) {
            const double miss = g.occlusion == 0 ? noise_.miss_prob
                              : g.occlusion == 1 ? noise_.miss_prob_partial : noise_.miss_prob_heavy;
            const bool missed = rng.bernoulli(miss);
            const double ncx = rng.normal(), ncy = rng.normal(), nw = rng.normal(), nh = rng.normal();
            double score = rng.normal(noise_.score_mean, noise_.score_sd);
            if (missed) continue;
            if (g.occlusion == 1) score -= noise_.score_drop_partial;
            if (g.occlusion == 2) score -= noise_.score_drop_heavy;
            const double w = std::max(2.0, g.box.w * (1.0 + noise_.size_sigma * nw));
            const double h = std::max(2.0, g.box.h * (1.0 + noise_.size_sigma * nh));
            const double cx = g.box.cx() + noise_.center_sigma * g.box.w * ncx;
            const double cy = g.box.cy() + noise_.center_sigma * g.box.h * ncy;
            Detection d;
            d.box = BBox::from_center(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(w), static_cast<float>(h));
            d.score = static_cast<float>(std::clamp(score, 0.05, 1.0));
            d.cls = g.category;
            d.gt_id = g.id;
            out.push_back(d);
        }
    }
    const int n_fp = rng.poisson(noise_.false_positives_per_frame);
    for (int k = 0; k < n_fp; ++k) {
        Detection d;
        const double w = rng.uniform(12, 60), h = rng.uniform(12, 60);
        d.box = {static_cast<float>(rng.uniform(0, std::max(1.0, w_ - w))), static_cast<float>(rng.uniform(0, std::max(1.0, h_ - h))),
                 static_cast<float>(w), static_cast<float>(h)};
        d.score = static_cast<float>(rng.uniform(0.1, 0.65));
        out.push_back(d);
    }
    return out;
}

}  // namespace holdfast
