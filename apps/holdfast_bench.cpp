// holdfast_bench: per-stage latency (p50/p95) and throughput of the full onboard pipeline
// (decode -> camera-motion -> detect -> track) with the real ONNX detector.
//
//   holdfast_bench --source data/.../sequences/<seq> --frames 300 --threads 1

#include <format>
#include <iostream>
#include <thread>

#include "cli.hpp"
#include "holdfast/app/pipeline.hpp"

int main(int argc, char** argv) {
    holdfast::cli::Args a(argc, argv);
    if (!a.has("source")) {
        std::cerr << "usage: holdfast_bench --source DIR [--model PATH] [--frames 300] [--threads 1] [--detector onnx|gt --gt FILE]\n";
        return 2;
    }
    holdfast::RunOptions o;
    o.source = a.get("source");
    o.gt = a.get("gt");
    o.detector = a.get("detector", "onnx");
    o.model = a.get("model", o.model);
    o.max_frames = static_cast<int>(a.num("frames", 300));
    o.threads = static_cast<int>(a.num("threads", 1));
    o.configs = {"full"};
    try {
        const auto r = holdfast::run_pipeline(o);
        double total_p50 = 0;
        for (const auto& s : r.timer.summary()) total_p50 += s.p50;
        std::cout << std::format("sequence {}  frames {}  threads {}  hw threads {}\n", r.sequence, r.frames_delivered,
                                 o.threads, std::thread::hardware_concurrency());
        std::cout << r.timer.table();
        std::cout << std::format("end-to-end: {:.1f} fps over {:.1f}s  (sum of stage p50 {:.1f} ms)\n",
                                 r.frames_delivered / r.wall_s, r.wall_s, total_p50);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
