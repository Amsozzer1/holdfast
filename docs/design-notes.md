# Design notes

Why each decision was made, what it costs, and what the numbers said. These are also the
talking points for walking someone through the code.

## 1. What problem this is actually solving

Two things break identity on drone video:

1. **The camera moves.** A Kalman filter in pixel coordinates cannot tell "the car moved"
   from "the drone yawed". A 50 px camera jump between frames pushes every prediction off
   its object, IoU drops to zero, and every track gets a new ID at once.
2. **The feed degrades.** Frames get dropped in bursts, detections flicker, the video
   freezes, timestamps jitter. A tracker built for clean benchmark video treats each of
   these as "the object disappeared".

The metric that exposes both is **identity**, not detection: ID switches, IDF1 and AssA
(HOTA's association half). MOTA is dominated by detection errors (FP + FN are far more
numerous than ID switches), so a tracker can get *worse* at keeping identity while MOTA
barely moves. That is why the tables lead with HOTA/AssA/IDF1/IDSW.

## 2. Kalman filter

State `x = [cx, cy, w, h, vx, vy, vw, vh]`, constant-velocity model.

```
predict:  x = F x            F = [I  dt·I; 0  I]
          P = F P Fᵀ + Q(dt)
update:   S = H P Hᵀ + R      (innovation covariance, H picks the first 4 states)
          K = P Hᵀ S⁻¹        (Kalman gain)
          x = x + K (z − H x)
          P = (I − K H) P
gating:   d² = (z − Hx)ᵀ S⁻¹ (z − Hx)   ~ χ² with 2 or 4 degrees of freedom
```

