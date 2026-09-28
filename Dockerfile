# Multi-stage build. Works natively on linux/amd64 (x86 edge box) and linux/arm64
# (Jetson-class CPUs, or Apple Silicon running Docker).
#
#   docker build -t holdfast .
#   docker build --target asan .          # core tests under ASan + UBSan
#   docker run --rm --cpus=1 -v $PWD/data:/data -v $PWD/models:/models holdfast \
#       --source /data/VisDrone2019-MOT-val/sequences/<seq> --model /models/yolox_nano.onnx

# Base image pinned by digest (multi-arch index) so builds can't silently change.
FROM ubuntu:26.04@sha256:da6fc2be547864451aa253836dd926da33623312df4a9a243e35dc877c378a78 AS deps
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build curl ca-certificates libeigen3-dev libgtest-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY scripts/_verify.sh scripts/build_opencv_min.sh scripts/fetch_onnxruntime.sh scripts/
RUN ./scripts/build_opencv_min.sh /opt/opencv && ./scripts/fetch_onnxruntime.sh /opt/onnxruntime

FROM deps AS build
COPY CMakeLists.txt CMakePresets.json ./
COPY include include
COPY src src
COPY apps apps
COPY tests tests
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DOpenCV_DIR=/opt/opencv/lib/cmake/opencv4 -DHOLDFAST_ORT_DIR=/opt/onnxruntime \
    && cmake --build build \
    && ctest --test-dir build --output-on-failure

FROM deps AS asan
COPY CMakeLists.txt CMakePresets.json ./
COPY include include
COPY src src
COPY tests tests
RUN cmake --preset asan && cmake --build --preset asan && ctest --preset asan

# Onboard image: tracker binaries + ONNX Runtime only. (Rendering MP4s needs the ffmpeg CLI;
# do that on a workstation, not on the vehicle.)
FROM ubuntu:26.04@sha256:da6fc2be547864451aa253836dd926da33623312df4a9a243e35dc877c378a78 AS runtime
COPY --from=build /opt/onnxruntime/lib/libonnxruntime.so* /usr/local/lib/
COPY --from=build /src/build/holdfast_run /src/build/holdfast_bench /usr/local/bin/
RUN ldconfig && useradd --create-home --uid 10001 holdfast && mkdir -p /work && chown holdfast /work
USER holdfast
WORKDIR /work
ENTRYPOINT ["holdfast_bench"]
