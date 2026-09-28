#pragma once

#include <cstdio>
#include <deque>
#include <map>
#include <opencv2/core.hpp>
#include <string>
#include <vector>

#include "holdfast/tracker.hpp"

namespace holdfast {

// Writes an annotated MP4 by piping raw frames into the ffmpeg CLI.
// One panel per tracker, side by side, all fed the same degraded input.
class Renderer {
public:
    struct Panel {
        std::string title;
        std::vector<TrackOutput> tracks;  // every live track (confirmed, lost, coasting)
        int id_switches = 0;              // live estimate from hidden GT ids
        int recoveries = 0;
        int recoveries_correct = 0;
        bool has_ground_truth = true;     // false: hide GT-based stats
    };
    enum class Banner { None, Blackout, Frozen };

    Renderer(const std::string& path, cv::Size source_size, int panels, double fps, int panel_width = 960);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // image may be empty (blackout). camera_motion warps the drawn trails.
    void draw(const cv::Mat& image, const std::vector<Panel>& panels, Banner banner, const Affine2& camera_motion,
              int frame, double t, const std::string& footer);

private:
    void draw_panel(cv::Mat& canvas, const Panel& p, std::map<int, std::deque<cv::Point2f>>& trails, Banner banner);

    FILE* pipe_ = nullptr;
    cv::Size src_, panel_;
    double scale_;
    int n_;
    std::vector<std::map<int, std::deque<cv::Point2f>>> trails_;  // per panel, full-res points
};

}  // namespace holdfast
