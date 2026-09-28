#pragma once

#include <opencv2/core.hpp>

#include "holdfast/types.hpp"

namespace holdfast {

// Global camera-motion estimate between consecutive *received* frames.
// Sparse features on a downscaled grayscale image, pyramidal Lucas-Kanade, then a
// RANSAC similarity fit (rotation + uniform scale + translation). Moving objects are
// outliers to the dominant background motion, so RANSAC rejects them.
class CameraMotionEstimator {
public:
    struct Params {
        int work_width = 640;     // downscale for speed
        int max_corners = 400;
        double quality = 0.01;
        double min_distance = 8;
        double ransac_px = 1.5;   // at work resolution
        int min_inliers = 25;
    };

    CameraMotionEstimator() = default;
    explicit CameraMotionEstimator(Params p) : p_(p) {}

    // Returns the affine mapping previous-frame pixels to this frame (full resolution),
    // or identity on the first frame / when the fit is unreliable.
    Affine2 estimate(const cv::Mat& bgr);
    [[nodiscard]] bool last_fit_failed() const { return failed_; }
    [[nodiscard]] int last_inliers() const { return inliers_; }

    // Mean absolute difference (0-255) of this frame against the previous one at work
    // resolution; ~0 means a repeated (frozen) frame. Valid after estimate().
    [[nodiscard]] double last_frame_difference() const { return diff_; }

private:
    Params p_{};
    cv::Mat prev_gray_;
    bool failed_ = false;
    int inliers_ = 0;
    double diff_ = 255;
};

}  // namespace holdfast
