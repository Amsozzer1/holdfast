#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace holdfast {

// A directory of frame images (VisDrone ships sequences as numbered JPGs).
// Frames are 1-based to match MOT annotations.
class ImageDirSource {
public:
    explicit ImageDirSource(const std::string& dir);
    [[nodiscard]] int size() const { return static_cast<int>(files_.size()); }
    [[nodiscard]] cv::Mat load(int frame) const;  // BGR
    [[nodiscard]] cv::Size frame_size() const { return size_; }
    [[nodiscard]] std::string name() const { return name_; }

private:
    std::vector<std::string> files_;
    cv::Size size_;
    std::string name_;
};

}  // namespace holdfast
