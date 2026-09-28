#!/usr/bin/env bash
# Downloads YOLOX-Nano (Apache-2.0) ONNX weights into models/.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/models"
OUT="$ROOT/models/yolox_nano.onnx"
[ -f "$OUT" ] && { echo "model already at $OUT"; exit 0; }
curl -fL --retry 6 --retry-all-errors --retry-delay 5 -o "$OUT" "https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_nano.onnx"
echo "saved $OUT"
