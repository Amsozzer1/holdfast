#!/usr/bin/env bash
# Builds the tracker core + simulator to WebAssembly for the browser playground and copies the
# module to web/src/wasm/holdfast.js. Uses a local Emscripten if present, otherwise the pinned
# official Emscripten container.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="emscripten/emsdk:4.0.10@sha256:90b757eb11fa9a0e3ce4d2d9f76d932a56018e4accc37b5a28b2783751e60eb7"
BUILD='emcmake cmake -S wasm -B build/wasm -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build/wasm -j'
cd "$ROOT"
if command -v emcmake >/dev/null; then
  bash -c "$BUILD"
else
  docker run --rm --platform linux/amd64 -v "$ROOT":/src -w /src "$IMAGE" \
    bash -c "$BUILD && chown -R $(id -u):$(id -g) build/wasm"
fi
mkdir -p web/src/wasm
cp build/wasm/holdfast.js web/src/wasm/holdfast.js
ls -lh web/src/wasm/holdfast.js
