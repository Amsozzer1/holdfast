"""Reproduce the results table: every degradation preset x every tracker config, plus the
OC-SORT reference, scored with TrackEval and written to results/summary.md.

    python eval/sweep.py --data data/VisDrone2019-MOT-val --bin build/release/holdfast_run

Deterministic: same seed => same table.
"""
from __future__ import annotations

import argparse
import datetime as dt
import json
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from evaluate import evaluate  # noqa: E402
from run_ocsort import run_sequence as run_ocsort  # noqa: E402

# Tuning decisions were made on DEV only; HELDOUT was not looked at until the final table.
DEV = ["uav0000086_00000_v", "uav0000117_02622_v", "uav0000137_00458_v"]
HELDOUT = ["uav0000182_00000_v", "uav0000268_05773_v", "uav0000305_00000_v", "uav0000339_00001_v"]

MAIN = ["baseline", "cmc", "full"]
ABLATIONS = ["full+cmc=0", "full+recovery=0", "full+oc=0", "full+stale=0", "full+vdt=0"]
PRESETS = ["clean", "light", "heavy", "blackout", "freeze"]

LABELS = {
    "baseline": "baseline (ByteTrack-style)",
    "cmc": "baseline + CMC",
    "full": "**Holdfast (full)**",
    "ocsort": "OC-SORT (reference)",
    "full+cmc=0": "full − camera-motion comp.",
    "full+recovery=0": "full − recovery stage",
    "full+oc=0": "full − OC re-update",
    "full+stale=0": "full − stale-frame guard",
    "full+vdt=0": "full − timestamp dt (fixed dt)",
}


def run_preset(args, preset: str, sequences: list[str]) -> Path:
    out = args.out / preset
    shutil.rmtree(out, ignore_errors=True)
    configs = ",".join(MAIN + ABLATIONS)

    def one(seq: str) -> None:
        cmd = [str(args.bin), "--source", str(args.data / "sequences" / seq), "--gt", str(args.data / "annotations" / f"{seq}.txt"),
               "--configs", configs, "--degrade", preset, "--seed", str(args.seed), "--out-dir", str(out), "--quiet"]
        subprocess.run(cmd, check=True)

    with ThreadPoolExecutor(args.jobs) as ex:
        list(ex.map(one, sequences))
    # OC-SORT keeps its track-id counter in a class variable, so it must not run in parallel
    # threads within one process (they would share and reset each other's ids).
    for seq in sequences:
        run_ocsort(out / "dets" / f"{seq}.jsonl", out / "trackers" / "ocsort" / "data" / f"{seq}.txt")
    return out


def fmt_row(label: str, m: dict) -> str:
    rec = f"{m['recoveries']} / {m['recovery_precision']:.1f}%" if "recoveries" in m else "–"
    return (f"| {label} | {m['HOTA']:.1f} | {m['AssA']:.1f} | {m['IDF1']:.1f} | {m['MOTA']:.1f} | {m['IDSW']} | {rec} |")


def table(results: dict, split: str, trackers: list[str]) -> str:
    lines = ["| preset | tracker | HOTA | AssA | IDF1 | MOTA | IDSW | recoveries / correct |",
             "|---|---|---:|---:|---:|---:|---:|---:|"]
    for preset in results:
        for i, t in enumerate(trackers):
            row = fmt_row(LABELS.get(t, t), results[preset][split][t])
            lines.append(f"| {preset if i == 0 else ''} " + row)
    return "\n".join(lines)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", type=Path, default=Path("data/VisDrone2019-MOT-val"))
    ap.add_argument("--bin", type=Path, default=Path("build/release/holdfast_run"))
    ap.add_argument("--out", type=Path, default=Path("results"))
    ap.add_argument("--presets", nargs="*", default=PRESETS)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()

    sequences = DEV + HELDOUT
    results: dict = {}
    for preset in args.presets:
        print(f"[{preset}] running trackers...", flush=True)
        root = run_preset(args, preset, sequences)
        print(f"[{preset}] scoring...", flush=True)
        results[preset] = {
            "all": evaluate(root, args.data / "annotations", sequences),
            "heldout": evaluate(root, args.data / "annotations", HELDOUT),
            "dev": evaluate(root, args.data / "annotations", DEV),
        }

    (args.out / "summary.json").write_text(json.dumps(results, indent=2))
    try:
        commit = subprocess.run(["git", "rev-parse", "--short", "HEAD"], capture_output=True, text=True).stdout.strip()
    except OSError:
        commit = "?"
    md = [
        "# Results",
        "",
        f"Generated {dt.date.today()} by `eval/sweep.py`, seed {args.seed}, commit `{commit or 'uncommitted'}`.",
        "VisDrone2019-MOT val, 5 classes (pedestrian, car, van, truck, bus), class-agnostic. "
        "Detections = ground truth + seeded noise (jitter, occlusion-dependent misses and scores, false positives).",
        "HOTA/AssA/IDF1/MOTA/IDSW from TrackEval. *Recoveries* = confirmed tracks re-acquired after ≥ 2 missed frames; "
        "*correct* = the re-acquired detection is the same ground-truth object.",
        "",
        f"Dev sequences (used for every tuning decision): {', '.join(DEV)}.  ",
        f"Held-out sequences (not looked at until this table): {', '.join(HELDOUT)}.",
        "",
        "## All 7 sequences",
        "",
        table(results, "all", MAIN + ["ocsort"]),
        "",
        "## Held-out 4 sequences only",
        "",
        table(results, "heldout", MAIN + ["ocsort"]),
        "",
        "## Ablations (all 7 sequences)",
        "",
        "Each row removes one piece from the full tracker.",
        "",
        table(results, "all", ["full"] + ABLATIONS),
        "",
    ]
    (args.out / "summary.md").write_text("\n".join(md))
    print(f"wrote {args.out / 'summary.md'}")


if __name__ == "__main__":
    main()
