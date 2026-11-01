# Phase 0 — Weather firmware regression checklist

**Purpose:** preserve the existing Weather dashboard while platform responsibilities are extracted.  
**Reference implementation:** `main` at `7657264db4fac8437d7b2cfc5a4b080239726ee3`.  
**Hardware:** Waveshare ESP32-S3 Touch LCD 2, 320 × 240 landscape.  
**Build environment:** see [Step 3 — Build and toolchain baseline](PHASE0_STEP3_BUILD_TOOLCHAIN.md).  
**Memory comparison:** see [Phase 0 memory profile](PHASE0_MEMORY_PROFILE.md).

This is a manual physical-device checklist. The repository currently has no checked-in automated firmware test project or CI test suite. Do not report an automated test pass when these manual checks are performed.

## 1. Before a test run

- [ ] Start from a clean working tree and record the branch and commit being tested.
- [ ] Use the recorded board FQBN and ESP32 core version in the Step 3 document.
- [ ] Confirm the sketch is compiled from a folder named `weatherDashboard`, matching the root `weatherDashboard.ino` sketch name.
- [ ] Create local `secrets.h` from `secrets.example.h` only when live-mode credentials are needed. Confirm it is ignored by Git and never include its contents in logs or commits.
- [ ] Record whether the build uses live mode (`USE_DEMO_WEATHER_DATA 0`) or offline demo mode (`1`).
- [ ] Use demo mode for repeatable UI/touch tests; use live mode only for network, location, time and real-data validation.
- [ ] Record power source, USB connection, Wi-Fi environment and any attached peripherals that could affect results.

## 2. Build and boot

| Check | Procedure | Pass condition | Baseline status |
|---|---|---|---|
| Clean compile | Compile with the documented board options and installed libraries | Build succeeds without errors | Verified in Step 3 |
| Upload / boot | Upload to the Waveshare board and restart | Firmware starts; no reboot loop or display-init failure | Verified in Step 3; repeat after each relevant extraction |
| Display setup | Observe orientation, backlight, colours and full-screen layout | 320 × 240 landscape image is legible and correctly aligned | Existing baseline |
| Touch controller | Observe CST816 detection and tap/swipe | Controller is detected and input is registered | Existing baseline; repeat after input changes |
| Canvas allocation | Capture framebuffer region and heap summary where available | Existing canvases are available or use their documented fallback; on the recorded build all three are in PSRAM | Verified in memory profile |

## 3. Weather and forecast rendering

| Check | Procedure | Pass condition |
|---|---|---|
| Current Weather screen | Boot into Weather or return from Forecast | Location/header, current conditions, icon and metric cards render |
| Metric content | Check all available metric values and labels | Values are aligned, readable and clipped within the intended panel |
| Metric vertical drag | Drag the metric panel vertically | Cards scroll without unintended tab navigation |
| Metric boundaries | Drag to both ends and release | Scroll stays within limits; elastic overscroll and snap behaviour remain consistent |
| Forecast list | Open Forecast in list mode | Hourly entries render with time, conditions, temperature/feels-like and precipitation probability |
| Forecast list drag | Drag the list up and down | List follows the drag and remains clipped to the viewport |
| Forecast list boundaries | Drag to the first and last available entries | No blank/out-of-range content; final position is bounded |
| List/graph selector | Switch between List and Graph | Correct view appears; selector state is accurate |
| Forecast graph | Inspect plotted values, time labels and temperature range | Graph uses the available forecast records and labels remain legible |
| Graph drag / hold | Hold and drag along the graph | Point/scroll interaction works; graph retains ownership where intended |
| Graph gesture suppression | Finish a graph drag and attempt a swipe | One physical gesture is not processed twice as both drag and tab navigation |
| Forecast return navigation | Swipe from Forecast back to Weather | Weather returns without a stale transition frame |
| Tab transition | Switch tabs repeatedly | Transition renders correctly; fallback remains usable if transition canvas is unavailable |

## 4. Input and interaction

- [ ] Test each accepted horizontal tab gesture in both directions.
- [ ] Test forecast vertical gesture fallback in both directions.
- [ ] Verify vertical gestures starting outside the metric panel do not scroll metrics.
- [ ] Verify horizontal intent over the metric panel navigates tabs rather than starting a vertical drag.
- [ ] Verify touch-down, movement, release, snap and controller-gesture suppression do not double-apply one gesture.
- [ ] Repeat the same gesture after a neutral/no-gesture sample to check that a latched gesture does not block the next attempt.
- [ ] Test tap targets in the forecast selector and any visible controls.
- [ ] Test interaction during metric snap animation and tab transition.

The exact coordinate/gesture arbitration is documented in [Step 2 — Runtime data-flow map](PHASE0_STEP2_RUNTIME_DATA_FLOW.md). Treat it as regression-sensitive; do not simplify the separate metric, forecast-list and graph state just because some values look similar.

## 5. Clock and location

