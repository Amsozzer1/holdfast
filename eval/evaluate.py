"""Score MOT-format tracker outputs against VisDrone ground truth with TrackEval.

Layout expected under a results root (one degradation preset):
    <root>/trackers/<tracker>/data/<seq>.txt
    <root>/events/<seq>.jsonl          (optional, Holdfast recovery events)

Usage:
    python eval/evaluate.py --results results/blackout --gt-dir data/VisDrone2019-MOT-val/annotations
"""
from __future__ import annotations

import argparse
import contextlib
import io
import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "third_party" / "TrackEval"))

# TrackEval predates NumPy 1.24/2.x and still uses a few removed aliases.
for alias, real in {"float": float, "int": int, "bool": bool}.items():
    if not hasattr(np, alias):
        setattr(np, alias, real)

import trackeval  # noqa: E402

EVAL_CATEGORIES = {1, 4, 5, 6, 9}  # pedestrian, car, van, truck, bus (class-agnostic)


def convert_gt(annotation: Path, out_dir: Path) -> int:
    """VisDrone annotation -> MOTChallenge gt.txt (frame,id,x,y,w,h,conf,class,vis). Returns seq length."""
    rows, last_frame = [], 0
    for line in annotation.read_text().splitlines():
        if not line.strip():
            continue
        f, tid, x, y, w, h, score, cat, _trunc, _occ = (int(float(v)) for v in line.split(",")[:10])
        last_frame = max(last_frame, f)
        if score == 0 or cat not in EVAL_CATEGORIES or w <= 0 or h <= 0:
            continue
        rows.append(f"{f},{tid},{x},{y},{w},{h},1,1,1")
    (out_dir / "gt").mkdir(parents=True, exist_ok=True)
    (out_dir / "gt" / "gt.txt").write_text("\n".join(rows) + "\n")
    return last_frame


def evaluate(results: Path, gt_dir: Path, sequences: list[str] | None = None,
             trackers: list[str] | None = None) -> dict:
    trackers_root = results / "trackers"
    trackers = trackers or sorted(p.name for p in trackers_root.iterdir() if (p / "data").is_dir())
    if sequences is None:
        sequences = sorted({p.stem for t in trackers for p in (trackers_root / t / "data").glob("*.txt")})

    gt_root = results / "_gt"
    seq_info = {s: convert_gt(gt_dir / f"{s}.txt", gt_root / s) for s in sequences}

    ds_cfg = trackeval.datasets.MotChallenge2DBox.get_default_dataset_config()
    ds_cfg.update({
        "GT_FOLDER": str(gt_root), "TRACKERS_FOLDER": str(trackers_root), "OUTPUT_FOLDER": str(results / "_trackeval"),
        "TRACKERS_TO_EVAL": trackers, "CLASSES_TO_EVAL": ["pedestrian"], "BENCHMARK": "VisDrone", "SPLIT_TO_EVAL": "val",
        "DO_PREPROC": False, "TRACKER_SUB_FOLDER": "data", "SEQ_INFO": seq_info,
        "GT_LOC_FORMAT": "{gt_folder}/{seq}/gt/gt.txt", "SKIP_SPLIT_FOL": True, "PRINT_CONFIG": False,
    })
    ev_cfg = trackeval.Evaluator.get_default_eval_config()
    ev_cfg.update({"USE_PARALLEL": False, "PRINT_RESULTS": False, "PRINT_CONFIG": False, "OUTPUT_SUMMARY": False,
                   "OUTPUT_DETAILED": False, "PLOT_CURVES": False, "TIME_PROGRESS": False, "DISPLAY_LESS_PROGRESS": True,
                   "OUTPUT_EMPTY_CLASSES": False})
    with contextlib.redirect_stdout(io.StringIO()):
        evaluator = trackeval.Evaluator(ev_cfg)
        dataset = trackeval.datasets.MotChallenge2DBox(ds_cfg)
        metrics = [trackeval.metrics.HOTA(), trackeval.metrics.CLEAR(), trackeval.metrics.Identity()]
        out, msgs = evaluator.evaluate([dataset], metrics)

    res = {}
    for tracker, per_seq in out["MotChallenge2DBox"].items():
        c = per_seq["COMBINED_SEQ"]["pedestrian"]
        res[tracker] = {
            "HOTA": 100 * float(np.mean(c["HOTA"]["HOTA"])),
            "AssA": 100 * float(np.mean(c["HOTA"]["AssA"])),
            "DetA": 100 * float(np.mean(c["HOTA"]["DetA"])),
            "IDF1": 100 * float(c["Identity"]["IDF1"]),
            "MOTA": 100 * float(c["CLEAR"]["MOTA"]),
            "IDSW": int(c["CLEAR"]["IDSW"]),
            "Frag": int(c["CLEAR"]["Frag"]),
        }
    # Recovery events (Holdfast trackers only).
    rec: dict[str, list[int]] = {}
    for seq in sequences:
        ev_file = results / "events" / f"{seq}.jsonl"
        if not ev_file.exists():
            continue
        for line in ev_file.read_text().splitlines():
            e = json.loads(line)
            if e.get("kind") != "recovered":
                continue
            r = rec.setdefault(e["tracker"], [0, 0])
            r[0] += 1
            r[1] += bool(e["correct"])
    for tracker, (n, ok) in rec.items():
        if tracker in res:
            res[tracker]["recoveries"] = n
            res[tracker]["recovery_precision"] = 100.0 * ok / n if n else 0.0
    return res


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", type=Path, required=True)
    ap.add_argument("--gt-dir", type=Path, required=True)
    ap.add_argument("--sequences", nargs="*")
    args = ap.parse_args()
    res = evaluate(args.results, args.gt_dir, args.sequences)
    cols = ["HOTA", "AssA", "DetA", "IDF1", "MOTA", "IDSW", "Frag"]
    print(f"{'tracker':<12}" + "".join(f"{c:>8}" for c in cols) + f"{'recov':>8}{'rec%':>8}")
    for t, m in res.items():
        vals = "".join(f"{m[c]:>8.2f}" if isinstance(m[c], float) else f"{m[c]:>8}" for c in cols)
        extra = f"{m.get('recoveries', '-'):>8}" + (f"{m['recovery_precision']:>8.1f}" if "recovery_precision" in m else f"{'-':>8}")
        print(f"{t:<12}{vals}{extra}")


if __name__ == "__main__":
    main()
