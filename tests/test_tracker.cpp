#include <gtest/gtest.h>

#include "holdfast/tracker.hpp"

using namespace holdfast;

namespace {

Detection det(float cx, float cy, float score = 0.9f, int gt = 1) {
    Detection d;
    d.box = BBox::from_center(cx, cy, 30, 30);
    d.score = score;
    d.gt_id = gt;
    return d;
}

FrameInput frame(double t, std::vector<Detection> d) {
    FrameInput f;
    f.t = t;
    f.detections = std::move(d);
    return f;
}

// Runs one object at vx px/s for n frames at fps, starting at frame index `from`.
int run_linear(Tracker& tr, int from, int n, double fps, float vx) {
    int id = -1;
    for (int i = from; i < from + n; ++i) {
        const double t = i / fps;
        auto out = tr.update(frame(t, {det(static_cast<float>(100 + vx * t), 200)}));
        if (!out.empty()) id = out[0].id;
    }
    return id;
}

}  // namespace

TEST(Lifecycle, TentativeConfirmsAfterThreeHits) {
    Tracker tr;
    tr.update(frame(0.0, {}));  // not the first frame, so new tracks start tentative
    EXPECT_TRUE(tr.update(frame(1 / 30.0, {det(100, 100)})).empty());
    EXPECT_TRUE(tr.update(frame(2 / 30.0, {det(101, 100)})).empty());
    auto out = tr.update(frame(3 / 30.0, {det(102, 100)}));
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].state, TrackState::Confirmed);
}

TEST(Lifecycle, TentativeDiesOnFirstMiss) {
    Tracker tr;
    tr.update(frame(0.0, {}));
    tr.update(frame(1 / 30.0, {det(100, 100)}));
    tr.update(frame(2 / 30.0, {}));
    EXPECT_TRUE(tr.all_tracks().empty());
}

TEST(Lifecycle, ConfirmedGoesLostThenRecoversWithSameId) {
    Tracker tr;
    const int id = run_linear(tr, 0, 30, 30, 60);
    ASSERT_GT(id, 0);
    // 20-frame blackout: no calls at all, then the object reappears where it should be.
    const double t = 55 / 30.0;
    auto out = tr.update(frame(t, {det(static_cast<float>(100 + 60 * t), 200)}));
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].id, id);
    ASSERT_FALSE(tr.events().empty());
    EXPECT_EQ(tr.events().back().kind, TrackEvent::Kind::Recovered);
    EXPECT_TRUE(tr.events().back().correct);
}

TEST(Lifecycle, FrameIndexTrackerLosesIdAfterFastBlackout) {
    // Same scenario, fixed-dt baseline: it believes only one frame passed, predicts in the
    // wrong place, and IoU matching fails. This is the failure the full tracker fixes.
    Tracker tr(TrackerConfig::baseline());
    const int id = run_linear(tr, 0, 30, 30, 120);
    const double t = 55 / 30.0;
    tr.update(frame(t, {det(static_cast<float>(100 + 120 * t), 200)}));
    tr.update(frame(t + 1 / 30.0, {det(static_cast<float>(100 + 120 * (t + 1 / 30.0)), 200)}));
    auto out = tr.update(frame(t + 2 / 30.0, {det(static_cast<float>(100 + 120 * (t + 2 / 30.0)), 200)}));
    ASSERT_EQ(out.size(), 1u);
    EXPECT_NE(out[0].id, id);
}

class MaxAgeSeconds : public ::testing::TestWithParam<double> {};

TEST_P(MaxAgeSeconds, DeletedAfterMaxAgeRegardlessOfFps) {
    const double fps = GetParam();
    TrackerConfig cfg;
    cfg.max_age_s = 1.0;
    cfg.nominal_dt = 1.0 / fps;
    Tracker tr(cfg);
    run_linear(tr, 0, static_cast<int>(fps), fps, 0);
    const double t0 = (fps - 1) / fps;
    // Keep feeding empty frames; the track must survive < 1 s and be gone after > 1 s.
    for (int i = 1; t0 + i / fps < t0 + 0.9; ++i) tr.update(frame(t0 + i / fps, {}));
    EXPECT_EQ(tr.all_tracks().size(), 1u);
    for (int i = 0; i < static_cast<int>(fps * 0.3); ++i) tr.update(frame(t0 + 0.9 + (i + 1) / fps, {}));
    EXPECT_TRUE(tr.all_tracks().empty());
}

