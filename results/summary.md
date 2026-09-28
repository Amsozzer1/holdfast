# Results

Generated 2026-09-28 by `eval/sweep.py`, seed 42, commit `bdccedb`.
VisDrone2019-MOT val, 5 classes (pedestrian, car, van, truck, bus), class-agnostic. Detections = ground truth + seeded noise (jitter, occlusion-dependent misses and scores, false positives).
HOTA/AssA/IDF1/MOTA/IDSW from TrackEval. *Recoveries* = confirmed tracks re-acquired after ≥ 2 missed frames; *correct* = the re-acquired detection is the same ground-truth object.

Dev sequences (used for every tuning decision): uav0000086_00000_v, uav0000117_02622_v, uav0000137_00458_v.  
Held-out sequences (not looked at until this table): uav0000182_00000_v, uav0000268_05773_v, uav0000305_00000_v, uav0000339_00001_v.

## All 7 sequences

| preset | tracker | HOTA | AssA | IDF1 | MOTA | IDSW | recoveries / correct |
|---|---|---:|---:|---:|---:|---:|---:|
| clean | baseline (ByteTrack-style) | 62.5 | 59.1 | 74.9 | 76.0 | 766 | 1182 / 87.7% |
|  | baseline + CMC | 68.5 | 67.5 | 83.0 | 79.6 | 278 | 1188 / 96.3% |
|  | **Holdfast (full)** | 69.0 | 68.2 | 83.5 | 79.9 | 255 | 1259 / 93.7% |
|  | OC-SORT (reference) | 52.3 | 49.0 | 68.8 | 65.7 | 671 | – |
| light | baseline (ByteTrack-style) | 49.7 | 44.3 | 62.0 | 63.7 | 908 | 2328 / 90.5% |
|  | baseline + CMC | 57.6 | 56.0 | 73.8 | 67.8 | 345 | 2520 / 97.2% |
|  | **Holdfast (full)** | 58.0 | 56.7 | 74.4 | 68.2 | 333 | 2618 / 95.8% |
|  | OC-SORT (reference) | 35.5 | 32.2 | 52.0 | 45.4 | 739 | – |
| heavy | baseline (ByteTrack-style) | 32.0 | 28.3 | 43.1 | 39.9 | 977 | 4589 / 93.4% |
|  | baseline + CMC | 36.9 | 34.9 | 51.3 | 43.6 | 459 | 5064 / 97.6% |
|  | **Holdfast (full)** | 38.9 | 38.2 | 55.7 | 44.8 | 361 | 5360 / 96.6% |
|  | OC-SORT (reference) | 14.5 | 12.8 | 23.9 | 17.9 | 757 | – |
| blackout | baseline (ByteTrack-style) | 42.9 | 35.2 | 52.7 | 59.6 | 791 | 1156 / 85.2% |
|  | baseline + CMC | 47.5 | 41.2 | 58.7 | 62.1 | 479 | 1160 / 91.5% |
|  | **Holdfast (full)** | 50.5 | 46.1 | 64.4 | 63.3 | 355 | 1392 / 88.4% |
|  | OC-SORT (reference) | 37.2 | 31.4 | 49.8 | 51.5 | 650 | – |
| freeze | baseline (ByteTrack-style) | 49.4 | 42.1 | 58.2 | 63.5 | 930 | 1014 / 87.1% |
|  | baseline + CMC | 55.2 | 50.2 | 66.2 | 66.6 | 533 | 1026 / 94.4% |
|  | **Holdfast (full)** | 59.5 | 57.2 | 74.2 | 69.0 | 338 | 1539 / 91.6% |
|  | OC-SORT (reference) | 41.2 | 34.2 | 51.2 | 54.2 | 883 | – |

## Held-out 4 sequences only

| preset | tracker | HOTA | AssA | IDF1 | MOTA | IDSW | recoveries / correct |
|---|---|---:|---:|---:|---:|---:|---:|
| clean | baseline (ByteTrack-style) | 59.8 | 57.9 | 72.4 | 70.7 | 248 | 561 / 88.2% |
|  | baseline + CMC | 65.9 | 66.7 | 80.5 | 74.2 | 86 | 570 / 97.4% |
|  | **Holdfast (full)** | 66.1 | 66.9 | 80.7 | 74.5 | 86 | 618 / 94.5% |
|  | OC-SORT (reference) | 50.5 | 48.5 | 67.6 | 61.9 | 216 | – |
| light | baseline (ByteTrack-style) | 47.9 | 44.3 | 60.4 | 59.3 | 292 | 1040 / 91.2% |
|  | baseline + CMC | 54.4 | 53.9 | 70.2 | 62.6 | 124 | 1089 / 97.7% |
|  | **Holdfast (full)** | 55.1 | 55.0 | 71.3 | 63.0 | 122 | 1149 / 96.2% |
|  | OC-SORT (reference) | 34.4 | 32.3 | 51.2 | 42.8 | 213 | – |
| heavy | baseline (ByteTrack-style) | 31.4 | 30.8 | 43.2 | 35.3 | 361 | 1914 / 93.4% |
|  | baseline + CMC | 35.9 | 36.9 | 50.8 | 39.1 | 146 | 2140 / 97.9% |
|  | **Holdfast (full)** | 36.9 | 38.0 | 53.0 | 40.6 | 111 | 2260 / 96.7% |
|  | OC-SORT (reference) | 13.8 | 12.9 | 23.0 | 16.0 | 284 | – |
| blackout | baseline (ByteTrack-style) | 44.0 | 38.9 | 54.5 | 56.5 | 282 | 537 / 87.0% |
|  | baseline + CMC | 49.4 | 46.7 | 62.1 | 59.2 | 162 | 537 / 94.4% |
|  | **Holdfast (full)** | 50.9 | 48.6 | 64.6 | 60.5 | 128 | 658 / 89.5% |
|  | OC-SORT (reference) | 38.2 | 34.4 | 52.3 | 49.6 | 235 | – |
| freeze | baseline (ByteTrack-style) | 50.2 | 46.0 | 60.0 | 60.2 | 323 | 483 / 87.4% |
|  | baseline + CMC | 55.4 | 53.3 | 67.4 | 63.2 | 180 | 495 / 95.6% |
|  | **Holdfast (full)** | 58.0 | 56.9 | 72.3 | 65.4 | 119 | 737 / 92.4% |
|  | OC-SORT (reference) | 42.4 | 37.9 | 54.2 | 52.1 | 300 | – |

