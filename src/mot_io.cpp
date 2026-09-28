#include "holdfast/mot_io.hpp"

#include <filesystem>
#include <format>
#include <stdexcept>

namespace holdfast {

namespace {
std::ofstream open_out(const std::string& path) {
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write " + path);
    return out;
}
}  // namespace

MotWriter::MotWriter(const std::string& path) : out_(open_out(path)) {}

void MotWriter::write(int frame, const std::vector<TrackOutput>& tracks) {
    for (const auto& t : tracks) {
        out_ << std::format("{},{},{:.2f},{:.2f},{:.2f},{:.2f},{:.3f},-1,-1,-1\n", frame, t.id, t.box.x, t.box.y,
                            t.box.w, t.box.h, t.score);
    }
}

DetectionDumpWriter::DetectionDumpWriter(const std::string& path) : out_(open_out(path)) {}

void DetectionDumpWriter::write(int true_frame, double t, bool stale, const std::vector<Detection>& dets) {
    out_ << std::format(R"({{"frame":{},"t":{:.6f},"stale":{},"dets":[)", true_frame, t, stale ? "true" : "false");
    for (size_t i = 0; i < dets.size(); ++i) {
        const auto& d = dets[i];
        out_ << std::format("{}[{:.2f},{:.2f},{:.2f},{:.2f},{:.4f}]", i ? "," : "", d.box.x, d.box.y, d.box.w, d.box.h, d.score);
    }
    out_ << "]}\n";
}

EventWriter::EventWriter(const std::string& path) : out_(open_out(path)) {}

void EventWriter::write(int true_frame, const std::string& tracker, const TrackEvent& e) {
    out_ << std::format(R"({{"frame":{},"tracker":"{}","kind":"{}","id":{},"gap_s":{:.4f},"correct":{}}})", true_frame,
                        tracker, e.kind == TrackEvent::Kind::Recovered ? "recovered" : "deleted", e.track_id, e.gap_s,
                        e.correct ? "true" : "false")
         << '\n';
}

void EventWriter::write_note(int true_frame, const std::string& kind) {
    out_ << std::format(R"({{"frame":{},"kind":"{}"}})", true_frame, kind) << '\n';
}

}  // namespace holdfast
