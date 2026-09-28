#!/usr/bin/env bash
# Downloads VisDrone2019-MOT val (~1.5 GB, academic-use licence) into data/VisDrone2019-MOT-val.
# Source 1: Hugging Face mirror (huseyincavus/visdrone2019-mot). Source 2: official Google Drive link.
# Manual fallback: https://github.com/VisDrone/VisDrone-Dataset (Task 4, val set).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PY="${PYTHON:-$ROOT/.venv/bin/python}"
mkdir -p "$ROOT/data" && cd "$ROOT/data"
[ -d VisDrone2019-MOT-val/sequences ] && { echo "already present"; exit 0; }

"$PY" -m pip -q install huggingface_hub==2.0.0 gdown==6.4.0
if "$PY" - <<'PY'
from huggingface_hub import snapshot_download
snapshot_download("huseyincavus/visdrone2019-mot", repo_type="dataset",
                  allow_patterns=["VisDrone2019-MOT-val/*"], local_dir="hf_tmp", max_workers=16)
PY
then
  mv hf_tmp/VisDrone2019-MOT-val/VisDrone2019-MOT-val ./VisDrone2019-MOT-val && rm -rf hf_tmp
else
  echo "HF mirror failed; trying Google Drive" >&2
  "$PY" -m gdown "1rqnKe9IgU_crMaxRoel9_nuUsMEBBVQu" -O VisDrone2019-MOT-val.zip
  unzip -q VisDrone2019-MOT-val.zip && rm VisDrone2019-MOT-val.zip
fi
ls VisDrone2019-MOT-val/sequences
