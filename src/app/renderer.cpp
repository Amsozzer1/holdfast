#include "holdfast/app/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <vector>

#include <csignal>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace holdfast {

namespace {

cv::Scalar id_color(int id) {
    cv::Mat hsv(1, 1, CV_8UC3, cv::Scalar((id * 47) % 180, 210, 255)), bgr;
    cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
    const auto c = bgr.at<cv::Vec3b>(0, 0);
    return {static_cast<double>(c[0]), static_cast<double>(c[1]), static_cast<double>(c[2])};
}

void dashed_line(cv::Mat& img, cv::Point2f a, cv::Point2f b, const cv::Scalar& c, int thickness, float dash = 6, float gap = 5) {
    const cv::Point2f d = b - a;
    const float len = std::hypot(d.x, d.y);
    if (len < 1) return;
    const cv::Point2f u = d / len;
    for (float s = 0; s < len; s += dash + gap) {
        const float e = std::min(len, s + dash);
        cv::line(img, a + u * s, a + u * e, c, thickness, cv::LINE_AA);
    }
}

void dashed_rect(cv::Mat& img, const cv::Rect2f& r, const cv::Scalar& c, int thickness) {
    const cv::Point2f tl = r.tl(), br = r.br(), tr(br.x, tl.y), bl(tl.x, br.y);
    dashed_line(img, tl, tr, c, thickness);
    dashed_line(img, tr, br, c, thickness);
    dashed_line(img, br, bl, c, thickness);
    dashed_line(img, bl, tl, c, thickness);
}

void dashed_ellipse(cv::Mat& img, cv::Point2f center, cv::Size2f axes, const cv::Scalar& c) {
    std::vector<cv::Point> pts;
    cv::ellipse2Poly(cv::Point(cvRound(center.x), cvRound(center.y)),
                     cv::Size(std::max(1, cvRound(axes.width)), std::max(1, cvRound(axes.height))), 0, 0, 360, 8, pts);
    for (size_t i = 0; i + 1 < pts.size(); i += 2) cv::line(img, pts[i], pts[i + 1], c, 1, cv::LINE_AA);
}

void label(cv::Mat& img, const std::string& text, cv::Point org, double scale, const cv::Scalar& fg, const cv::Scalar& bg) {
    int base = 0;
    const auto sz = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, scale, 1, &base);
    cv::rectangle(img, cv::Rect(org.x, org.y - sz.height - 4, sz.width + 6, sz.height + base + 4), bg, cv::FILLED);
    cv::putText(img, text, cv::Point(org.x + 3, org.y - 2), cv::FONT_HERSHEY_SIMPLEX, scale, fg, 1, cv::LINE_AA);
}

}  // namespace

Renderer::Renderer(const std::string& path, cv::Size source_size, int panels, double fps, int panel_width)
    : src_(source_size), n_(panels), trails_(panels) {
    scale_ = static_cast<double>(panel_width) / source_size.width;
    panel_ = cv::Size(panel_width, static_cast<int>(std::lround(source_size.height * scale_ / 2.0)) * 2);
    // Spawn ffmpeg directly with an argv array (no shell), so the output path is never
    // interpreted as shell syntax.
    const std::vector<std::string> args = {
        "ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "bgr24",
        "-s", std::format("{}x{}", panel_.width * n_, panel_.height), "-r", std::format("{}", fps), "-i", "-",
        "-c:v", "libx264", "-preset", "veryfast", "-crf", "22", "-pix_fmt", "yuv420p", "-movflags", "+faststart",
        "--", path};
    std::vector<char*> argv;
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    std::signal(SIGPIPE, SIG_IGN);  // a dead ffmpeg becomes a write error, not a silent exit
    int fds[2];
    if (pipe(fds) != 0) throw std::runtime_error("pipe() failed");
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, fds[0], STDIN_FILENO);
    posix_spawn_file_actions_addclose(&fa, fds[1]);
    pid_t pid = -1;
    const int rc = posix_spawnp(&pid, "ffmpeg", &fa, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&fa);
    close(fds[0]);
    if (rc != 0) {
        close(fds[1]);
        throw std::runtime_error("cannot start ffmpeg (is it installed?)");
    }
    child_ = pid;
    pipe_ = fdopen(fds[1], "w");
    if (!pipe_) throw std::runtime_error("fdopen() failed");
}

Renderer::~Renderer() {
    if (pipe_) std::fclose(pipe_);  // EOF on ffmpeg's stdin finalizes the file
    if (child_ > 0) {
        int status = 0;
        waitpid(child_, &status, 0);
    }
}

