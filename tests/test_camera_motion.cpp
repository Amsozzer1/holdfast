#include <gtest/gtest.h>

#include <opencv2/imgproc.hpp>

#include "holdfast/app/camera_motion.hpp"
#include "holdfast/rng.hpp"

using namespace holdfast;

namespace {
cv::Mat textured(int w, int h) {
    cv::Mat img(h, w, CV_8UC3, cv::Scalar(90, 90, 90));
    Rng rng(3);
    for (int i = 0; i < 600; ++i) {
        const cv::Point c(rng.uniform_int(0, w - 1), rng.uniform_int(0, h - 1));
        cv::circle(img, c, rng.uniform_int(3, 14), cv::Scalar(rng.uniform_int(0, 255), rng.uniform_int(0, 255), rng.uniform_int(0, 255)), cv::FILLED);
    }
    cv::GaussianBlur(img, img, cv::Size(3, 3), 0);
    return img;
}
}  // namespace

TEST(CameraMotion, RecoversKnownTranslationWithCorrectSign) {
    const cv::Mat a = textured(1280, 720);
    cv::Mat b;
    // Content moves +30 px right, -12 px up between frames.
    const cv::Mat M = (cv::Mat_<double>(2, 3) << 1, 0, 30, 0, 1, -12);
    cv::warpAffine(a, b, M, a.size(), cv::INTER_LINEAR, cv::BORDER_REFLECT);
    CameraMotionEstimator est;
    est.estimate(a);
    const Affine2 A = est.estimate(b);
    EXPECT_NEAR(A.m[2], 30, 1.0);
    EXPECT_NEAR(A.m[5], -12, 1.0);
    EXPECT_NEAR(A.m[0], 1, 0.01);
    EXPECT_FALSE(est.last_fit_failed());
}

TEST(CameraMotion, RepeatedFrameIsDetectedAsStale) {
    const cv::Mat a = textured(640, 360);
    CameraMotionEstimator est;
    est.estimate(a);
    const Affine2 A = est.estimate(a.clone());
    EXPECT_TRUE(A.is_identity());
    EXPECT_LT(est.last_frame_difference(), 0.5);
}

TEST(CameraMotion, FeaturelessFrameFallsBackToIdentity) {
    const cv::Mat a(360, 640, CV_8UC3, cv::Scalar(10, 10, 10));
    cv::Mat b(360, 640, CV_8UC3, cv::Scalar(40, 40, 40));
    CameraMotionEstimator est;
    est.estimate(a);
    EXPECT_TRUE(est.estimate(b).is_identity());
    EXPECT_TRUE(est.last_fit_failed());
}
