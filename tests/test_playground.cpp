#include <gtest/gtest.h>

#include <cmath>

#include "holdfast/playground.hpp"

using namespace holdfast;

TEST(Simulator, SameSeedSameStream) {
    Simulator a({}, 7), b({}, 7);
    for (int i = 0; i < 200; ++i) {
        const auto fa = a.step(), fb = b.step();
        ASSERT_EQ(fa.dets.size(), fb.dets.size());
        for (size_t k = 0; k < fa.dets.size(); ++k) ASSERT_FLOAT_EQ(fa.dets[k].box.x, fb.dets[k].box.x);
    }
}

TEST(Simulator, KeepsPopulationNearTarget) {
    SimParams p;
    p.target_objects = 40;
    Simulator s(p, 3);
    size_t min_seen = 1000, max_seen = 0;
    for (int i = 0; i < 900; ++i) {
        const auto f = s.step();
        if (i > 60) { min_seen = std::min(min_seen, f.truth.size()); max_seen = std::max(max_seen, f.truth.size()); }
    }
    EXPECT_GT(min_seen, 15u);
    EXPECT_LT(max_seen, 90u);
}

TEST(Simulator, BlackoutAndFreeze) {
    Simulator s({}, 1);
    for (int i = 0; i < 10; ++i) s.step();
    s.blackout(1.0);
    for (int i = 0; i < 30; ++i) EXPECT_FALSE(s.step().delivered);
    EXPECT_TRUE(s.step().delivered);
    const auto before = s.step();
    s.freeze(0.5);
    for (int i = 0; i < 15; ++i) {
        const auto f = s.step();
        EXPECT_TRUE(f.stale);
        ASSERT_EQ(f.dets.size(), before.dets.size());
        EXPECT_TRUE(f.camera_motion.is_identity());
    }
    EXPECT_FALSE(s.step().stale);
}

TEST(Simulator, CameraMotionEstimateMatchesTruth) {
    SimParams p;
    p.cmc_noise_px = 0;
    Simulator s(p, 5);
    auto prev = s.step();
    for (int i = 0; i < 120; ++i) {
        const auto f = s.step();
        if (f.cmc_failed) continue;
        // The estimate maps the previous view to the current one.
        const Affine2 expect = compose(f.view, invert(prev.view));
        for (int k = 0; k < 6; ++k) ASSERT_NEAR(f.camera_motion.m[k], expect.m[k], 1e-6);
        prev = f;
    }
}

TEST(Playground, FullTrackerKeepsIdentityBetterUnderDegradation) {
    SimParams p;
    p.auto_blackout_every_s = 3;
    p.auto_freeze_every_s = 4;
    p.dropout = 0.1;
    Playground pg(42, "baseline", "full", p);
    Playground::Frame f;
    for (int i = 0; i < 30 * 60; ++i) f = pg.step();
    EXPECT_LT(f.sides[1].id_switches, f.sides[0].id_switches * 0.7)
        << "baseline " << f.sides[0].id_switches << " full " << f.sides[1].id_switches;
}
