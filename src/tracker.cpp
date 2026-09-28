#include "holdfast/tracker.hpp"

#include <algorithm>
#include <cmath>

#include "holdfast/hungarian.hpp"

namespace holdfast {

const char* to_string(TrackState s) {
    switch (s) {
        case TrackState::Tentative: return "tentative";
        case TrackState::Confirmed: return "confirmed";
        case TrackState::Lost: return "lost";
        case TrackState::Deleted: return "deleted";
    }
    return "?";
}

TrackerConfig TrackerConfig::baseline() {
    TrackerConfig c;
    c.name = "baseline";
    c.variable_dt = false;
    c.max_age_in_seconds = false;
    c.gate_chi2 = 0;
    c.recovery = false;
    c.oc_reupdate = false;
    c.stale_guard = false;
    c.cmc = false;
    return c;
}

TrackerConfig TrackerConfig::full() { return TrackerConfig{}; }

std::optional<TrackerConfig> TrackerConfig::by_name(const std::string& spec) {
    // "<base>+key=value+key=value", e.g. "full+recovery=0+gate=0" for ablations.
    const auto plus = spec.find('+');
    auto cfg = by_base_name(spec.substr(0, plus));
    if (!cfg) return std::nullopt;
    cfg->name = spec;
    for (size_t pos = plus; pos != std::string::npos;) {
        const size_t next = spec.find('+', pos + 1);
        const std::string kv = spec.substr(pos + 1, next == std::string::npos ? std::string::npos : next - pos - 1);
        pos = next;
        const auto eq = kv.find('=');
        if (eq == std::string::npos) return std::nullopt;
        const std::string k = kv.substr(0, eq);
        const double v = std::stod(kv.substr(eq + 1));
        if (k == "recovery") cfg->recovery = v != 0;
        else if (k == "oc") cfg->oc_reupdate = v != 0;
        else if (k == "gate") cfg->gate_chi2 = v;
        else if (k == "rgate") cfg->recovery_gate_chi2 = v;
        else if (k == "rsize") cfg->recovery_max_size_ratio = v;
        else if (k == "maxage") { cfg->max_age_in_seconds = true; cfg->max_age_s = v; }
        else if (k == "vdt") cfg->variable_dt = v != 0;
        else if (k == "stale") cfg->stale_guard = v != 0;
        else if (k == "cmc") cfg->cmc = v != 0;
        else return std::nullopt;
    }
    return cfg;
}

std::optional<TrackerConfig> TrackerConfig::by_base_name(const std::string& name) {
    if (name == "baseline") return baseline();
    if (name == "cmc") {
        auto c = baseline();
        c.name = "cmc";
        c.cmc = true;
        return c;
    }
    if (name == "full") return full();
    if (name == "full_nocmc") {
        auto c = full();
        c.name = "full_nocmc";
        c.cmc = false;
        return c;
    }
    return std::nullopt;
}

Tracker::Tracker(TrackerConfig cfg) : cfg_(std::move(cfg)) {}

namespace {

BBox lerp(const BBox& a, const BBox& b, double s) {
    const auto f = static_cast<float>(s);
    return BBox::from_center(a.cx() + f * (b.cx() - a.cx()), a.cy() + f * (b.cy() - a.cy()),
                             a.w + f * (b.w - a.w), a.h + f * (b.h - a.h));
}

BBox warp(const BBox& b, const Affine2& A) {
    const auto& m = A.m;
    const double cx = m[0] * b.cx() + m[1] * b.cy() + m[2];
    const double cy = m[3] * b.cx() + m[4] * b.cy() + m[5];
    const double s = std::sqrt(std::abs(m[0] * m[4] - m[1] * m[3]));
    return BBox::from_center(static_cast<float>(cx), static_cast<float>(cy),
                             static_cast<float>(b.w * s), static_cast<float>(b.h * s));
}

}  // namespace

double Tracker::seconds_since_update(const Track& tr, double now) const {
    return now - tr.t_last_update;
}

void Tracker::apply_camera_motion(const Affine2& A) {
    if (A.is_identity()) return;
    for (auto& tr : tracks_) {
        tr.kf.apply_affine(A);
        KalmanBoxFilter snap;
        snap.set_state(tr.obs_mean, tr.obs_cov);
        snap.apply_affine(A);
        tr.obs_mean = snap.mean();
        tr.obs_cov = snap.covariance();
        tr.last_obs = warp(tr.last_obs, A);
    }
}

void Tracker::match(std::vector<int>& track_idx, std::vector<int>& det_idx, const std::vector<Detection>& dets,
                    double max_cost, bool use_gate, std::vector<std::pair<int, int>>& matches) {
    if (track_idx.empty() || det_idx.empty()) return;
    CostMatrix C(static_cast<int>(track_idx.size()), static_cast<int>(det_idx.size()));
    for (size_t i = 0; i < track_idx.size(); ++i) {
        const Track& tr = tracks_[track_idx[i]];
        const BBox pred = tr.kf.box();
        for (size_t j = 0; j < det_idx.size(); ++j) {
            const Detection& d = dets[det_idx[j]];
            if (use_gate && cfg_.gate_chi2 > 0 && tr.kf.gating_distance(d.box) > cfg_.gate_chi2) continue;
            C(static_cast<int>(i), static_cast<int>(j)) = 1.0 - iou(pred, d.box);
        }
    }
    std::vector<char> t_used(track_idx.size(), 0), d_used(det_idx.size(), 0);
    for (auto [i, j] : solve_assignment(C, max_cost)) {
        matches.emplace_back(track_idx[i], det_idx[j]);
        t_used[i] = d_used[j] = 1;
    }
    std::vector<int> t_rest, d_rest;
    for (size_t i = 0; i < track_idx.size(); ++i) if (!t_used[i]) t_rest.push_back(track_idx[i]);
    for (size_t j = 0; j < det_idx.size(); ++j) if (!d_used[j]) d_rest.push_back(det_idx[j]);
    track_idx.swap(t_rest);
    det_idx.swap(d_rest);
}

void Tracker::recover(std::vector<int>& lost_idx, std::vector<int>& det_idx, const std::vector<Detection>& dets,
                      std::vector<std::pair<int, int>>& matches) {
    if (lost_idx.empty() || det_idx.empty()) return;
    // Cost is the normalized position Mahalanobis distance. The covariance has been growing
    // since the last update, so the gate widens automatically the longer a track is unseen.
    CostMatrix C(static_cast<int>(lost_idx.size()), static_cast<int>(det_idx.size()));
    for (size_t i = 0; i < lost_idx.size(); ++i) {
        const Track& tr = tracks_[lost_idx[i]];
        const BBox pred = tr.kf.box();
        for (size_t j = 0; j < det_idx.size(); ++j) {
            const Detection& d = dets[det_idx[j]];
            const double ra = std::max(d.box.area(), 1.0f) / std::max(tr.last_obs.area(), 1.0f);
            const double r = std::sqrt(ra);
            if (r > cfg_.recovery_max_size_ratio || r < 1.0 / cfg_.recovery_max_size_ratio) continue;
            const double m = tr.kf.gating_distance(d.box, /*position_only=*/true);
            if (m > cfg_.recovery_gate_chi2) continue;
            // Small IoU bonus breaks ties between candidates at similar distances.
            C(static_cast<int>(i), static_cast<int>(j)) = m / cfg_.recovery_gate_chi2 + (1.0 - iou(pred, d.box)) * 0.1;
        }
    }
    std::vector<char> t_used(lost_idx.size(), 0), d_used(det_idx.size(), 0);
    for (auto [i, j] : solve_assignment(C)) {
        matches.emplace_back(lost_idx[i], det_idx[j]);
        t_used[i] = d_used[j] = 1;
    }
    std::vector<int> t_rest, d_rest;
    for (size_t i = 0; i < lost_idx.size(); ++i) if (!t_used[i]) t_rest.push_back(lost_idx[i]);
    for (size_t j = 0; j < det_idx.size(); ++j) if (!d_used[j]) d_rest.push_back(det_idx[j]);
    lost_idx.swap(t_rest);
    det_idx.swap(d_rest);
}

void Tracker::apply_match(Track& tr, const Detection& d, double t, double t_real) {
    const double gap = t - tr.t_last_update;
    const bool had_gap = gap > 1.5 * cfg_.nominal_dt;

    if (cfg_.oc_reupdate && had_gap && tr.hits > 0) {
        // OC-SORT re-update: roll back to the last real observation and replay the gap with
        // virtual observations on the straight line between the old and the new box. This
        // replaces the velocity the filter drifted to while coasting with what actually happened.
        const int k = std::clamp(static_cast<int>(std::lround(gap / cfg_.nominal_dt)), 1, cfg_.oc_max_virtual_steps);
        const double step = gap / k;
        tr.kf.set_state(tr.obs_mean, tr.obs_cov);
        for (int i = 1; i < k; ++i) {
            tr.kf.predict(step);
            tr.kf.update(lerp(tr.last_obs, d.box, static_cast<double>(i) / k));
        }
        tr.kf.predict(step);
    }
    tr.kf.update(d.box);

    // A recovery is a previously confirmed track re-acquired after >= 2 missed frames of real
    // time. Real time, not model time, so a fixed-dt tracker is scored the same way.
    const double real_gap = t_real - tr.t_last_real;
    const bool was_confirmed = tr.state == TrackState::Confirmed || tr.state == TrackState::Lost;
    if (was_confirmed && real_gap > 2.5 * cfg_.nominal_dt) {
        events_.push_back({TrackEvent::Kind::Recovered, tr.id, real_gap, d.gt_id >= 0 && d.gt_id == tr.last_gt_id});
    }

    tr.hits += 1;
    tr.frames_since_update = 0;
    tr.t_last_update = t;
    tr.t_last_real = t_real;
    tr.score = d.score;
    tr.last_gt_id = d.gt_id;
    tr.matched = true;
    tr.last_obs = d.box;
    tr.obs_mean = tr.kf.mean();
    tr.obs_cov = tr.kf.covariance();
    if (tr.state == TrackState::Lost) tr.state = TrackState::Confirmed;
    if (tr.state == TrackState::Tentative && tr.hits >= cfg_.n_init) tr.state = TrackState::Confirmed;
}

std::vector<TrackOutput> Tracker::update(const FrameInput& in) {
    // Model time: real timestamps, or (for a frame-index tracker) one nominal frame per call.
    const double dt = first_frame_ ? 0.0
                      : cfg_.variable_dt ? std::max(0.0, in.t - t_prev_)
                                         : cfg_.nominal_dt;
    const double now = first_frame_ ? (cfg_.variable_dt ? in.t : 0.0) : t_now_ + dt;
    t_prev_ = in.t;
    t_now_ = now;

    if (cfg_.cmc) apply_camera_motion(in.camera_motion);
    for (auto& tr : tracks_) {
        tr.kf.predict(dt);
        tr.matched = false;
    }

    const bool skip_measurements = in.stale && cfg_.stale_guard;
    if (!skip_measurements) {
        const auto& dets = in.detections;
        std::vector<int> high, low;
        for (int j = 0; j < static_cast<int>(dets.size()); ++j) {
            if (dets[j].score >= cfg_.high_score) high.push_back(j);
            else if (dets[j].score >= cfg_.low_score) low.push_back(j);
        }
        std::vector<int> pool, tentative;
        for (int i = 0; i < static_cast<int>(tracks_.size()); ++i) {
            (tracks_[i].state == TrackState::Tentative ? tentative : pool).push_back(i);
        }

        std::vector<std::pair<int, int>> matches;
        // Stage 1: confirmed + lost tracks vs high-score detections, IoU with a Mahalanobis gate.
        match(pool, high, dets, cfg_.stage1_max_cost, /*use_gate=*/true, matches);
        // Stage 2: still-unmatched *confirmed* tracks vs low-score detections, IoU only.
        std::vector<int> pool_confirmed, pool_lost;
        for (int i : pool) (tracks_[i].state == TrackState::Confirmed ? pool_confirmed : pool_lost).push_back(i);
        match(pool_confirmed, low, dets, cfg_.stage2_max_cost, /*use_gate=*/false, matches);
        // Recovery: every still-unmatched track that was confirmed, by position.
        if (cfg_.recovery) {
            std::vector<int> rec = pool_lost;
            rec.insert(rec.end(), pool_confirmed.begin(), pool_confirmed.end());
            recover(rec, high, dets, matches);
        }
        // Tentative tracks vs what is left.
        match(tentative, high, dets, cfg_.tentative_max_cost, /*use_gate=*/false, matches);

        for (auto [ti, dj] : matches) apply_match(tracks_[ti], dets[dj], now, in.t);

        // Unmatched tracks change state.
        for (auto& tr : tracks_) {
            if (tr.matched) continue;
            tr.frames_since_update += 1;
            if (tr.state == TrackState::Tentative) tr.state = TrackState::Deleted;
            else if (tr.state == TrackState::Confirmed) tr.state = TrackState::Lost;
        }

        // New tracks from unmatched high-score detections. On the very first frame they are
        // confirmed immediately (nothing to be consistent with yet), as ByteTrack does.
        for (int j : high) {
            if (dets[j].score < cfg_.new_track_score) continue;
            Track tr;
            tr.id = next_id_++;
            tr.kf = KalmanBoxFilter(dets[j].box, {.nominal_dt = cfg_.nominal_dt});
            tr.state = first_frame_ ? TrackState::Confirmed : TrackState::Tentative;
            tr.hits = 1;
            tr.t_last_update = now;
            tr.t_last_real = in.t;
            tr.score = dets[j].score;
            tr.last_gt_id = dets[j].gt_id;
            tr.matched = true;
            tr.last_obs = dets[j].box;
            tr.obs_mean = tr.kf.mean();
            tr.obs_cov = tr.kf.covariance();
            tracks_.push_back(tr);
        }
    }

    // Lifetime: seconds of real time, or a count of frames actually processed.
    for (auto& tr : tracks_) {
        if (tr.state != TrackState::Lost) continue;
        const bool expired = cfg_.max_age_in_seconds ? seconds_since_update(tr, now) > cfg_.max_age_s
                                                     : tr.frames_since_update > cfg_.max_age_frames;
        if (expired) {
            tr.state = TrackState::Deleted;
            events_.push_back({TrackEvent::Kind::Deleted, tr.id, seconds_since_update(tr, now), false});
        }
    }
    std::erase_if(tracks_, [](const Track& tr) { return tr.state == TrackState::Deleted; });

    first_frame_ = false;

    std::vector<TrackOutput> out;
    for (const auto& tr : tracks_) {
        if (tr.state != TrackState::Confirmed) continue;
        // On a frozen frame nothing is matched; report confirmed tracks on their prediction.
        if (tr.matched || skip_measurements) out.push_back(snapshot(tr, now));
    }
    return out;
}

TrackOutput Tracker::snapshot(const Track& tr, double now) const {
    TrackOutput o;
    o.id = tr.id;
    o.box = tr.kf.box();
    o.state = tr.state;
    o.score = tr.score;
    o.matched_this_frame = tr.matched;
    o.sigma_x = static_cast<float>(2.0 * std::sqrt(std::max(tr.kf.covariance()(0, 0), 0.0)));
    o.sigma_y = static_cast<float>(2.0 * std::sqrt(std::max(tr.kf.covariance()(1, 1), 0.0)));
    o.seconds_since_update = seconds_since_update(tr, now);
    o.last_gt_id = tr.last_gt_id;
    return o;
}

std::vector<TrackOutput> Tracker::all_tracks() const {
    std::vector<TrackOutput> out;
    out.reserve(tracks_.size());
    for (const auto& tr : tracks_) out.push_back(snapshot(tr, t_now_));
    return out;
}

}  // namespace holdfast

namespace holdfast {

std::vector<TrackOutput> Tracker::preview(double t) const {
    std::vector<TrackOutput> out;
    const double dt = cfg_.variable_dt ? std::max(0.0, t - t_prev_) : 0.0;
    for (const auto& tr : tracks_) {
        if (tr.state != TrackState::Confirmed && tr.state != TrackState::Lost) continue;
        Track copy = tr;
        copy.kf.predict(dt);
        copy.matched = false;
        out.push_back(snapshot(copy, t_now_ + dt));
    }
    return out;
}

}  // namespace holdfast
