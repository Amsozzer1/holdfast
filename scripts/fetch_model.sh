#!/usr/bin/env bash
# Downloads YOLOX-Nano (Apache-2.0) ONNX weights into models/.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$(dirname "$0")/_verify.sh"
SHA256="c789161ed43c8269fcd4e67c67eeeb4e80c622da2eb296a20bc6007bd18a0b7d"
mkdir -p "$ROOT/models"
OUT="$ROOT/models/yolox_nano.onnx"
[ -f "$OUT" ] && { echo "model already at $OUT"; exit 0; }
curl -fL --retry 6 --retry-all-errors --retry-delay 5 -o "$OUT" "https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_nano.onnx"
verify_sha256 "$OUT" "$SHA256"
echo "saved $OUT (sha256 ok)"
