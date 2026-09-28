// holdfast_run: run one or more tracker configs on one sequence under one degradation preset.
//
//   holdfast_run --source data/VisDrone2019-MOT-val/sequences/<seq> \
//                --gt data/VisDrone2019-MOT-val/annotations/<seq>.txt \
//                --configs baseline,full --degrade heavy --seed 42 \
//                --out-dir results/heavy --render out.mp4

#include <format>
#include <iostream>

#include "cli.hpp"
#include "holdfast/app/pipeline.hpp"

namespace {
const char* kUsage = R"(usage: holdfast_run --source DIR [options]
  --gt FILE            VisDrone annotations (needed for --detector gt)
  --detector gt|onnx   default gt (ground truth + realistic noise)
  --model PATH         ONNX model, default models/yolox_nano.onnx
  --configs LIST       comma list of baseline,cmc,full,full_nocmc (default full)
  --degrade PRESET     clean|light|heavy|blackout|freeze (default clean)
  --seed N             default 42
  --fps F              timestamps for image sequences, default 30
  --out-dir DIR        writes DIR/trackers/<config>/data/<seq>.txt, DIR/dets, DIR/events
  --render FILE.mp4    annotated video, one panel per config
  --cmc-log FILE.csv   per-frame camera-motion estimates
  --start N            first frame (1-based), default 1
  --frames N           max frames, default all
  --threads N          default 1
  --quiet
)";
}

int main(int argc, char** argv) {
    holdfast::cli::Args a(argc, argv);
    if (!a.has("source") || a.has("help")) {
        std::cerr << kUsage;
        return a.has("help") ? 0 : 2;
    }
    holdfast::RunOptions o;
    o.source = a.get("source");
    o.gt = a.get("gt");
    o.detector = a.get("detector", "gt");
    o.model = a.get("model", o.model);
    o.configs = a.list("configs", "full");
    o.degrade = a.get("degrade", "clean");
    o.seed = static_cast<std::uint64_t>(a.num("seed", 42));
    o.fps = a.num("fps", 30);
    o.out_dir = a.get("out-dir");
    o.render = a.get("render");
    o.cmc_log = a.get("cmc-log");
    o.start_frame = static_cast<int>(a.num("start", 1));
    o.max_frames = static_cast<int>(a.num("frames", 0));
    o.threads = static_cast<int>(a.num("threads", 1));
    o.quiet = a.has("quiet");

    try {
        const auto r = holdfast::run_pipeline(o);
        if (!o.quiet) {
            std::cout << std::format("{}  preset={}  frames {} delivered {} stale {}  cmc-fallbacks {}  {:.1f}s\n",
                                     r.sequence, o.degrade, r.frames_total, r.frames_delivered, r.frames_stale,
                                     r.cmc_failures, r.wall_s);
            for (const auto& t : r.trackers) {
                std::cout << std::format("  {:<11} live-IDSW {:>5}   recoveries {:>5} (correct {:>5})\n", t.config,
                                         t.id_switches_live, t.recoveries, t.recoveries_correct);
            }
            std::cout << r.timer.table();
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