| Check | Procedure | Pass condition | Status |
|---|---|---|---|
| Time sync | Start live mode with internet access | TimeAPI sets a valid system clock; header displays UK local time | Existing baseline |
| Clock continuity | Leave device running across a minute change | Clock advances locally and updates without a request every second | Existing baseline |
| Offline reboot | Restart demo/offline or live mode with network unavailable | Demo clock is deterministic; live mode clearly indicates unavailable time until sync succeeds | Documented behaviour |
| Wi-Fi positioning | Restart live firmware with Google key and access points available | Scan and successful location result are logged; accuracy metadata is retained in output | Observed once in Phase 0 |
| IP fallback | Test with Google lookup unavailable/disabled | IP location is attempted; configured coordinates remain the final fallback | Documented in source; physical fallback path not yet verified |
| Location display | Inspect header and weather requests after location resolution | The selected location name and coordinates are used consistently | Existing baseline |

## 6. Network and data refresh

| Check | Procedure | Pass condition | Status |
|---|---|---|---|
| Current-weather request | Trigger or wait for a live refresh | Weather data updates or the existing error/stale-data path remains usable | Healthy request measured |
| Hourly forecast request | Trigger or wait for forecast refresh | Paginated hourly data loads, up to the configured capacity | Healthy two-page request measured |
| Combined refresh | Exercise current weather followed by forecast | Both data sets update and the active view redraws | Measured; see Step 4 / memory profile |
| Cached icon | Redraw Weather with the current icon already cached | No network download is initiated for the same cached asset | Observed cache-hit path |
| Icon cache miss | Cause a new valid icon to be requested | PNG downloads, cache updates and rendering completes | One forced cache miss measured |
| HTTP failure | Cause a controlled request failure without exposing credentials | Error/stale-data state is clear; input recovers after the operation | Pending |
| Wi-Fi unavailable at startup | Boot with the access point unavailable | UI reaches a defined fallback/error state and can recover when network returns | Pending |
| Wi-Fi loss while running | Disconnect the access point during normal use | UI remains recoverable and reconnect behaviour is understood | Pending |
| Periodic refresh | Leave device running until scheduled refresh | Refresh occurs and the dashboard returns to the active view | Basic path documented; longer soak pending |

## 7. Responsiveness and animation

- [ ] Record idle loop/touch cadence before a change.
- [ ] Measure touch-poll gap during metric scroll/snap animation.
- [ ] Measure touch-poll gap during tab transition.
- [ ] Measure touch-poll gap during a healthy weather request.
- [ ] Measure touch-poll gap during forecast pagination.
- [ ] Measure touch-poll gap during a forced icon cache miss.
- [ ] Measure physical touch-to-visible-response latency for representative taps and drags.
- [ ] Repeat the above after any change to scheduling, network work, input ownership or rendering.

The initial Step 4 measurements include idle gaps of 32–33 ms, weather request durations around 718–726 ms and forecast duration around 907 ms. The later memory session measured a 564 ms icon cache miss and a 1,207 ms maximum full-refresh loop. See [Step 4](PHASE0_STEP4_BLOCKING_RESPONSIVENESS_AUDIT.md) for conditions and limitations.

**Important:** the temporary diagnostic's full-refresh summary contained a known reporting-window mismatch between maximum loop duration and maximum touch-poll gap. Do not use its 31 ms full-refresh touch-gap value as a pass result. Physical touch-to-response latency has not yet been measured.

## 8. Memory and resource checks

For a change that affects buffers, models, strings, caches, networking or app lifecycle:

- [ ] Record current free internal 8-bit heap and largest block before the operation.
- [ ] Record the same values after data loading and after rendering.
- [ ] Record PSRAM free space and largest block.
- [ ] Check each framebuffer pointer's memory region and requested size.
- [ ] Compare low-water marks separately; they are since-boot measurements.
- [ ] Repeat the operation to look for progressive loss or shrinking largest blocks.
- [ ] Confirm failure/fallback paths release temporary allocations.
- [ ] Compare results with the [Phase 0 memory profile](PHASE0_MEMORY_PROFILE.md).

Reference values after startup on the recorded board/configuration were 213,608 bytes free internal heap, 118,772 bytes largest internal block and 110,532 bytes minimum-ever internal free. All three canvases were in PSRAM. Treat these as comparison data, not universal acceptance thresholds.

## 9. Credentials and security

- [ ] Confirm `secrets.h` is ignored and is not staged.
- [ ] Never commit Wi-Fi passwords, API keys, APN values, SIM identifiers or modem credentials.
- [ ] Do not copy live secrets into serial logs, screenshots or PR descriptions.
- [ ] Keep TLS certificate validation visible as a separate security issue; do not silently treat `setInsecure()` as a final platform policy.

## 10. Test record

For each extraction PR, record:

| Field | Value |
|---|---|
| Branch / commit | |
| Board FQBN / core | |
| Demo or live mode | |
| Hardware / power / network conditions | |
| Checks run | |
| Checks passed / failed | |
| Memory before / after | |
| Timing observations | |
| Known limitations | |
| Reviewer / date | |

Only mark a check as passed when the procedure has actually been performed on the stated firmware revision and hardware. Static code inspection is useful evidence, but it is not a substitute for physical validation.
