#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "holdfast/stage_timer.hpp"

namespace holdfast {

struct RunOptions {
    std::string source;            // image directory
    std::string gt;                // VisDrone annotation file (required for --detector gt)
    std::string detector = "gt";   // gt | onnx
    std::string model = "models/yolox_nano.onnx";
    int threads = 1;
    std::vector<std::string> configs = {"full"};
    std::string degrade = "clean";
    std::uint64_t seed = 42;
    double fps = 30;
    std::string out_dir;           // results root for this preset; empty = no files
    std::string render;            // mp4 path; empty = no video
    std::string cmc_log;           // CSV of per-frame camera-motion estimates; empty = off
    int max_frames = 0;            // 0 = all
    int start_frame = 1;
    bool quiet = false;
};

struct TrackerSummary {
    std::string config;
    int id_switches_live = 0;
    int recoveries = 0;
    int recoveries_correct = 0;
};

struct RunResult {
    std::string sequence;
    int frames_total = 0;
    int frames_delivered = 0;
    int frames_stale = 0;
    int cmc_failures = 0;
    double wall_s = 0;
    std::vector<TrackerSummary> trackers;
    StageTimer timer;
};

RunResult run_pipeline(const RunOptions& opt);

}  // namespace holdfast
