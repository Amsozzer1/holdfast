#include <gtest/gtest.h>

#include <cmath>

#include "holdfast/kalman.hpp"

using namespace holdfast;

namespace {
KalmanBoxFilter moving_filter(double vx, double vy, double fps = 30.0) {
    // Feed 30 observations of a box moving at constant velocity (px/s).
    KalmanBoxFilter kf(BBox::from_center(100, 100, 20, 40));
    for (int i = 1; i <= 30; ++i) {
        kf.predict(1.0 / fps);
        kf.update(BBox::from_center(static_cast<float>(100 + vx * i / fps), static_cast<float>(100 + vy * i / fps), 20, 40));
    }
    return kf;
}
}  // namespace

TEST(Kalman, LearnsVelocityInPixelsPerSecond) {
    const auto kf = moving_filter(60, -30);
    EXPECT_NEAR(kf.mean()(4), 60, 3);
    EXPECT_NEAR(kf.mean()(5), -30, 3);
}

TEST(Kalman, CoastLandsWherePhysicsSays) {
    auto kf = moving_filter(60, -30);
    const double x0 = kf.mean()(0), y0 = kf.mean()(1);
    for (int i = 0; i < 20; ++i) kf.predict(1.0 / 30.0);
    const double t = 20.0 / 30.0;
    EXPECT_NEAR(kf.mean()(0), x0 + 60 * t, 3);
    EXPECT_NEAR(kf.mean()(1), y0 - 30 * t, 3);
}

TEST(Kalman, OneBigStepEqualsManySmallSteps) {
    // A blackout is one predict with a large dt: the mean must match stepping frame by frame.
    auto a = moving_filter(60, -30), b = a;
    a.predict(1.0);
    for (int i = 0; i < 30; ++i) b.predict(1.0 / 30.0);
    EXPECT_NEAR(a.mean()(0), b.mean()(0), 1e-6);
    EXPECT_NEAR(a.mean()(1), b.mean()(1), 1e-6);
}

TEST(Kalman, CovarianceGrowsMonotonicallyWhileCoasting) {
    auto kf = moving_filter(10, 10);
    double prev = kf.covariance().trace();
    for (int i = 0; i < 60; ++i) {
        kf.predict(1.0 / 30.0);
        const double tr = kf.covariance().trace();
        ASSERT_GT(tr, prev);
        prev = tr;
    }
}

TEST(Kalman, LongerGapMeansWiderGate) {
    auto a = moving_filter(0, 0), b = a;
    a.predict(0.1);
    b.predict(1.0);
    const BBox off = BBox::from_center(static_cast<float>(a.mean()(0) + 15), static_cast<float>(a.mean()(1)), 20, 40);
    EXPECT_LT(b.gating_distance(off, true), a.gating_distance(off, true));
}

TEST(Kalman, AffineTranslationMovesMeanNotCovariance) {
    auto kf = moving_filter(10, 0);
    const auto P = kf.covariance();
    const double x = kf.mean()(0), y = kf.mean()(1);
    Affine2 A;
    A.m = {1, 0, 25, 0, 1, -10};
    kf.apply_affine(A);
    EXPECT_NEAR(kf.mean()(0), x + 25, 1e-9);
    EXPECT_NEAR(kf.mean()(1), y - 10, 1e-9);
    EXPECT_TRUE(kf.covariance().isApprox(P));
}

TEST(Kalman, AffineRotationRotatesVelocity) {
    auto kf = moving_filter(60, 0);
    const double vx = kf.mean()(4), w = kf.mean()(2);
    Affine2 A;  // 90 degrees about the origin
    A.m = {0, -1, 0, 1, 0, 0};
    kf.apply_affine(A);
    EXPECT_NEAR(kf.mean()(5), vx, 1e-6);   // vx became vy
    EXPECT_NEAR(kf.mean()(4), 0, 0.5);
    EXPECT_NEAR(kf.mean()(2), w, 1e-6);    // size keeps scale 1
}
