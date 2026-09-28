#!/usr/bin/env bash
# Builds a minimal OpenCV (core, imgproc, imgcodecs, video, calib3d) into .deps/opencv.
# ~100 MB installed, versus several GB for a full distro/Homebrew OpenCV.
set -euo pipefail
VER="${OPENCV_VERSION:-4.10.0}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${1:-$ROOT/.deps/opencv}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

if [ -f "$PREFIX/lib/cmake/opencv4/OpenCVConfig.cmake" ]; then
  echo "OpenCV already installed at $PREFIX"; exit 0
fi

curl -fsSL --retry 6 --retry-all-errors --retry-delay 5 -o "$WORK/src.tar.gz" "https://github.com/opencv/opencv/archive/refs/tags/${VER}.tar.gz"
tar xzf "$WORK/src.tar.gz" -C "$WORK"
cmake -S "$WORK/opencv-${VER}" -B "$WORK/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DBUILD_LIST=core,imgproc,imgcodecs,video,calib3d \
  -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF \
  -DBUILD_opencv_apps=OFF -DBUILD_opencv_python3=OFF -DBUILD_JAVA=OFF \
  -DBUILD_JPEG=ON -DBUILD_PNG=ON -DBUILD_ZLIB=ON \
  -DWITH_TIFF=OFF -DWITH_WEBP=OFF -DWITH_OPENEXR=OFF -DWITH_JASPER=OFF -DWITH_OPENJPEG=OFF \
  -DWITH_FFMPEG=OFF -DWITH_GSTREAMER=OFF -DWITH_OPENCL=OFF -DWITH_IPP=OFF -DWITH_ITT=OFF \
  -DWITH_PROTOBUF=OFF -DWITH_EIGEN=OFF -DWITH_LAPACK=OFF -DWITH_ADE=OFF -DWITH_AVIF=OFF \
  -DWITH_IMGCODEC_HDR=OFF -DWITH_IMGCODEC_SUNRASTER=OFF -DWITH_IMGCODEC_PXM=OFF -DWITH_IMGCODEC_PFM=OFF \
  -DOPENCV_GENERATE_PKGCONFIG=OFF
cmake --build "$WORK/build" -j"$(getconf _NPROCESSORS_ONLN)"
cmake --install "$WORK/build"
echo "OpenCV installed at $PREFIX"
