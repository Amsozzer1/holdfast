#include "holdfast/kalman.hpp"

#include <Eigen/Cholesky>
#include <algorithm>
#include <cmath>

namespace holdfast {

namespace {
using Mat48 = Eigen::Matrix<double, 4, 8>;

Mat48 H() {
    Mat48 h = Mat48::Zero();
    h.leftCols<4>().setIdentity();
    return h;
}

KalmanBoxFilter::Vec4 to_z(const BBox& b) {
    return {b.cx(), b.cy(), b.w, b.h};
}
}  // namespace

KalmanBoxFilter::KalmanBoxFilter(const BBox& first, Params p) : p_(p) {
    x_.setZero();
    x_.head<4>() = to_z(first);
    const double w = first.w, h = first.h;
    const double sp = p_.std_weight_position, sv = p_.std_weight_velocity / p_.nominal_dt;
    Vec8 std;
    std << 2 * sp * w, 2 * sp * h, 2 * sp * w, 2 * sp * h,
           10 * sv * w, 10 * sv * h, 10 * sv * w, 10 * sv * h;
    P_ = std.array().square().matrix().asDiagonal();
}

void KalmanBoxFilter::predict(double dt) {
    dt = std::max(dt, 0.0);
    Mat8 F = Mat8::Identity();
    F.topRightCorner<4, 4>() = Mat4::Identity() * dt;

    // Random-walk process noise: variance per nominal frame, scaled by elapsed time.
    const double w = std::max(x_(2), 1.0), h = std::max(x_(3), 1.0);
    const double sp = p_.std_weight_position, sv = p_.std_weight_velocity / p_.nominal_dt;
    Vec8 std;
    std << sp * w, sp * h, sp * w, sp * h, sv * w, sv * h, sv * w, sv * h;
    const Mat8 Q = Mat8(std.array().square().matrix().asDiagonal()) * (dt / p_.nominal_dt);

    x_ = F * x_;
    P_ = F * P_ * F.transpose() + Q;
    // Keep size positive while coasting on a shrinking velocity.
    x_(2) = std::max(x_(2), 1.0);
    x_(3) = std::max(x_(3), 1.0);
}

KalmanBoxFilter::Mat4 KalmanBoxFilter::measurement_noise() const {
    const double w = std::max(x_(2), 1.0), h = std::max(x_(3), 1.0);
    const double sp = p_.std_weight_position;
    Vec4 std(sp * w, sp * h, sp * w, sp * h);
    return std.array().square().matrix().asDiagonal();
}

void KalmanBoxFilter::update(const BBox& z) {
    const Mat48 Hm = H();
    const Mat4 S = Hm * P_ * Hm.transpose() + measurement_noise();
    const Eigen::Matrix<double, 8, 4> K = S.ldlt().solve(Hm * P_).transpose();
    x_ += K * (to_z(z) - Hm * x_);
    P_ = (Mat8::Identity() - K * Hm) * P_;
    P_ = 0.5 * (P_ + P_.transpose());  // keep it symmetric against round-off
}

double KalmanBoxFilter::gating_distance(const BBox& z, bool position_only) const {
    const Mat48 Hm = H();
    const Mat4 S = Hm * P_ * Hm.transpose() + measurement_noise();
    const Vec4 d = to_z(z) - Hm * x_;
    if (position_only) {
        const Eigen::Matrix2d S2 = S.topLeftCorner<2, 2>();
        const Eigen::Vector2d d2 = d.head<2>();
        return d2.dot(S2.ldlt().solve(d2));
    }
    return d.dot(S.ldlt().solve(d));
}

void KalmanBoxFilter::apply_affine(const Affine2& A) {
    // Position and its velocity get the full 2x2 linear part; size and its velocity get the
    // isotropic scale only (rotating a (w, h) pair as if it were a vector is meaningless).
    Eigen::Matrix2d R;
    R << A.m[0], A.m[1], A.m[3], A.m[4];
    const double s = std::sqrt(std::abs(R(0, 0) * R(1, 1) - R(0, 1) * R(1, 0)));
    Mat8 T = Mat8::Zero();
    T.block<2, 2>(0, 0) = R;
    T.block<2, 2>(4, 4) = R;
    T(2, 2) = T(3, 3) = T(6, 6) = T(7, 7) = s;
    x_ = T * x_;
    x_(0) += A.m[2];
    x_(1) += A.m[5];
    P_ = T * P_ * T.transpose();
}

BBox KalmanBoxFilter::box() const {
    return BBox::from_center(static_cast<float>(x_(0)), static_cast<float>(x_(1)),
                             static_cast<float>(x_(2)), static_cast<float>(x_(3)));
}

}  // namespace holdfast
