#!/usr/bin/env bash
# One command: side-by-side baseline vs Holdfast on a blacked-out, moving-camera sequence.
#   ./scripts/demo.sh [sequence] [preset]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SEQ="${1:-uav0000137_00458_v}"
PRESET="${2:-blackout}"
DATA="$ROOT/data/VisDrone2019-MOT-val"
BIN="$ROOT/build/release/holdfast_run"
[ -x "$BIN" ] || { echo "build first: cmake --preset release && cmake --build --preset release"; exit 1; }
mkdir -p "$ROOT/results/demo"
OUT="$ROOT/results/demo/${SEQ}_${PRESET}.mp4"
"$BIN" --source "$DATA/sequences/$SEQ" --gt "$DATA/annotations/$SEQ.txt" \
       --configs baseline,full --degrade "$PRESET" --seed 42 --render "$OUT"
echo "wrote $OUT"
command -v open >/dev/null && open "$OUT" || true
