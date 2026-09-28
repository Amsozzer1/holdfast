#include "holdfast/app/pipeline.hpp"

#include <chrono>
#include <cstdio>
#include <map>
#include <memory>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgproc.hpp>
#include <optional>
#include <stdexcept>

#include "holdfast/app/camera_motion.hpp"
#include "holdfast/app/image_source.hpp"
#include "holdfast/app/onnx_detector.hpp"
#include "holdfast/app/renderer.hpp"
#include "holdfast/degrade.hpp"
#include "holdfast/gt_detector.hpp"
#include "holdfast/mot_io.hpp"
#include "holdfast/tracker.hpp"

namespace holdfast {

namespace {

std::string title_for(const std::string& config) {
    if (config == "baseline") return "BASELINE  (ByteTrack-style, frame-index)";
    if (config == "cmc") return "BASELINE + camera-motion comp.";
    if (config == "full") return "HOLDFAST  (full)";
    if (config == "full_nocmc") return "HOLDFAST  (no camera-motion comp.)";
    return config;
}

// Live identity-switch estimate for the video overlay: a GT object seen under a different
// track id than last time. TrackEval computes the real numbers; this is only for display.
struct LiveIdStats {
    std::map<int, int> gt_to_track;
    int switches = 0;
    void observe(const std::vector<TrackOutput>& out) {
        for (const auto& t : out) {
            if (!t.matched_this_frame || t.last_gt_id < 0) continue;
            auto [it, inserted] = gt_to_track.try_emplace(t.last_gt_id, t.id);
            if (!inserted && it->second != t.id) { ++switches; it->second = t.id; }
        }
    }
};

}  // namespace

RunResult run_pipeline(const RunOptions& opt) {
    cv::setNumThreads(opt.threads);
    const auto wall0 = std::chrono::steady_clock::now();

    ImageDirSource source(opt.source);
    RunResult res;
    res.sequence = source.name();

    const int last = opt.max_frames > 0 ? std::min(source.size(), opt.start_frame + opt.max_frames - 1) : source.size();
    const int n = last - opt.start_frame + 1;
    res.frames_total = n;

    const auto degrade = DegradeConfig::preset(opt.degrade);
    if (!degrade) throw std::runtime_error("unknown --degrade preset: " + opt.degrade);
    // Schedule indices are 1..n; offset back to real frame numbers.
    Schedule sched = plan_schedule(n, opt.fps, *degrade, opt.seed);
    const int off = opt.start_frame - 1;
    std::map<int, Delivery> delivered;
    for (auto d : sched.delivered) {
        d.true_frame += off;
        d.content_frame += off;
        delivered[d.true_frame] = d;
    }
    res.frames_delivered = static_cast<int>(sched.delivered.size());

    std::optional<GroundTruthDetector> gt_det;
    std::unique_ptr<OnnxDetector> onnx;
    if (opt.detector == "gt") {
        if (opt.gt.empty()) throw std::runtime_error("--detector gt needs --gt <annotations.txt>");
        gt_det.emplace(load_visdrone_gt(opt.gt), source.frame_size().width, source.frame_size().height, GtNoise{}, opt.seed);
    } else if (opt.detector == "onnx") {
        onnx = std::make_unique<OnnxDetector>(OnnxDetector::Params{.model_path = opt.model, .threads = opt.threads});
    } else {
        throw std::runtime_error("unknown --detector: " + opt.detector);
    }
    DetectionDropout dropout(degrade->dropout, opt.seed);

    std::vector<Tracker> trackers;
    std::vector<LiveIdStats> live(opt.configs.size());
    std::vector<std::unique_ptr<MotWriter>> writers;
    for (const auto& name : opt.configs) {
        auto cfg = TrackerConfig::by_name(name);
        if (!cfg) throw std::runtime_error("unknown tracker config: " + name);
        cfg->nominal_dt = 1.0 / opt.fps;
        trackers.emplace_back(*cfg);
        res.trackers.push_back({name});
        if (!opt.out_dir.empty()) {
            writers.push_back(std::make_unique<MotWriter>(opt.out_dir + "/trackers/" + name + "/data/" + res.sequence + ".txt"));
        }
    }
    std::unique_ptr<DetectionDumpWriter> dump;
    std::unique_ptr<EventWriter> events;
    if (!opt.out_dir.empty()) {
        dump = std::make_unique<DetectionDumpWriter>(opt.out_dir + "/dets/" + res.sequence + ".jsonl");
        events = std::make_unique<EventWriter>(opt.out_dir + "/events/" + res.sequence + ".jsonl");
    }
    std::unique_ptr<Renderer> renderer;
    if (!opt.render.empty()) {
        renderer = std::make_unique<Renderer>(opt.render, source.frame_size(), static_cast<int>(trackers.size()), opt.fps);
    }

    CameraMotionEstimator cmc;
    std::FILE* cmc_log = opt.cmc_log.empty() ? nullptr : std::fopen(opt.cmc_log.c_str(), "w");
    if (cmc_log) std::fprintf(cmc_log, "frame,a,b,tx,c,d,ty,inliers,diff\n");
    cv::Mat img;
    int loaded_content = -1;

    auto make_panels = [&](bool preview, double t) {
        std::vector<Renderer::Panel> panels;
        for (size_t k = 0; k < trackers.size(); ++k) {
            Renderer::Panel p;
            p.title = title_for(opt.configs[k]);
            p.tracks = preview ? trackers[k].preview(t) : trackers[k].all_tracks();
            p.id_switches = live[k].switches;
            p.recoveries = res.trackers[k].recoveries;
            p.recoveries_correct = res.trackers[k].recoveries_correct;
            p.has_ground_truth = gt_det.has_value();
            panels.push_back(std::move(p));
        }
        return panels;
    };
    const std::string footer = "degrade: " + opt.degrade + "   detector: " + opt.detector;

    for (int f = opt.start_frame; f <= last; ++f) {
        const auto it = delivered.find(f);
        if (it == delivered.end()) {
            // Blackout: nothing arrives. Only the video shows it, with what the trackers are holding.
            if (renderer) {
                const double t = (f - 1) / opt.fps;
                renderer->draw({}, make_panels(true, t), Renderer::Banner::Blackout, Affine2::identity(), f, t, footer);
            }
            continue;
        }
        const Delivery& d = it->second;

        {
            auto s = res.timer.scope("decode");
            if (d.content_frame != loaded_content) {
                img = source.load(d.content_frame);
                if (degrade->blur_sigma > 0 && onnx) cv::GaussianBlur(img, img, cv::Size(), degrade->blur_sigma);
                loaded_content = d.content_frame;
            }
        }
        Affine2 A;
        bool stale = false;
        {
            auto s = res.timer.scope("cmc");
            A = cmc.estimate(img);
            stale = cmc.last_frame_difference() < 0.5;
            if (!stale && cmc.last_fit_failed()) ++res.cmc_failures;
            if (cmc_log) {
                std::fprintf(cmc_log, "%d,%.6f,%.6f,%.3f,%.6f,%.6f,%.3f,%d,%.3f\n", d.true_frame, A.m[0], A.m[1], A.m[2],
                             A.m[3], A.m[4], A.m[5], cmc.last_inliers(), cmc.last_frame_difference());
            }
        }
        if (stale) ++res.frames_stale;
        std::vector<Detection> dets;
        {
            auto s = res.timer.scope("detect");
            dets = gt_det ? gt_det->detect(d.content_frame) : onnx->detect(img);
            dets = dropout.apply(std::move(dets));
        }
        if (dump) dump->write(d.true_frame, d.t, stale, dets);

        {
            auto s = res.timer.scope("track");
            for (size_t k = 0; k < trackers.size(); ++k) {
                const auto out = trackers[k].update({d.t, dets, A, stale});
                if (!writers.empty()) writers[k]->write(d.true_frame, out);
                live[k].observe(out);
                for (const auto& e : trackers[k].events()) {
                    if (e.kind == TrackEvent::Kind::Recovered) {
                        ++res.trackers[k].recoveries;
                        if (e.correct) ++res.trackers[k].recoveries_correct;
                    }
                    if (events) events->write(d.true_frame, opt.configs[k], e);
                }
                trackers[k].clear_events();
            }
        }
        if (renderer) {
            auto s = res.timer.scope("render");
            renderer->draw(img, make_panels(false, d.t), stale ? Renderer::Banner::Frozen : Renderer::Banner::None, A,
                           d.true_frame, d.t, footer);
        }
    }
    if (cmc_log) std::fclose(cmc_log);
    for (size_t k = 0; k < trackers.size(); ++k) res.trackers[k].id_switches_live = live[k].switches;
    res.wall_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall0).count();
    return res;
}

}  // namespace holdfast
