#!/usr/bin/env bash
# Downloads a prebuilt ONNX Runtime (CPU) for this OS/arch into .deps/onnxruntime.
set -euo pipefail
VER="${ORT_VERSION:-1.22.0}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${1:-$ROOT/.deps/onnxruntime}"
case "$(uname -s)-$(uname -m)" in
  Darwin-arm64)            PKG="onnxruntime-osx-arm64-${VER}" ;;
  Darwin-x86_64)           PKG="onnxruntime-osx-x86_64-${VER}" ;;
  Linux-x86_64)            PKG="onnxruntime-linux-x64-${VER}" ;;
  Linux-aarch64|Linux-arm64) PKG="onnxruntime-linux-aarch64-${VER}" ;;
  *) echo "unsupported platform $(uname -s)-$(uname -m)"; exit 1 ;;
esac
if [ -f "$DEST/include/onnxruntime_cxx_api.h" ]; then echo "ORT already at $DEST"; exit 0; fi
mkdir -p "$DEST"
TMP="$(mktemp)"
curl -fsSL --retry 6 --retry-all-errors --retry-delay 5 -o "$TMP" "https://github.com/microsoft/onnxruntime/releases/download/v${VER}/${PKG}.tgz"
tar xzf "$TMP" -C "$DEST" --strip-components=1
rm -f "$TMP"
echo "ONNX Runtime ${VER} installed at $DEST"
