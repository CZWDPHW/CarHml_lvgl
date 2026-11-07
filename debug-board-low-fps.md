# Board Low FPS Debug Record

Status: [OPEN]
Session: board-low-fps
Symptom: T507 board UI reported as severely lagging, approximately 3 FPS after the latest UI migration.

## Falsifiable hypotheses
1. Main loop or LVGL refresh cadence is blocked or throttled; compare loop and refresh intervals.
2. A high-frequency animation/timer or repeated invalidation consumes the UI thread; record timer/handler duration and invalidation activity.
3. Image decode/scale/software drawing cost dominates; compare render and image-related timings.
4. Display flush, copy, or synchronization in the board display driver dominates; record flush duration and bytes/pixels.
5. Board build/runtime settings differ from expected hardware-accelerated/optimized configuration; capture build flags, display format/resolution and backend.

## Evidence collection plan
- Inspect LVGL tick/timer handler, main loop, display flush and board build configuration.
- Add instrumentation only after identifying a low-overhead, board-readable reporting path.
- Reproduce on target and collect interval/duration counters, then rank hypotheses.

## Current evidence
- User reports about 3 FPS on board after testing.
- No logs, exact board model/configuration, display resolution, or reproduction mode supplied yet.

## Changes
- No business logic changes made.
