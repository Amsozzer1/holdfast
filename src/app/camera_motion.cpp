#include "holdfast/app/camera_motion.hpp"

#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/video/tracking.hpp>

namespace holdfast {

Affine2 CameraMotionEstimator::estimate(const cv::Mat& bgr) {
    const double scale = static_cast<double>(p_.work_width) / bgr.cols;
    cv::Mat small, gray;
    cv::resize(bgr, small, cv::Size(), scale, scale, cv::INTER_AREA);
    cv::cvtColor(small, gray, cv::COLOR_BGR2GRAY);

    failed_ = false;
    inliers_ = 0;
    if (prev_gray_.empty() || prev_gray_.size() != gray.size()) {
        prev_gray_ = gray;
        diff_ = 255;
        return Affine2::identity();
    }
    cv::Mat d;
    cv::absdiff(gray, prev_gray_, d);
    diff_ = cv::mean(d)[0];
    if (diff_ < 0.5) {  // identical frame: no motion to estimate
        return Affine2::identity();
    }

    std::vector<cv::Point2f> p0, p1;
    cv::goodFeaturesToTrack(prev_gray_, p0, p_.max_corners, p_.quality, p_.min_distance);
    Affine2 out = Affine2::identity();
    if (p0.size() >= static_cast<size_t>(p_.min_inliers)) {
        std::vector<unsigned char> status;
        std::vector<float> err;
        cv::calcOpticalFlowPyrLK(prev_gray_, gray, p0, p1, status, err, cv::Size(21, 21), 3);
        std::vector<cv::Point2f> a, b;
        for (size_t i = 0; i < p0.size(); ++i) {
            if (status[i]) { a.push_back(p0[i]); b.push_back(p1[i]); }
        }
        if (a.size() >= static_cast<size_t>(p_.min_inliers)) {
            std::vector<unsigned char> inl;
            cv::Mat M = cv::estimateAffinePartial2D(a, b, inl, cv::RANSAC, p_.ransac_px);
            inliers_ = static_cast<int>(cv::countNonZero(inl));
            if (!M.empty() && inliers_ >= p_.min_inliers) {
                // Back to full resolution: linear part unchanged, translation / scale.
                out.m = {M.at<double>(0, 0), M.at<double>(0, 1), M.at<double>(0, 2) / scale,
                         M.at<double>(1, 0), M.at<double>(1, 1), M.at<double>(1, 2) / scale};
            }
        }
    }
    failed_ = out.is_identity();
    prev_gray_ = gray;
    return out;
}

}  // namespace holdfast
