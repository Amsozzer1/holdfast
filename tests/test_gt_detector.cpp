#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "holdfast/gt_detector.hpp"

using namespace holdfast;

namespace {
std::string write_sample() {
    const auto p = std::filesystem::temp_directory_path() / "holdfast_gt_sample.txt";
    std::ofstream out(p);
    out << "1,1,10,10,20,20,1,4,0,0\n"    // car, kept
        << "1,2,50,50,20,20,1,1,0,2\n"    // pedestrian, heavy occlusion
        << "1,3,90,90,20,20,0,0,0,0\n"    // ignored region, dropped
        << "1,4,90,90,20,20,1,3,0,0\n"    // tricycle, not evaluated
        << "2,1,12,10,20,20,1,4,0,0\n";
    return p.string();
}
}  // namespace

TEST(GtDetector, LoadsOnlyEvaluatedCategories) {
    const auto gt = load_visdrone_gt(write_sample());
    ASSERT_EQ(gt.at(1).size(), 2u);
    ASSERT_EQ(gt.at(2).size(), 1u);
}

TEST(GtDetector, DeterministicPerSeedAndFrame) {
    GtNoise noise;
    noise.miss_prob = 0;
    GroundTruthDetector a(load_visdrone_gt(write_sample()), 640, 480, noise, 42);
    GroundTruthDetector b(load_visdrone_gt(write_sample()), 640, 480, noise, 42);
    const auto da = a.detect(1), db = b.detect(1);
    ASSERT_EQ(da.size(), db.size());
    for (size_t i = 0; i < da.size(); ++i) {
        EXPECT_FLOAT_EQ(da[i].box.x, db[i].box.x);
        EXPECT_FLOAT_EQ(da[i].score, db[i].score);
    }
    // Calling out of order gives the same answer (frames are independently seeded).
    (void)b.detect(2);
    const auto da2 = b.detect(1);
    ASSERT_EQ(da2.size(), da.size());
    EXPECT_FLOAT_EQ(da2[0].box.x, da[0].box.x);
}

TEST(GtDetector, OccludedBoxesScoreLower) {
    GtNoise noise;
    noise.miss_prob = noise.miss_prob_partial = noise.miss_prob_heavy = 0;
    noise.false_positives_per_frame = 0;
    GroundTruthDetector d(load_visdrone_gt(write_sample()), 640, 480, noise, 1);
    double clear = 0, occl = 0;
    for (int s = 0; s < 200; ++s) {
        GroundTruthDetector ds(load_visdrone_gt(write_sample()), 640, 480, noise, s);
        for (const auto& x : ds.detect(1)) (x.gt_id == 1 ? clear : occl) += x.score;
    }
    EXPECT_GT(clear, occl + 50);
}
