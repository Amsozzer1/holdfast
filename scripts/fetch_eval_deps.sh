#!/usr/bin/env bash
# Evaluation dependencies: TrackEval (HOTA/IDF1/MOTA) and OC-SORT (reference tracker, MIT).
# Cloned into eval/third_party (gitignored); Python packages into .venv.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TP="$ROOT/eval/third_party"
mkdir -p "$TP"
# Pinned to exact commits.
clone_at() {  # clone_at URL DIR COMMIT
  [ -d "$2" ] && return 0
  git init -q "$2" && git -C "$2" remote add origin "$1"
  git -C "$2" fetch -q --depth 1 origin "$3" && git -C "$2" checkout -q FETCH_HEAD
}
clone_at https://github.com/JonathonLuiten/TrackEval.git "$TP/TrackEval" 12c8791b303e0a0b50f753af204249e622d0281a
clone_at https://github.com/noahcao/OC_SORT.git          "$TP/OC_SORT"   8462e7e729a93ccd3bd995c0a79a890336cb3a0b
[ -d "$ROOT/.venv" ] || python3 -m venv "$ROOT/.venv"
"$ROOT/.venv/bin/pip" -q install -r "$ROOT/eval/requirements.txt"
echo "TrackEval $(git -C "$TP/TrackEval" rev-parse --short HEAD), OC_SORT $(git -C "$TP/OC_SORT" rev-parse --short HEAD)"