INSTANTIATE_TEST_SUITE_P(Fps, MaxAgeSeconds, ::testing::Values(10.0, 30.0));

TEST(OcReupdate, FixesVelocityAfterGapWithTurn) {
    // Object moves right for 1 s, then (unseen, during a 0.5 s gap) turns and moves down.
    // Compare the next prediction error: with the re-update the filter has already learned the turn.
    auto next_error = [](bool oc) {
        TrackerConfig cfg;
        cfg.oc_reupdate = oc;
        cfg.recovery_gate_chi2 = 1e9;
        Tracker tr(cfg);
        run_linear(tr, 0, 30, 30, 60);
        const float x_turn = 100 + 60.0f * (29 / 30.0f);
        auto truth_y = [&](double t) { return static_cast<float>(200 + 80 * (t - 29 / 30.0)); };
        const double t1 = 45 / 30.0;
        tr.update(frame(t1, {det(x_turn, truth_y(t1))}));
        const double t2 = 46 / 30.0;
        // Frame with no detection: read the coasted prediction.
        tr.update(frame(t2, {}));
        const auto p = tr.all_tracks().at(0).box;
        return std::hypot(p.cx() - x_turn, p.cy() - truth_y(t2));
    };
    EXPECT_LT(next_error(true), next_error(false));
}

TEST(StaleGuard, FrozenFramesCauseNoUpdates) {
    TrackerConfig cfg;
    Tracker tr(cfg);
    run_linear(tr, 0, 30, 30, 60);
    const double v_before = 60;
    // Feed the same (stale) detection 10 times with new timestamps, flagged stale.
    const auto stale_det = det(100 + 60 * (29 / 30.0f), 200);
    for (int i = 30; i < 40; ++i) {
        FrameInput f = frame(i / 30.0, {stale_det});
        f.stale = true;
        auto out = tr.update(f);
        ASSERT_EQ(out.size(), 1u);  // still reported, on its prediction
        EXPECT_FALSE(out[0].matched_this_frame);
    }
    // Velocity survived: the prediction kept moving at ~60 px/s.
    const auto b = tr.all_tracks().at(0).box;
    EXPECT_NEAR(b.cx(), 100 + v_before * (39 / 30.0), 6);
}

TEST(StaleGuard, WithoutGuardFrozenFramesKillVelocity) {
    Tracker tr(TrackerConfig::baseline());
    run_linear(tr, 0, 30, 30, 60);
    const auto stale_det = det(100 + 60 * (29 / 30.0f), 200);
    for (int i = 30; i < 40; ++i) {
        FrameInput f = frame(i / 30.0, {stale_det});
        f.stale = true;
        tr.update(f);
    }
    const auto b = tr.all_tracks().at(0).box;
    EXPECT_LT(b.cx(), 100 + 60 * (39 / 30.0) - 6);
}

TEST(CameraMotion, CompensationKeepsIdUnderLargePan) {
    // Static object; camera pans 40 px per frame. Without CMC the IoU drops to zero.
    auto run = [](bool cmc) {
        Tracker tr;
        int first = -1, last = -1;
        for (int i = 0; i < 20; ++i) {
            FrameInput f = frame(i / 30.0, {det(static_cast<float>(600 - 40 * i), 200)});
            if (cmc && i > 0) f.camera_motion.m = {1, 0, -40, 0, 1, 0};
            auto out = tr.update(f);
            if (!out.empty()) { if (first < 0) first = out[0].id; last = out[0].id; }
        }
        return first == last;
    };
    EXPECT_TRUE(run(true));
}