void Renderer::draw_panel(cv::Mat& canvas, const Panel& p, std::map<int, std::deque<cv::Point2f>>& trails, Banner banner) {
    const float s = static_cast<float>(scale_);
    // Trails first, under the boxes.
    for (const auto& [id, pts] : trails) {
        const auto c = id_color(id);
        for (size_t i = 1; i < pts.size(); ++i) cv::line(canvas, pts[i - 1] * s, pts[i] * s, c, 1, cv::LINE_AA);
    }
    for (const auto& t : p.tracks) {
        const cv::Rect2f r(t.box.x * s, t.box.y * s, t.box.w * s, t.box.h * s);
        const auto c = id_color(t.id);
        const bool solid = t.matched_this_frame && t.state == TrackState::Confirmed;
        if (solid) {
            cv::rectangle(canvas, r, c, 2, cv::LINE_AA);
        } else if (t.state == TrackState::Confirmed || t.state == TrackState::Lost) {
            // Coasting: dashed box plus the 2-sigma position uncertainty, which grows with time unseen.
            dashed_rect(canvas, r, c, 1);
            dashed_ellipse(canvas, cv::Point2f(t.box.cx() * s, t.box.cy() * s), cv::Size2f(t.sigma_x * s, t.sigma_y * s), c);
        } else {
            continue;  // tentative: not drawn
        }
        label(canvas, std::to_string(t.id), cv::Point(cvRound(r.x), cvRound(r.y)), 0.4, cv::Scalar(20, 20, 20), c);
    }
    // Header.
    cv::Mat roi = canvas(cv::Rect(0, 0, canvas.cols, 34));
    roi *= 0.35;
    cv::putText(canvas, p.title, cv::Point(10, 23), cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    const std::string stats = p.has_ground_truth
        ? std::format("ID switches {}   recovered {}/{}", p.id_switches, p.recoveries_correct, p.recoveries)
        : std::format("tracks {}   re-acquired {}", std::count_if(p.tracks.begin(), p.tracks.end(), [](const TrackOutput& t) {
              return t.state == TrackState::Confirmed; }), p.recoveries);
    int base = 0;
    const auto sz = cv::getTextSize(stats, cv::FONT_HERSHEY_SIMPLEX, 0.55, 1, &base);
    cv::putText(canvas, stats, cv::Point(canvas.cols - sz.width - 10, 23), cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    if (banner != Banner::None) {
        const bool bo = banner == Banner::Blackout;
        const std::string msg = bo ? "NO VIDEO  -  frames lost" : "FROZEN FEED  -  stale frame ignored";
        const cv::Scalar bg = bo ? cv::Scalar(40, 40, 200) : cv::Scalar(0, 150, 230);
        const auto msz = cv::getTextSize(msg, cv::FONT_HERSHEY_SIMPLEX, 0.7, 2, &base);
        const cv::Point org((canvas.cols - msz.width) / 2, canvas.rows / 2);
        cv::rectangle(canvas, cv::Rect(org.x - 12, org.y - msz.height - 12, msz.width + 24, msz.height + 24), bg, cv::FILLED);
        cv::putText(canvas, msg, org, cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2, cv::LINE_AA);
    }
}

void Renderer::draw(const cv::Mat& image, const std::vector<Panel>& panels, Banner banner, const Affine2& A,
                    int frame, double t, const std::string& footer) {
    cv::Mat base;
    if (image.empty()) {
        base = cv::Mat(panel_, CV_8UC3, cv::Scalar(18, 18, 18));
    } else {
        cv::resize(image, base, panel_, 0, 0, cv::INTER_AREA);
    }
    cv::Mat out(panel_.height, panel_.width * n_, CV_8UC3);
    for (int i = 0; i < n_ && i < static_cast<int>(panels.size()); ++i) {
        // Update trails: warp old points by the camera motion, append matched centers.
        auto& trails = trails_[i];
        if (!A.is_identity()) {
            for (auto& [id, pts] : trails) {
                for (auto& q : pts) {
                    q = cv::Point2f(static_cast<float>(A.m[0] * q.x + A.m[1] * q.y + A.m[2]),
                                    static_cast<float>(A.m[3] * q.x + A.m[4] * q.y + A.m[5]));
                }
            }
        }
        std::map<int, std::deque<cv::Point2f>> next;
        for (const auto& tr : panels[i].tracks) {
            if (tr.state != TrackState::Confirmed && tr.state != TrackState::Lost) continue;
            auto pts = trails.count(tr.id) ? trails[tr.id] : std::deque<cv::Point2f>{};
            if (tr.matched_this_frame) pts.emplace_back(tr.box.cx(), tr.box.cy() + 0.5f * tr.box.h);
            while (pts.size() > 15) pts.pop_front();
            next[tr.id] = std::move(pts);
        }
        trails = std::move(next);

        cv::Mat panel = out(cv::Rect(i * panel_.width, 0, panel_.width, panel_.height));
        base.copyTo(panel);
        draw_panel(panel, panels[i], trails, banner);
        if (i > 0) cv::line(out, cv::Point(i * panel_.width, 0), cv::Point(i * panel_.width, panel_.height), cv::Scalar(255, 255, 255), 2);
    }
    const std::string foot = std::format("frame {:4d}   t {:6.2f}s   {}", frame, t, footer);
    label(out, foot, cv::Point(8, out.rows - 8), 0.5, cv::Scalar(255, 255, 255), cv::Scalar(30, 30, 30));
    const size_t n = out.total() * out.elemSize();
    if (std::fwrite(out.data, 1, n, pipe_) != n) throw std::runtime_error("ffmpeg stopped accepting frames (see its error above)");
}

}  // namespace holdfast
