#include "holdfast/sim.hpp"

#include <algorithm>
#include <cmath>

namespace holdfast {

namespace {
constexpr double kWorldW = 4200, kWorldH = 3000;
constexpr double kRoadHalf = 36;   // half road width
constexpr double kLane = 16;       // lane offset from road centre
}  // namespace

Affine2 compose(const Affine2& a, const Affine2& b) {
    const auto& A = a.m;
    const auto& B = b.m;
    Affine2 r;
    r.m = {A[0] * B[0] + A[1] * B[3], A[0] * B[1] + A[1] * B[4], A[0] * B[2] + A[1] * B[5] + A[2],
           A[3] * B[0] + A[4] * B[3], A[3] * B[1] + A[4] * B[4], A[3] * B[2] + A[4] * B[5] + A[5]};
    return r;
}

Affine2 invert(const Affine2& a) {
    const auto& A = a.m;
    const double det = A[0] * A[4] - A[1] * A[3];
    Affine2 r;
    r.m = {A[4] / det, -A[1] / det, 0, -A[3] / det, A[0] / det, 0};
    r.m[2] = -(r.m[0] * A[2] + r.m[1] * A[5]);
    r.m[5] = -(r.m[3] * A[2] + r.m[4] * A[5]);
    return r;
}

Simulator::Simulator(SimParams p, std::uint64_t seed) : p_(p), rng_(seed) {
    road_y_ = {800, 1500, 2250};
    road_x_ = {900, 1750, 2600, 3400};
    for (double y : road_y_) roads_.push_back({0, static_cast<float>(y - kRoadHalf), static_cast<float>(kWorldW), static_cast<float>(2 * kRoadHalf)});
    for (double x : road_x_) roads_.push_back({static_cast<float>(x - kRoadHalf), 0, static_cast<float>(2 * kRoadHalf), static_cast<float>(kWorldH)});
    // A bridge across a road and tree canopies: things that hide objects from above.
    occluders_ = {{1180, 1440, 150, 120}, {2050, 740, 110, 120}, {1690, 1880, 120, 150},
                  {2900, 1440, 170, 120}, {2540, 1000, 120, 130}, {3100, 2190, 140, 120}};
    // Blocks between roads (drawn as rooftops; they make camera motion visible).
    for (size_t i = 0; i + 1 < road_x_.size(); ++i) {
        for (size_t j = 0; j + 1 < road_y_.size(); ++j) {
            const double x0 = road_x_[i] + 90, x1 = road_x_[i + 1] - 90, y0 = road_y_[j] + 90, y1 = road_y_[j + 1] - 90;
            const double mx = 0.5 * (x0 + x1);
            buildings_.push_back({static_cast<float>(x0), static_cast<float>(y0), static_cast<float>(mx - x0 - 40), static_cast<float>(y1 - y0)});
            buildings_.push_back({static_cast<float>(mx + 40), static_cast<float>(y0), static_cast<float>(x1 - mx - 40), static_cast<float>(0.45 * (y1 - y0))});
        }
    }
    cam_x_ = wp_x_ = 1750;
    cam_y_ = wp_y_ = 1500;
    next_auto_blackout_ = p_.auto_blackout_every_s;
    next_auto_freeze_ = p_.auto_freeze_every_s;
    spawn_initial();
}

Affine2 Simulator::world_to_image(double cx, double cy, double theta) const {
    const double c = std::cos(theta), s = std::sin(theta);
    Affine2 a;
    a.m = {c, -s, 0, s, c, 0};
    a.m[2] = -(c * cx - s * cy) + p_.image_w / 2.0;
    a.m[5] = -(s * cx + c * cy) + p_.image_h / 2.0;
    return a;
}

bool Simulator::occluded(double x, double y) const {
    for (const auto& o : occluders_) {
        if (x >= o.x && x <= o.x + o.w && y >= o.y && y <= o.y + o.h) return true;
    }
    return false;
}

bool Simulator::spawn_one(bool inside_view) {
    const double half_w = p_.image_w / 2.0 + (inside_view ? -40 : 140);
    const double half_h = p_.image_h / 2.0 + (inside_view ? -40 : 140);
    Object o{};
    o.car = rng_.bernoulli(0.7);

    bool placed = false;
    if (o.car) {
        // A car on a road that crosses the (expanded) view, entering from the upstream edge.
        const bool horizontal = rng_.bernoulli(0.5);
        const double speed = rng_.uniform(70, 190);
        const int dir = rng_.bernoulli(0.5) ? 1 : -1;
        const auto& roads = horizontal ? road_y_ : road_x_;
        const double cam = horizontal ? cam_y_ : cam_x_, half = horizontal ? half_h : half_w;
        std::vector<double> candidates;
        for (double r : roads) if (std::abs(r - cam) < half) candidates.push_back(r);
        if (!candidates.empty()) {
            const double r = candidates[rng_.uniform_int(0, static_cast<int>(candidates.size()) - 1)];
            const double length = rng_.uniform(40, 56), width = rng_.uniform(20, 26);
            if (horizontal) {
                o.w = length; o.h = width;
                o.y = r + dir * kLane;
                o.x = inside_view ? cam_x_ + rng_.uniform(-half_w, half_w) : cam_x_ - dir * half_w;
                o.vx = dir * speed;
            } else {
                o.w = width; o.h = length;
                o.x = r - dir * kLane;
                o.y = inside_view ? cam_y_ + rng_.uniform(-half_h, half_h) : cam_y_ - dir * half_h;
                o.vy = dir * speed;
            }
            placed = true;
        }
    }
    if (!placed) {
        // A pedestrian (also the fallback when no road crosses the view).
        o.car = false;
        o.w = rng_.uniform(10, 14);
        o.h = rng_.uniform(10, 14);
        const double a = rng_.uniform(0, 2 * 3.14159265);
        const double speed = rng_.uniform(20, 45);
        o.vx = speed * std::cos(a);
        o.vy = speed * std::sin(a);
        if (inside_view) {
            o.x = cam_x_ + rng_.uniform(-half_w, half_w);
            o.y = cam_y_ + rng_.uniform(-half_h, half_h);
        } else {
            // Just outside a random edge, walking into the frame.
            const int edge = rng_.uniform_int(0, 3);
            const double along = rng_.uniform(-0.8, 0.8);
            const double inward = edge == 0 ? 0 : edge == 1 ? 3.14159265 : edge == 2 ? 1.5707963 : -1.5707963;
            const double h = inward + rng_.uniform(-0.6, 0.6);
            o.vx = speed * std::cos(h);
            o.vy = speed * std::sin(h);
            const double ew = p_.image_w / 2.0 + 20, eh = p_.image_h / 2.0 + 20;
            o.x = cam_x_ + (edge == 0 ? -ew : edge == 1 ? ew : along * ew);
            o.y = cam_y_ + (edge == 2 ? -eh : edge == 3 ? eh : along * eh);
        }
        o.heading_timer = rng_.uniform(2, 5);
    }
    if (o.x < 0 || o.y < 0 || o.x > kWorldW || o.y > kWorldH) return false;
    o.id = next_id_++;
    objects_.push_back(o);
    return true;
}

void Simulator::spawn_initial() {
    for (int i = 0, tries = 0; i < p_.target_objects && tries < 1000; ++tries) {
        if (spawn_one(true)) ++i;
    }
}

void Simulator::maintain_population() {
    const double keep_w = p_.image_w / 2.0 + 400, keep_h = p_.image_h / 2.0 + 400;
    std::erase_if(objects_, [&](const Object& o) {
        return std::abs(o.x - cam_x_) > keep_w || std::abs(o.y - cam_y_) > keep_h ||
               o.x < -100 || o.y < -100 || o.x > kWorldW + 100 || o.y > kWorldH + 100;
    });
    // Top up from just outside the edges, one object per frame, while the frame is under target.
    int visible = 0;
    for (const auto& o : objects_) {
        if (std::abs(o.x - cam_x_) < p_.image_w / 2.0 && std::abs(o.y - cam_y_) < p_.image_h / 2.0) ++visible;
    }
    if (visible < p_.target_objects && static_cast<int>(objects_.size()) < 3 * p_.target_objects) {
        for (int k = 0; k < 4 && !spawn_one(false); ++k) {}
    }
    // Lowering the target: drop objects that are out of frame first.
    if (visible > p_.target_objects + 10) {
        std::erase_if(objects_, [&](const Object& o) {
            return std::abs(o.x - cam_x_) > p_.image_w / 2.0 || std::abs(o.y - cam_y_) > p_.image_h / 2.0;
        });
    }
}

void Simulator::move_objects(double dt) {
    for (auto& o : objects_) {
        if (o.car) {
            // At an intersection, sometimes turn (non-linear motion, exactly what coasting gets wrong).
            const bool horizontal = std::abs(o.vx) > 0;
            const auto& cross = horizontal ? road_x_ : road_y_;
            for (int k = 0; k < static_cast<int>(cross.size()); ++k) {
                const double c = cross[k];
                const double pos = horizontal ? o.x : o.y;
                const double v = horizontal ? o.vx : o.vy;
                const double next = pos + v * dt;
                if (k == o.last_crossing || (pos - c) * (next - c) > 0) continue;
                o.last_crossing = k;
                if (!rng_.bernoulli(0.25)) continue;
                const double speed = std::abs(v);
                const int dir = rng_.bernoulli(0.5) ? 1 : -1;
                if (horizontal) {
                    o.x = c - dir * kLane;
                    o.vx = 0;
                    o.vy = dir * speed;
                } else {
                    o.y = c + dir * kLane;
                    o.vy = 0;
                    o.vx = dir * speed;
                }
                std::swap(o.w, o.h);
                // The road we just joined: the index refers to the other road family now.
                o.last_crossing = -1;
                const auto& other = horizontal ? road_y_ : road_x_;
                for (int q = 0; q < static_cast<int>(other.size()); ++q) {
                    if (std::abs(other[q] - (horizontal ? o.y : o.x)) < kRoadHalf + 1) o.last_crossing = q;
                }
                break;
            }
        } else {
            o.heading_timer -= dt;
            if (o.heading_timer <= 0) {
                const double a = std::atan2(o.vy, o.vx) + rng_.uniform(-1.2, 1.2);
                const double speed = std::hypot(o.vx, o.vy);
                o.vx = speed * std::cos(a);
                o.vy = speed * std::sin(a);
                o.heading_timer = rng_.uniform(2, 5);
            }
        }
        o.x += o.vx * dt;
        o.y += o.vy * dt;
    }
}

void Simulator::move_camera(double dt) {
    if (p_.camera == CameraMode::Static) return;
    // Wander between waypoints over the road grid.
    if (std::hypot(wp_x_ - cam_x_, wp_y_ - cam_y_) < 40) {
        wp_x_ = rng_.uniform(900, kWorldW - 900);
        wp_y_ = rng_.uniform(700, kWorldH - 700);
    }
    const double speed = p_.camera == CameraMode::Pan ? 35 : 55;
    const double d = std::max(1.0, std::hypot(wp_x_ - cam_x_, wp_y_ - cam_y_));
    const double tvx = speed * (wp_x_ - cam_x_) / d, tvy = speed * (wp_y_ - cam_y_) / d;
    cam_vx_ += (tvx - cam_vx_) * std::min(1.0, dt * 0.8);
    cam_vy_ += (tvy - cam_vy_) * std::min(1.0, dt * 0.8);
    cam_x_ += cam_vx_ * dt;
    cam_y_ += cam_vy_ * dt;
    cam_theta_ += 0.004 * dt * std::sin(frame_ / 90.0);

    if (p_.camera == CameraMode::Drone) {
        // Wind shake plus occasional sharp repositioning: 50-110 px over a few frames,
        // the kind of motion that breaks a pixel-space motion model.
        cam_x_ += rng_.normal(0, 0.8);
        cam_y_ += rng_.normal(0, 0.8);
        jerk_timer_ -= dt;
        if (jerk_timer_ <= 0 && jerk_frames_ == 0) {
            jerk_frames_ = rng_.uniform_int(3, 5);
            const double mag = rng_.uniform(50, 110), a = rng_.uniform(0, 2 * 3.14159265);
            jerk_dx_ = mag * std::cos(a) / jerk_frames_;
            jerk_dy_ = mag * std::sin(a) / jerk_frames_;
            jerk_timer_ = rng_.uniform(1.5, 3.5);
        }
        if (jerk_frames_ > 0) {
            cam_x_ += jerk_dx_;
            cam_y_ += jerk_dy_;
            --jerk_frames_;
        }
    }
    cam_x_ = std::clamp(cam_x_, p_.image_w / 2.0, kWorldW - p_.image_w / 2.0);
    cam_y_ = std::clamp(cam_y_, p_.image_h / 2.0, kWorldH - p_.image_h / 2.0);
}

std::vector<SimBox> Simulator::visible_truth(const Affine2& V) const {
    std::vector<SimBox> out;
    const auto& m = V.m;
    for (const auto& o : objects_) {
        // Axis-aligned bounds of the rotated world box.
        const double cx = m[0] * o.x + m[1] * o.y + m[2], cy = m[3] * o.x + m[4] * o.y + m[5];
        const double hw = 0.5 * (std::abs(m[0]) * o.w + std::abs(m[1]) * o.h);
        const double hh = 0.5 * (std::abs(m[3]) * o.w + std::abs(m[4]) * o.h);
        const double x0 = std::max(0.0, cx - hw), x1 = std::min<double>(p_.image_w, cx + hw);
        const double y0 = std::max(0.0, cy - hh), y1 = std::min<double>(p_.image_h, cy + hh);
        if (x1 - x0 < 0.5 * 2 * hw || y1 - y0 < 0.5 * 2 * hh) continue;  // mostly out of frame
        SimBox b;
        b.box = {static_cast<float>(x0), static_cast<float>(y0), static_cast<float>(x1 - x0), static_cast<float>(y1 - y0)};
        b.id = o.id;
        b.car = o.car;
        b.occluded = occluded(o.x, o.y);
        out.push_back(b);
    }
    return out;
}

std::vector<Detection> Simulator::detect(const std::vector<SimBox>& truth) {
    std::vector<Detection> out;
    for (const auto& t : truth) {
        const double miss = t.occluded ? p_.occluded_miss_prob : p_.miss_prob;
        const double n1 = rng_.normal(), n2 = rng_.normal(), n3 = rng_.normal(), n4 = rng_.normal();
        double score = rng_.normal(0.8, 0.1) - (t.occluded ? 0.35 : 0.0);
        if (rng_.bernoulli(miss) || rng_.bernoulli(p_.dropout)) continue;
        const double w = std::max(3.0, t.box.w * (1 + 0.05 * n3)), h = std::max(3.0, t.box.h * (1 + 0.05 * n4));
        Detection d;
        d.box = BBox::from_center(static_cast<float>(t.box.cx() + 0.05 * t.box.w * n1), static_cast<float>(t.box.cy() + 0.05 * t.box.h * n2),
                                  static_cast<float>(w), static_cast<float>(h));
        d.score = static_cast<float>(std::clamp(score, 0.05, 1.0));
        d.cls = t.car ? 1 : 0;
        d.gt_id = t.id;
        out.push_back(d);
    }
    for (int k = rng_.poisson(p_.false_positives); k > 0; --k) {
        Detection d;
        const double s = rng_.uniform(12, 45);
        d.box = {static_cast<float>(rng_.uniform(0, p_.image_w - s)), static_cast<float>(rng_.uniform(0, p_.image_h - s)),
                 static_cast<float>(s), static_cast<float>(s * rng_.uniform(0.5, 1.5))};
        d.score = static_cast<float>(rng_.uniform(0.1, 0.65));
        out.push_back(d);
    }
    return out;
}

void Simulator::blackout(double seconds) { blackout_left_ = std::max(blackout_left_, static_cast<int>(seconds * p_.fps)); }
void Simulator::freeze(double seconds) {
    if (blackout_left_ == 0) freeze_left_ = std::max(freeze_left_, static_cast<int>(seconds * p_.fps));
}

SimFrame Simulator::step() {
    const double dt = 1.0 / p_.fps;
    SimFrame f;
    f.index = frame_;
    f.t_true = frame_ * dt;

    if (frame_ > 0) {
        move_objects(dt);
        move_camera(dt);
    }
    maintain_population();

    if (p_.auto_blackout_every_s > 0 && f.t_true >= next_auto_blackout_) {
        blackout(rng_.uniform(0.5, 1.0));
        next_auto_blackout_ = f.t_true + p_.auto_blackout_every_s * rng_.uniform(0.7, 1.3);
    }
    if (p_.auto_freeze_every_s > 0 && f.t_true >= next_auto_freeze_) {
        freeze(rng_.uniform(0.3, 0.6));
        next_auto_freeze_ = f.t_true + p_.auto_freeze_every_s * rng_.uniform(0.7, 1.3);
    }

    const Affine2 view = world_to_image(cam_x_, cam_y_, cam_theta_);
    ++frame_;

    if (blackout_left_ > 0) {
        --blackout_left_;
        f.delivered = false;
        f.view = view;
        return f;
    }

    double stamp = f.t_true + (p_.jitter_ms > 0 ? rng_.normal(0, p_.jitter_ms / 1000.0) : 0.0);
    stamp = std::max(stamp, last_stamp_ + 1e-4);
    last_stamp_ = stamp;

    if (freeze_left_ > 0 && have_prev_) {
        --freeze_left_;
        SimFrame s = frozen_;  // same pixels, same detections, new timestamp
        s.index = f.index;
        s.t_true = f.t_true;
        s.t_stamp = stamp;
        s.stale = true;
        s.camera_motion = Affine2::identity();
        s.cmc_failed = false;
        return s;
    }
    freeze_left_ = 0;

    f.t_stamp = stamp;
    f.view = view;
    f.truth = visible_truth(view);
    f.dets = detect(f.truth);
    if (have_prev_) {
        // Ground-truth camera motion between delivered frames, plus estimator noise; the real
        // estimator (LK + RANSAC) gives up on very large motion, so this one does too.
        Affine2 A = compose(view, invert(prev_view_));
        // How far the image centre moved between the two frames.
        const double cx = p_.image_w / 2.0, cy = p_.image_h / 2.0;
        const double shift = std::hypot(A.m[0] * cx + A.m[1] * cy + A.m[2] - cx, A.m[3] * cx + A.m[4] * cy + A.m[5] - cy);
        if (shift > p_.cmc_fail_px) {
            f.cmc_failed = true;
            A = Affine2::identity();
        } else {
            A.m[2] += rng_.normal(0, p_.cmc_noise_px);
            A.m[5] += rng_.normal(0, p_.cmc_noise_px);
        }
        f.camera_motion = A;
    }
    have_prev_ = true;
    prev_view_ = view;
    frozen_ = f;
    return f;
}

}  // namespace holdfast
