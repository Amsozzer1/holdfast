#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "holdfast/kalman.hpp"
#include "holdfast/types.hpp"

namespace holdfast {

enum class TrackState : std::uint8_t { Tentative, Confirmed, Lost, Deleted };

const char* to_string(TrackState s);

struct TrackerConfig {
    std::string name = "full";

    // Association thresholds (ByteTrack defaults).
    float high_score = 0.5f;        // stage 1 detections
    float low_score = 0.1f;         // stage 2 detections are in [low, high)
    float new_track_score = 0.6f;   // unmatched detections above this start a track
    double stage1_max_cost = 0.8;   // 1 - IoU  => IoU >= 0.2
    double stage2_max_cost = 0.5;   // IoU >= 0.5
    double tentative_max_cost = 0.7;
    int n_init = 3;

    // Optional Mahalanobis gate on stage 1 (e.g. 13.277 = chi-square 0.99, 4 DOF). Off by
    // default: on the dev sequences it rejected valid IoU matches (noisy boxes, imperfect CMC)
    // and cost ~3 HOTA. IoU alone already bounds stage 1; the gate lives in recovery instead.
    double gate_chi2 = 0;

    // Time model. variable_dt=false reproduces a frame-index tracker: every call is one
    // nominal frame, whatever the timestamps say.
    bool variable_dt = true;
    double nominal_dt = 1.0 / 30.0;
    bool max_age_in_seconds = true;
    double max_age_s = 1.5;
    int max_age_frames = 30;

    // Recovery stage for Lost tracks: position-only Mahalanobis (2 DOF), since IoU is ~0
    // after a long gap. chi-square 0.99 with 2 DOF = 9.21.
    bool recovery = true;
    double recovery_gate_chi2 = 9.21;
    double recovery_max_size_ratio = 2.0;

    // OC-SORT observation-centric re-update when a track that missed updates is matched again.
    bool oc_reupdate = true;
    int oc_max_virtual_steps = 60;

    // Frozen-feed guard: stale frames are predict-only.
    bool stale_guard = true;

    // Camera-motion compensation: use FrameInput::camera_motion (otherwise ignored).
    bool cmc = true;

    static TrackerConfig baseline();  // ByteTrack-lite: fixed dt, frame-count lifetimes, IoU only, no CMC
    static TrackerConfig full();
    // "baseline", "cmc" (baseline + CMC), "full", "full_nocmc" (ablation).
    // Also accepts overrides for ablations: "full+recovery=0+gate=0".
    static std::optional<TrackerConfig> by_name(const std::string& spec);
    static std::optional<TrackerConfig> by_base_name(const std::string& name);
};

struct TrackOutput {
    int id = 0;
    BBox box;
    TrackState state = TrackState::Tentative;
    float score = 0;
    bool matched_this_frame = false;
    // 2-sigma half-axes of position uncertainty, for drawing.
    float sigma_x = 0, sigma_y = 0;
    double seconds_since_update = 0;
    int last_gt_id = -1;
};

struct TrackEvent {
    enum class Kind : std::uint8_t { Recovered, Deleted } kind;
    int track_id;
    double gap_s;
    bool correct;  // recovered: detection's gt_id equals the track's last gt_id
};

// One frame worth of input.
struct FrameInput {
    double t = 0;                       // seconds
    std::vector<Detection> detections;  // already degraded
    Affine2 camera_motion;              // previous received frame -> this frame
    bool stale = false;                 // frame is a repeat of the previous image
};

class Tracker {
public:
    explicit Tracker(TrackerConfig cfg = TrackerConfig::full());

    // Advances the tracker by one received frame; returns Confirmed tracks to report.
    std::vector<TrackOutput> update(const FrameInput& in);

    // Every live track (including Lost/Tentative), for rendering.
    [[nodiscard]] std::vector<TrackOutput> all_tracks() const;
    // Where confirmed/lost tracks would be at time t, without changing any state.
    // Used to draw what the tracker is holding while no frames arrive.
    [[nodiscard]] std::vector<TrackOutput> preview(double t) const;
    [[nodiscard]] const std::vector<TrackEvent>& events() const { return events_; }
    void clear_events() { events_.clear(); }
    [[nodiscard]] const TrackerConfig& config() const { return cfg_; }

private:
    struct Track {
        int id = 0;
        KalmanBoxFilter kf;
        TrackState state = TrackState::Tentative;
        int hits = 0;
        int frames_since_update = 0;
        double t_last_update = 0;  // model time
        double t_last_real = 0;    // timestamp time
        float score = 0;
        int last_gt_id = -1;
        bool matched = false;
        // Snapshot at the last real observation, for the OC-SORT re-update.
        BBox last_obs;
        KalmanBoxFilter::Vec8 obs_mean;
        KalmanBoxFilter::Mat8 obs_cov;
    };

    void apply_camera_motion(const Affine2& A);
    void match(std::vector<int>& track_idx, std::vector<int>& det_idx, const std::vector<Detection>& dets,
               double max_cost, bool use_gate, std::vector<std::pair<int, int>>& matches);
    void recover(std::vector<int>& lost_idx, std::vector<int>& det_idx, const std::vector<Detection>& dets,
                 std::vector<std::pair<int, int>>& matches);
    void apply_match(Track& tr, const Detection& d, double t, double t_real);
    [[nodiscard]] TrackOutput snapshot(const Track& tr, double now) const;
    [[nodiscard]] double seconds_since_update(const Track& tr, double now) const;

    TrackerConfig cfg_;
    std::vector<Track> tracks_;
    std::vector<TrackEvent> events_;
    int next_id_ = 1;
    double t_prev_ = 0;
    double t_now_ = 0;
    bool first_frame_ = true;
};

}  // namespace holdfast
