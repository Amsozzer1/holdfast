#!/usr/bin/env bash
# Evaluation dependencies: TrackEval (HOTA/IDF1/MOTA) and OC-SORT (reference tracker, MIT).
# Cloned into eval/third_party (gitignored); Python packages into .venv.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TP="$ROOT/eval/third_party"
mkdir -p "$TP"
[ -d "$TP/TrackEval" ] || git clone -q --depth 1 https://github.com/JonathonLuiten/TrackEval.git "$TP/TrackEval"
[ -d "$TP/OC_SORT" ]   || git clone -q --depth 1 https://github.com/noahcao/OC_SORT.git "$TP/OC_SORT"
[ -d "$ROOT/.venv" ] || python3 -m venv "$ROOT/.venv"
"$ROOT/.venv/bin/pip" -q install -r "$ROOT/eval/requirements.txt"
echo "TrackEval $(git -C "$TP/TrackEval" rev-parse --short HEAD), OC_SORT $(git -C "$TP/OC_SORT" rev-parse --short HEAD)"
