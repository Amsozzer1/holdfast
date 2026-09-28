#pragma once

#include <cstdint>
#include <vector>

#include "holdfast/rng.hpp"
#include "holdfast/types.hpp"

namespace holdfast {

// A small synthetic top-down drone scene: cars on a road grid (with turns at intersections),
// pedestrians, occluders (trees, a bridge) and a moving camera. It produces the same inputs
// the real pipeline gives the tracker: noisy detections, timestamps, a camera-motion estimate
// and a stale flag, plus ground truth for scoring. Used by the browser playground, where the
// VisDrone frames cannot be redistributed. Deterministic for a given seed.

struct WorldRect { float x, y, w, h; };

enum class CameraMode : int { Static = 0, Pan = 1, Drone = 2 };

struct SimParams {
    int image_w = 1280, image_h = 720;
    double fps = 30;
    int target_objects = 45;
    CameraMode camera = CameraMode::Drone;
    double dropout = 0.0;              // per-detection drop probability
    double jitter_ms = 0.0;            // timestamp jitter (sd)
    double false_positives = 0.6;      // Poisson rate per frame
    double miss_prob = 0.05;
    double occluded_miss_prob = 0.85;
    double cmc_noise_px = 0.3;         // error of the simulated camera-motion estimator
    double cmc_fail_px = 150;          // larger inter-frame motion => estimator gives up (identity)
    double auto_blackout_every_s = 0;  // 0 = off
    double auto_freeze_every_s = 0;
};

struct SimBox {
    BBox box;
    int id = -1;
    bool car = true;
    bool occluded = false;
};

struct SimFrame {
    int index = 0;
    double t_true = 0;             // seconds
    double t_stamp = 0;            // what the tracker sees (jittered)
    bool delivered = true;         // false during a blackout
    bool stale = false;            // frozen feed: content is an old frame
    std::vector<Detection> dets;   // image coordinates
    std::vector<SimBox> truth;     // visible ground truth of the displayed content
    Affine2 camera_motion;         // estimate: previous delivered content -> this content
    bool cmc_failed = false;
    Affine2 view;                  // world -> image for the displayed content
};

class Simulator {
public:
    Simulator(SimParams p, std::uint64_t seed);

    SimFrame step();
    void blackout(double seconds);
    void freeze(double seconds);

    SimParams& params() { return p_; }
    [[nodiscard]] const std::vector<WorldRect>& roads() const { return roads_; }
    [[nodiscard]] const std::vector<WorldRect>& occluders() const { return occluders_; }
    [[nodiscard]] const std::vector<WorldRect>& buildings() const { return buildings_; }

    // Similarity world->image for a camera centred at (cx, cy), rotated by theta.
    [[nodiscard]] Affine2 world_to_image(double cx, double cy, double theta) const;

private:
    struct Object {
        int id;
        bool car;
        double x, y, vx, vy, w, h;  // world centre, px/s, size (along x/y)
        double heading_timer = 0;   // pedestrians: time to next heading change
        // Cars drive along one road: orientation, direction, cruise and current speed.
        bool horizontal = true;
        int dir = 1;
        double cruise = 0, speed = 0;
    };

    void spawn_initial();
    void maintain_population();
    bool spawn_one(bool inside_view);
    void move_objects(double dt);
    void move_car(Object& o, double dt, double t);
    [[nodiscard]] bool lane_free(bool horizontal, int dir, double lane, double pos, double clearance, const Object* self) const;
    [[nodiscard]] bool green(bool horizontal, int ix, int iy, double t) const;
    void move_camera(double dt);
    [[nodiscard]] std::vector<SimBox> visible_truth(const Affine2& view) const;
    [[nodiscard]] std::vector<Detection> detect(const std::vector<SimBox>& truth);
    [[nodiscard]] bool occluded(double x, double y) const;

    SimParams p_;
    Rng rng_;
    std::vector<WorldRect> roads_, occluders_, buildings_;
    std::vector<double> road_y_, road_x_;
    std::vector<Object> objects_;
    int next_id_ = 1;
    int frame_ = 0;

    // Camera state.
    double cam_x_, cam_y_, cam_theta_ = 0, cam_vx_ = 0, cam_vy_ = 0;
    double wp_x_, wp_y_;
    double jerk_timer_ = 2.0;
    int jerk_frames_ = 0;
    double jerk_dx_ = 0, jerk_dy_ = 0;

    // Delivery state.
    int blackout_left_ = 0, freeze_left_ = 0;
    double next_auto_blackout_ = 0, next_auto_freeze_ = 0;
    bool have_prev_ = false;
    Affine2 prev_view_;
    double last_stamp_ = -1;
    SimFrame frozen_;
};

// Affine helpers (row-major 2x3).
Affine2 compose(const Affine2& a, const Affine2& b);  // a after b
Affine2 invert(const Affine2& a);

}  // namespace holdfast
