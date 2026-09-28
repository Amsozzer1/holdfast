#pragma once

#include <Eigen/Core>

#include "holdfast/types.hpp"

namespace holdfast {

struct KalmanParams {
    double std_weight_position = 1.0 / 20.0;   // measurement + position process noise
    double std_weight_velocity = 1.0 / 160.0;  // velocity process noise, per nominal frame
    double nominal_dt = 1.0 / 30.0;            // frame period the per-frame weights refer to
};

// Constant-velocity Kalman filter over x = [cx, cy, w, h, vx, vy, vw, vh].
// Velocities are in pixels per *second*, and every predict takes a real dt, so a
// blackout is one big step rather than a run of missing frame indices.
//
// Noise follows DeepSORT/BoT-SORT: standard deviations are proportional to box size
// (w for x-terms, h for y-terms). Process noise is a per-second rate, so the
// covariance grows linearly with the time a track goes unseen.
class KalmanBoxFilter {
public:
    using Vec8 = Eigen::Matrix<double, 8, 1>;
    using Mat8 = Eigen::Matrix<double, 8, 8>;
    using Vec4 = Eigen::Matrix<double, 4, 1>;
    using Mat4 = Eigen::Matrix<double, 4, 4>;

    using Params = KalmanParams;

    KalmanBoxFilter() = default;
    explicit KalmanBoxFilter(const BBox& first, Params p = {});

    void predict(double dt);
    void update(const BBox& z);
    // Squared Mahalanobis distance of z from the predicted measurement.
    // position_only uses (cx, cy) only: 2 DOF instead of 4.
    [[nodiscard]] double gating_distance(const BBox& z, bool position_only = false) const;
    // Warp the state into the next frame's coordinates (camera-motion compensation).
    void apply_affine(const Affine2& A);

    [[nodiscard]] BBox box() const;
    [[nodiscard]] const Vec8& mean() const { return x_; }
    [[nodiscard]] const Mat8& covariance() const { return P_; }
    void set_state(const Vec8& x, const Mat8& P) { x_ = x; P_ = P; }

private:
    [[nodiscard]] Mat4 measurement_noise() const;
    Params p_{};
    Vec8 x_ = Vec8::Zero();
    Mat8 P_ = Mat8::Identity();
};

}  // namespace holdfast
