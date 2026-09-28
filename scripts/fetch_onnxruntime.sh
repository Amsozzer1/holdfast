#!/usr/bin/env bash
# Downloads a prebuilt ONNX Runtime (CPU) for this OS/arch into .deps/onnxruntime.
set -euo pipefail
VER="1.22.0"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$(dirname "$0")/_verify.sh"
DEST="${1:-$ROOT/.deps/onnxruntime}"
case "$(uname -s)-$(uname -m)" in
  Darwin-arm64)  PKG="onnxruntime-osx-arm64-${VER}";    SHA256="cab6dcbd77e7ec775390e7b73a8939d45fec3379b017c7cb74f5b204c1a1cc07" ;;
  Darwin-x86_64) PKG="onnxruntime-osx-x86_64-${VER}";   SHA256="e4ec94a7696de74fb1b12846569aa94e499958af6ffa186022cfde16c9d617f0" ;;
  Linux-x86_64)  PKG="onnxruntime-linux-x64-${VER}";    SHA256="8344d55f93d5bc5021ce342db50f62079daf39aaafb5d311a451846228be49b3" ;;
  Linux-aarch64|Linux-arm64)
                 PKG="onnxruntime-linux-aarch64-${VER}"; SHA256="bb76395092d150b52c7092dc6b8f2fe4d80f0f3bf0416d2f269193e347e24702" ;;
  *) echo "unsupported platform $(uname -s)-$(uname -m)"; exit 1 ;;
esac
if [ -f "$DEST/include/onnxruntime_cxx_api.h" ]; then echo "ORT already at $DEST"; exit 0; fi
mkdir -p "$DEST"
TMP="$(mktemp)"
curl -fsSL --retry 6 --retry-all-errors --retry-delay 5 -o "$TMP" "https://github.com/microsoft/onnxruntime/releases/download/v${VER}/${PKG}.tgz"
verify_sha256 "$TMP" "$SHA256"
tar xzf "$TMP" -C "$DEST" --strip-components=1
rm -f "$TMP"
echo "ONNX Runtime ${VER} installed at $DEST"
