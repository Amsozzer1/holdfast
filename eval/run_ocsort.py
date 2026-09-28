"""Run the reference OC-SORT tracker on the exact detection stream Holdfast saw.

Reads  <results>/dets/<seq>.jsonl  (written by holdfast_run --out-dir)
Writes <results>/trackers/ocsort/data/<seq>.txt

OC-SORT is a frame-index tracker. Frames lost in a blackout are fed to it as empty
updates, i.e. it is *told* how many frames were lost. That is more than a real receiver
would know, so this comparison is deliberately generous to the reference.
Hyperparameters are OC-SORT's published MOT17 defaults, except det_thresh=0.4 and
use_byte=True, which were the best of {0.4, 0.5, 0.6} x {byte on, off} on the dev sequences
(the default 0.6 / off throws away most occluded detections from this detector).
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "third_party" / "OC_SORT" / "trackers"))
from ocsort_tracker.ocsort import OCSort  # noqa: E402


def run_sequence(dets_file: Path, out_file: Path, det_thresh: float = 0.4, use_byte: bool = True) -> None:
    tracker = OCSort(det_thresh=det_thresh, max_age=30, min_hits=3, iou_threshold=0.3, delta_t=3,
                     asso_func="iou", inertia=0.2, use_byte=use_byte)
    frames = [json.loads(line) for line in dets_file.read_text().splitlines()]
    lines = []
    prev = frames[0]["frame"] - 1 if frames else 0
    for fr in frames:
        for _ in range(fr["frame"] - prev - 1):  # blacked-out frames: tell OC-SORT they happened
            tracker.update(np.empty((0, 5)), (1, 1), (1, 1))
        prev = fr["frame"]
        d = np.array(fr["dets"], dtype=np.float64).reshape(-1, 5)
        d[:, 2] += d[:, 0]  # xywh -> x1y1x2y2
        d[:, 3] += d[:, 1]
        for x1, y1, x2, y2, tid in tracker.update(d, (1, 1), (1, 1)):
            lines.append(f"{fr['frame']},{int(tid)},{x1:.2f},{y1:.2f},{x2 - x1:.2f},{y2 - y1:.2f},1,-1,-1,-1")
    out_file.parent.mkdir(parents=True, exist_ok=True)
    out_file.write_text("\n".join(lines) + ("\n" if lines else ""))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", type=Path, required=True, help="results root for one preset")
    args = ap.parse_args()
    for dets in sorted((args.results / "dets").glob("*.jsonl")):
        run_sequence(dets, args.results / "trackers" / "ocsort" / "data" / f"{dets.stem}.txt")


if __name__ == "__main__":
    main()
