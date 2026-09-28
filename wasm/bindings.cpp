// Embind wrapper around holdfast::Playground. Everything the page shows per frame comes back
// as a few typed arrays, so the JS side does no per-object allocation.

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>
#include <vector>

#include "holdfast/playground.hpp"

using emscripten::val;
using holdfast::CameraMode;
using holdfast::Playground;

namespace {

val f32(const std::vector<float>& v) {
    // Float32Array constructor copies, so the result survives wasm memory growth.
    return val::global("Float32Array").new_(emscripten::typed_memory_view(v.size(), v.data()));
}

val rects(const std::vector<holdfast::WorldRect>& rs) {
    std::vector<float> v;
    for (const auto& r : rs) v.insert(v.end(), {r.x, r.y, r.w, r.h});
    return f32(v);
}

}  // namespace

class PlaygroundJS {
public:
    PlaygroundJS(unsigned seed, const std::string& left, const std::string& right) : pg_(seed, left, right) {}

    // Advances one frame (1/30 s of simulated time).
    val step() {
        const auto fr = pg_.step();
        const auto& f = fr.sim;
        val out = val::object();
        out.set("frame", f.index);
        out.set("t", f.t_true);
        out.set("delivered", f.delivered);
        out.set("stale", f.stale);
        out.set("cmcFailed", f.cmc_failed);
        out.set("view", f32({static_cast<float>(f.view.m[0]), static_cast<float>(f.view.m[1]), static_cast<float>(f.view.m[2]),
                             static_cast<float>(f.view.m[3]), static_cast<float>(f.view.m[4]), static_cast<float>(f.view.m[5])}));
        out.set("motion", f32({static_cast<float>(f.camera_motion.m[0]), static_cast<float>(f.camera_motion.m[1]),
                               static_cast<float>(f.camera_motion.m[2]), static_cast<float>(f.camera_motion.m[3]),
                               static_cast<float>(f.camera_motion.m[4]), static_cast<float>(f.camera_motion.m[5])}));

        std::vector<float> truth;  // stride 7: x y w h id car occluded
        for (const auto& b : f.truth) {
            truth.insert(truth.end(), {b.box.x, b.box.y, b.box.w, b.box.h, static_cast<float>(b.id), b.car ? 1.f : 0.f, b.occluded ? 1.f : 0.f});
        }
        out.set("truth", f32(truth));
        std::vector<float> dets;  // stride 5: x y w h score
        for (const auto& d : f.dets) dets.insert(dets.end(), {d.box.x, d.box.y, d.box.w, d.box.h, d.score});
        out.set("dets", f32(dets));

        val sides = val::array();
        for (const auto& s : fr.sides) {
            val side = val::object();
            side.set("config", s.config);
            side.set("idSwitches", s.id_switches);
            side.set("recoveries", s.recoveries);
            side.set("recoveriesCorrect", s.recoveries_correct);
            side.set("updateMs", s.update_ms);
            std::vector<float> tr;  // stride 9: id x y w h state matched sigmaX sigmaY
            for (const auto& t : s.tracks) {
                tr.insert(tr.end(), {static_cast<float>(t.id), t.box.x, t.box.y, t.box.w, t.box.h, static_cast<float>(t.state),
                                     t.matched_this_frame ? 1.f : 0.f, t.sigma_x, t.sigma_y});
            }
            side.set("tracks", f32(tr));
            sides.call<void>("push", side);
        }
        out.set("sides", sides);
        return out;
    }

    val world() const {
        val out = val::object();
        out.set("roads", rects(pg_sim().roads()));
        out.set("buildings", rects(pg_sim().buildings()));
        out.set("occluders", rects(pg_sim().occluders()));
        return out;
    }

    bool setConfig(int side, const std::string& config) {
        if (!holdfast::TrackerConfig::by_name(config)) return false;
        pg_.set_config(side, config);
        return true;
    }
    void blackout(double s) { pg_.sim().blackout(s); }
    void freeze(double s) { pg_.sim().freeze(s); }
    void setDropout(double p) { pg_.sim().params().dropout = p; }
    void setJitterMs(double ms) { pg_.sim().params().jitter_ms = ms; }
    void setObjects(int n) { pg_.sim().params().target_objects = n; }
    void setFalsePositives(double rate) { pg_.sim().params().false_positives = rate; }
    void setCameraMode(int mode) { pg_.sim().params().camera = static_cast<CameraMode>(mode); }
    void setAutoBlackout(double every_s) { pg_.sim().params().auto_blackout_every_s = every_s; }
    void setAutoFreeze(double every_s) { pg_.sim().params().auto_freeze_every_s = every_s; }

private:
    const holdfast::Simulator& pg_sim() const { return const_cast<Playground&>(pg_).sim(); }
    Playground pg_;
};

EMSCRIPTEN_BINDINGS(holdfast) {
    emscripten::class_<PlaygroundJS>("Playground")
        .constructor<unsigned, std::string, std::string>()
        .function("step", &PlaygroundJS::step)
        .function("world", &PlaygroundJS::world)
        .function("setConfig", &PlaygroundJS::setConfig)
        .function("blackout", &PlaygroundJS::blackout)
        .function("freeze", &PlaygroundJS::freeze)
        .function("setDropout", &PlaygroundJS::setDropout)
        .function("setJitterMs", &PlaygroundJS::setJitterMs)
        .function("setObjects", &PlaygroundJS::setObjects)
        .function("setFalsePositives", &PlaygroundJS::setFalsePositives)
        .function("setCameraMode", &PlaygroundJS::setCameraMode)
        .function("setAutoBlackout", &PlaygroundJS::setAutoBlackout)
        .function("setAutoFreeze", &PlaygroundJS::setAutoFreeze);
}
