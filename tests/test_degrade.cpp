#include <gtest/gtest.h>

#include "holdfast/degrade.hpp"

using namespace holdfast;

TEST(Degrade, SameSeedSameSchedule) {
    const auto cfg = *DegradeConfig::preset("heavy");
    const auto a = plan_schedule(900, 30, cfg, 42), b = plan_schedule(900, 30, cfg, 42);
    ASSERT_EQ(a.delivered.size(), b.delivered.size());
    for (size_t i = 0; i < a.delivered.size(); ++i) {
        EXPECT_EQ(a.delivered[i].true_frame, b.delivered[i].true_frame);
        EXPECT_EQ(a.delivered[i].content_frame, b.delivered[i].content_frame);
        EXPECT_DOUBLE_EQ(a.delivered[i].t, b.delivered[i].t);
    }
    EXPECT_EQ(a.blacked_out, b.blacked_out);
    const auto c = plan_schedule(900, 30, cfg, 43);
    EXPECT_NE(a.blacked_out, c.blacked_out);
}

TEST(Degrade, CleanDeliversEverything) {
    const auto s = plan_schedule(100, 30, *DegradeConfig::preset("clean"), 1);
    ASSERT_EQ(s.delivered.size(), 100u);
    EXPECT_TRUE(s.blacked_out.empty());
    for (const auto& d : s.delivered) EXPECT_FALSE(d.frozen());
}

TEST(Degrade, BlackoutBurstsAreContiguousAndInRange) {
    const auto cfg = *DegradeConfig::preset("blackout");
    const auto s = plan_schedule(1500, 30, cfg, 5);
    ASSERT_FALSE(s.blacked_out.empty());
    EXPECT_EQ(s.delivered.size() + s.blacked_out.size(), 1500u);
    // Split into bursts and check each length.
    int len = 1;
    for (size_t i = 1; i <= s.blacked_out.size(); ++i) {
        if (i < s.blacked_out.size() && s.blacked_out[i] == s.blacked_out[i - 1] + 1) { ++len; continue; }
        EXPECT_GE(len, cfg.blackout_min_frames);
        EXPECT_LE(len, cfg.blackout_max_frames);
        len = 1;
    }
}

TEST(Degrade, FreezeRepeatsContentAndTimestampsIncrease) {
    const auto s = plan_schedule(900, 30, *DegradeConfig::preset("freeze"), 3);
    int frozen = 0;
    for (size_t i = 1; i < s.delivered.size(); ++i) {
        EXPECT_GT(s.delivered[i].t, s.delivered[i - 1].t);
        if (s.delivered[i].frozen()) {
            ++frozen;
            EXPECT_LT(s.delivered[i].content_frame, s.delivered[i].true_frame);
        }
    }
    EXPECT_GT(frozen, 0);
}

TEST(Degrade, DropoutIsSeededAndRoughlyP) {
    std::vector<Detection> dets(1000);
    DetectionDropout a(0.3, 9), b(0.3, 9);
    const auto ra = a.apply(dets), rb = b.apply(dets);
    EXPECT_EQ(ra.size(), rb.size());
    EXPECT_NEAR(static_cast<double>(ra.size()), 700.0, 60.0);
}
