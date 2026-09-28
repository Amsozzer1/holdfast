# Sequences

VisDrone2019-MOT **val** (7 sequences). Frame rate is not given with the image sequences;
timestamps are synthesized at 30 fps (`--fps`) and then perturbed by the degradation module.

| sequence | frames | resolution | evaluated objects / frame (median) | camera motion (p50 / p95 / max px per frame) | split |
|---|---:|---|---:|---|---|
| uav0000086_00000_v | 464 | 1344×756 | 32 | 1.9 / 2.8 / 3.4 (slow steady drift) | dev |
| uav0000117_02622_v | 349 | 2720×1530 | 32 | 3.1 / 21 / 71 | dev |
| uav0000137_00458_v | 233 | 2688×1512 | 48 | 2.3 / 50 / 85 | dev |
| uav0000182_00000_v | 363 | 1344×756 | 21 | | held-out |
| uav0000268_05773_v | 978 | 3840×2160 | 12 | | held-out |
| uav0000305_00000_v | 184 | 1904×1071 | 28 | | held-out |
| uav0000339_00001_v | 275 | 1904×1071 | 27 | | held-out |

Camera motion measured with `holdfast_run --cmc-log` (translation magnitude of the
estimated frame-to-frame affine).

**Dev / held-out.** Every tuning and design decision (gate on/off, recovery gate, max age,
OC-SORT reference threshold) was made on the 3 dev sequences. The 4 held-out sequences
were scored once, for the final table.

## Hero sequences for the demo

* **uav0000137_00458_v**: busy intersection, the densest sequence, sharp camera moves. Used for the
  side-by-side blackout demo (`scripts/demo.sh`).
* **uav0000117_02622_v**: large camera motion (up to 71 px/frame); best for showing CMC.
