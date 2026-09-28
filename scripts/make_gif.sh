#!/usr/bin/env bash
# Cut a README GIF from a rendered demo:  ./scripts/make_gif.sh in.mp4 out.gif START_S DURATION_S
set -euo pipefail
IN="$1"; OUT="$2"; START="${3:-2.3}"; DUR="${4:-2.5}"
PAL="$(mktemp -t palette).png"
F="fps=12,scale=1200:-1:flags=lanczos"
ffmpeg -v error -y -ss "$START" -t "$DUR" -i "$IN" -vf "$F,palettegen=max_colors=128" "$PAL"
ffmpeg -v error -y -ss "$START" -t "$DUR" -i "$IN" -i "$PAL" -lavfi "$F [x]; [x][1:v] paletteuse=dither=bayer:bayer_scale=4" "$OUT"
rm -f "$PAL"
ls -lh "$OUT"
