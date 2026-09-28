#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "holdfast/tracker.hpp"

namespace holdfast {

// MOTChallenge result lines: frame,id,x,y,w,h,conf,-1,-1,-1
class MotWriter {
public:
    explicit MotWriter(const std::string& path);
    void write(int frame, const std::vector<TrackOutput>& tracks);

private:
    std::ofstream out_;
};

// The exact detection stream a tracker saw, one JSON object per delivered frame, so a
// reference tracker (Python) can be run on identical input.
class DetectionDumpWriter {
public:
    explicit DetectionDumpWriter(const std::string& path);
    void write(int true_frame, double t, bool stale, const std::vector<Detection>& dets);

private:
    std::ofstream out_;
};

// Events as JSON lines: {"frame":..,"tracker":..,"kind":"recovered","id":..,"gap_s":..,"correct":..}
class EventWriter {
public:
    explicit EventWriter(const std::string& path);
    void write(int true_frame, const std::string& tracker, const TrackEvent& e);
    void write_note(int true_frame, const std::string& kind);

private:
    std::ofstream out_;
};

}  // namespace holdfast
