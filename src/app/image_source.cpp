#include "holdfast/app/image_source.hpp"

#include <algorithm>
#include <filesystem>
#include <opencv2/imgcodecs.hpp>
#include <stdexcept>

namespace fs = std::filesystem;

namespace holdfast {

ImageDirSource::ImageDirSource(const std::string& dir) {
    if (!fs::is_directory(dir)) throw std::runtime_error("not a directory: " + dir);
    for (const auto& e : fs::directory_iterator(dir)) {
        const auto ext = e.path().extension().string();
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png") files_.push_back(e.path().string());
    }
    if (files_.empty()) throw std::runtime_error("no images in " + dir);
    std::sort(files_.begin(), files_.end());
    name_ = fs::path(dir).filename().string();
    if (name_.empty()) name_ = fs::path(dir).parent_path().filename().string();
    size_ = load(1).size();
}

cv::Mat ImageDirSource::load(int frame) const {
    if (frame < 1 || frame > size()) throw std::out_of_range("frame out of range");
    cv::Mat img = cv::imread(files_[frame - 1], cv::IMREAD_COLOR);
    if (img.empty()) throw std::runtime_error("cannot read " + files_[frame - 1]);
    return img;
}

}  // namespace holdfast