Noise is proportional to box size (DeepSORT's idea): a 200 px bus and an 8 px pedestrian
should not share the same pixel noise.

**dt comes from timestamps.** Velocity is in px/second and `predict(dt)` takes real elapsed
time. A 20-frame blackout is one `predict(0.67 s)`, which lands the box where physics says
(unit test: one big step equals thirty small ones). A frame-index tracker instead calls
`predict()` once and believes one frame passed, so its prediction is 20 frames stale and
IoU matching fails. That is visible in the demo: during a blackout the baseline's boxes
stay frozen in place, Holdfast's keep moving.

**Process noise is a rate.** Q is scaled by `dt / nominal_dt`, so uncertainty grows with
time unseen, not with the number of calls. The growing dashed ellipse in the video is
the 2σ position covariance.

## 3. Camera-motion compensation (CMC)

Between consecutive received frames: downscale to 640 px wide, `goodFeaturesToTrack`,
pyramidal Lucas-Kanade, `estimateAffinePartial2D` with RANSAC (rotation, uniform scale,
translation). Moving objects are outliers to the dominant background motion, so RANSAC
drops them. The resulting 2×3 affine is applied to every track's state **before**
prediction: position and velocity get the full linear part, width/height get only the
scale (rotating a (w, h) pair as if it were a vector means nothing). Covariance is
transformed as `T P Tᵀ`. Same approach as BoT-SORT.

**Measured:** on the two dev sequences with real camera motion (95th percentile 21 px and
50 px per frame), CMC cut ID switches from 500 to 174 and raised HOTA from 58.8 to 69.7 on
clean input. On the sequence with a slow steady drift (~2 px/frame) it is neutral: a
constant-velocity model already absorbs a constant drift.

**Failure modes:** long blackouts (the two frames are far apart, LK loses the features,
falls back to identity and logs it), low-texture scenes (water, sky), and scenes dominated
by moving objects. An IMU/gimbal feed would fix the first two, which is the obvious next step
on real hardware.

Cost: ~8 ms/frame on one core at 640 px. It is the second most expensive stage after the
detector.

## 4. Association (ByteTrack-style, plus recovery)

1. **Stage 1.** Confirmed and Lost tracks vs high-score detections (≥ 0.5), cost `1 − IoU`,
   optimal assignment (own Hungarian, `O(n²m)`), IoU ≥ 0.2.
2. **Stage 2.** Still-unmatched confirmed tracks vs low-score detections (0.1–0.5). Occluded
   objects produce low-confidence boxes; throwing them away is how IDs get lost behind a
   tree. This is ByteTrack's main idea.
3. **Recovery.** Every unmatched, previously confirmed track vs remaining high-score detections,
   by **position Mahalanobis distance** (2 DOF, χ² 0.99 = 9.21) plus a size-ratio check.
   After a long gap IoU with the prediction is often zero even when the prediction is
   close, so IoU cannot recover it. Mahalanobis uses the grown covariance, so the gate
   widens automatically the longer the track has been unseen: no hand-tuned "search radius".
4. **Tentative** tracks vs what is left, IoU.
5. New tracks from unmatched detections ≥ 0.6.

**A decision the numbers reversed.** The first version put a 4-DOF Mahalanobis gate on
stage 1 as well. The ablation showed it *cost* ~3 HOTA on clean input and doubled ID
switches: the filter's covariance is tighter than the real detection noise plus residual
CMC error, so the gate rejected correct IoU matches and those objects were re-born under new
IDs. It is now off by default (`gate_chi2 = 0`), and the gate only lives in the recovery stage.

## 5. Track lifecycle

```
Tentative --3 hits--> Confirmed --miss--> Lost --recovered--> Confirmed (same ID)
Tentative --miss--> Deleted                Lost --unseen > max_age_s--> Deleted
```

`max_age` is **1.5 seconds**, not 30 frames. A frame count depends on how many frames
actually arrive, which is exactly what is broken during a blackout, and it means different
things at 10 fps and 30 fps. Unit test: deletion happens after the same wall time at both
frame rates.

## 6. OC-SORT observation-centric re-update

While a track coasts, its velocity estimate drifts and never gets corrected. When it is
re-acquired, Holdfast rolls the filter back to the state at the last real observation and
replays the gap with **virtual observations** on the straight line between the last box
and the new one, then applies the real update. This is OC-SORT's ORU.

**Measured:** on this data it is **neutral** (within ±0.3 HOTA). It helps when an object
changes direction during a gap (there is a unit test for exactly that), which is rarer in
these sequences than the linear-motion case where plain coasting is already right. It is
kept because it is cheap and principled, and reported as neutral.

## 7. Stale-frame guard

A frozen feed re-sends the same image with fresh timestamps. Without a guard the tracker
"sees" every object stand still: velocities collapse to zero and when the feed resumes the
predictions are wrong. The guard detects a repeated frame (mean absolute difference of the
downscaled image < 0.5 grey levels, computed for free inside CMC) and treats it as
**predict-only**: no updates, no new tracks, confirmed tracks reported on their predicted
position.

## 8. Why the eval uses ground truth + noise

A tracker's score is capped by its detector. Running a COCO-trained nano detector on tiny
aerial objects would bury every tracking difference under detection misses. So the table
uses `GroundTruthDetector`: VisDrone boxes with seeded jitter, occlusion-dependent miss
rates (5 / 15 / 40 %) and scores, and Poisson false positives. This isolates the tracker and
makes every run reproducible from a seed; it is the same logic as the "public detections"
track of MOT benchmarks. The real detector (YOLOX-Nano via ONNX Runtime) is used for the
latency numbers and demo footage only.

**What changes with a real detector:** misses are correlated in time and space (the same
small object is missed for many frames), scores are not calibrated, and false positives
repeat on the same background structures. All of these make recovery harder and would
lower every row of the table.

## 9. Evaluation hygiene

* Every tuning decision was made on 3 **dev** sequences. The other 4 were **held out** and
  scored once, at the end. The summary reports both.
* **OC-SORT** runs on the byte-identical detection stream, is told about missing frames
  (more than a real receiver would know), and its score threshold was tuned on the same dev
  set. Its gap to Holdfast here is mostly (a) no CMC on moving-camera sequences and (b) its
  output rule: a track is reported only after 3 consecutive hits, so every missed detection
  hides it for 3 frames. It is not evidence that Holdfast is a better general tracker.
* "Recoveries" are scored against ground truth (was it the same object?) instead of being a
  self-reported count.

## 10. Time budget (one core)

See the README latency table. On one core the detector is ~2/3 of the frame time, CMC
about a quarter, JPEG decode the rest; the tracker itself is ~0.1 ms
for ~50 detections per frame. So the tracker is not the bottleneck; the detector is, which is why
TensorRT/INT8 on a Jetson is the first thing to do on real hardware.

## 11. What I would do next

* **Learned ReID** (e.g. OSNet-x0.25 in ONNX) for the recovery stage, measured in ms per
  crop: appearance is the only signal that survives a gap long enough for motion to be useless.
* **IMU/gimbal-aided CMC** instead of pure image registration.
* **TensorRT on a Jetson** and INT8 for the detector.
* **Real detector evaluation** with a detector fine-tuned on VisDrone.
* **Bazel** build and **protobuf/gRPC** track output for integration.