## Ablations (all 7 sequences)

Each row removes one piece from the full tracker.

| preset | tracker | HOTA | AssA | IDF1 | MOTA | IDSW | recoveries / correct |
|---|---|---:|---:|---:|---:|---:|---:|
| clean | **Holdfast (full)** | 69.0 | 68.2 | 83.5 | 79.9 | 255 | 1259 / 93.7% |
|  | full − camera-motion comp. | 62.5 | 58.7 | 74.7 | 76.4 | 881 | 1393 / 79.4% |
|  | full − recovery stage | 68.7 | 67.9 | 83.2 | 79.7 | 273 | 1193 / 96.1% |
|  | full − OC re-update | 68.6 | 67.5 | 83.0 | 79.9 | 259 | 1261 / 93.7% |
|  | full − stale-frame guard | 69.0 | 68.2 | 83.5 | 79.9 | 255 | 1259 / 93.7% |
|  | full − timestamp dt (fixed dt) | 69.0 | 68.2 | 83.5 | 79.9 | 254 | 1258 / 93.8% |
| light | **Holdfast (full)** | 58.0 | 56.7 | 74.4 | 68.2 | 333 | 2618 / 95.8% |
|  | full − camera-motion comp. | 51.8 | 47.4 | 65.8 | 64.8 | 971 | 2653 / 85.8% |
|  | full − recovery stage | 57.8 | 56.4 | 74.1 | 67.8 | 349 | 2528 / 97.3% |
|  | full − OC re-update | 58.2 | 57.1 | 74.9 | 68.2 | 323 | 2619 / 95.7% |
|  | full − stale-frame guard | 58.0 | 56.7 | 74.4 | 68.2 | 333 | 2618 / 95.8% |
|  | full − timestamp dt (fixed dt) | 57.6 | 55.7 | 73.9 | 68.1 | 339 | 2620 / 95.6% |
| heavy | **Holdfast (full)** | 38.9 | 38.2 | 55.7 | 44.8 | 361 | 5360 / 96.6% |
|  | full − camera-motion comp. | 34.7 | 32.0 | 48.0 | 42.0 | 1026 | 5089 / 89.7% |
|  | full − recovery stage | 37.9 | 36.9 | 53.2 | 43.9 | 388 | 5075 / 98.0% |
|  | full − OC re-update | 39.1 | 38.5 | 56.0 | 45.0 | 358 | 5386 / 96.7% |
|  | full − stale-frame guard | 38.5 | 37.5 | 54.8 | 44.6 | 385 | 5367 / 96.6% |
|  | full − timestamp dt (fixed dt) | 37.4 | 35.4 | 52.7 | 44.5 | 448 | 5277 / 96.2% |
| blackout | **Holdfast (full)** | 50.5 | 46.1 | 64.4 | 63.3 | 355 | 1392 / 88.4% |
|  | full − camera-motion comp. | 46.9 | 41.3 | 59.7 | 60.7 | 771 | 1439 / 78.9% |
|  | full − recovery stage | 48.7 | 43.3 | 60.5 | 62.2 | 438 | 1152 / 93.6% |
|  | full − OC re-update | 50.3 | 45.7 | 64.0 | 63.3 | 354 | 1390 / 88.3% |
|  | full − stale-frame guard | 50.5 | 46.1 | 64.4 | 63.3 | 355 | 1392 / 88.4% |
|  | full − timestamp dt (fixed dt) | 47.8 | 41.4 | 59.2 | 62.5 | 489 | 1294 / 86.0% |
| freeze | **Holdfast (full)** | 59.5 | 57.2 | 74.2 | 69.0 | 338 | 1539 / 91.6% |
|  | full − camera-motion comp. | 55.3 | 50.6 | 67.8 | 67.9 | 866 | 1555 / 81.2% |
|  | full − recovery stage | 57.5 | 53.7 | 70.1 | 68.2 | 404 | 1359 / 95.0% |
|  | full − OC re-update | 59.6 | 57.2 | 74.2 | 69.1 | 338 | 1543 / 91.8% |
|  | full − stale-frame guard | 55.2 | 49.8 | 66.4 | 67.1 | 501 | 1174 / 87.6% |
|  | full − timestamp dt (fixed dt) | 59.5 | 57.2 | 74.2 | 69.0 | 337 | 1539 / 91.6% |
