# Phase 0 — Weather firmware regression checklist

**Purpose:** preserve the existing Weather dashboard while platform responsibilities are extracted.  
**Reference implementation:** `main` at PR #34 merge commit `b11416b62ff91b7f917f16c8a484d0d2e6b7f02d`.  
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

## PR #33 — Scroll-controller extraction regression (passed)

The user confirmed on 2026-11-02 that Arduino IDE Verify, upload/boot and the following physical checks passed on the Waveshare ESP32-S3 Touch LCD 2.

- [x] Metric cards: drag within the panel in both directions; 1:1 movement inside bounds, resisted/capped overscroll at both ends, and return to the nearest valid row on release passed.
- [x] Forecast list: drag within the list viewport in both directions; edge resistance and nearest-row snap remained correct without changing list offset ownership or redraw throttling.
- [x] Forecast graph: horizontal drag after the existing hold threshold retained resisted/capped overscroll at both ends and snapped to the nearest forecast point spacing.
- [x] Graph-specific behaviour: quick rightward tab return, hold-to-drag distinction, point selection and horizontal paging remained unchanged.
- [x] Cross-view behaviour: switching between Weather and Forecast and immediately starting a fresh touch after release produced no stale scroll position or ownership leakage.

The PR #33 helper is stateless and does not own offsets, drag state, release timing, rendering or animation. The user confirmed that the physical-device checks above passed on the PR #33 firmware.

## 4. Input and interaction

- [ ] Test each accepted horizontal tab gesture in both directions.
- [ ] Test forecast vertical gesture fallback in both directions.
- [ ] Verify vertical gestures starting outside the metric panel do not scroll metrics.
- [ ] Verify horizontal intent over the metric panel navigates tabs rather than starting a vertical drag.
- [ ] Verify touch-down, movement, release, snap and controller-gesture suppression do not double-apply one gesture.
- [ ] Repeat the same gesture after a neutral/no-gesture sample to check that a latched gesture does not block the next attempt.
- [ ] Test tap targets in the forecast selector and any visible controls.
- [ ] Test interaction during metric snap animation and tab transition.
- [x] Confirm metric-panel and forecast-list contacts remain unclaimed candidates until vertical intent is established; horizontal intent must still reach tab navigation.
- [x] Confirm vertical drag activation claims the correct view owner and suppresses the controller's duplicate gesture until the release grace period expires.
- [x] Confirm graph contacts remain owned from touch-down to release, including coordinate dropout; held horizontal drag and quick rightward tab return retain their existing distinction.
- [x] Confirm the forecast selector owns a stationary tap until release, while movement beyond tap slop cancels the control and leaves horizontal navigation available.
- [x] Start a new touch immediately after release and confirm no stale owner suppresses its gesture.

The PR #32 interaction-ownership checks above were physically regression-tested on the Waveshare ESP32-S3 Touch LCD 2 and confirmed passed by the user on 2026-11-02. The exact coordinate/gesture arbitration is documented in [Step 2 — Runtime data-flow map](PHASE0_STEP2_RUNTIME_DATA_FLOW.md). Treat it as regression-sensitive; do not simplify the separate metric, forecast-list and graph state just because some values look similar.

## PR #34 — TimeService extraction regression (physical test passed)

The user confirmed the PR #34 physical test passed on 2026-11-02. Individual checklist detail was not included in the confirmation, so the specific scenarios below remain unmarked rather than inferred.

- [ ] Build with Arduino IDE 2.3.10, ESP32 core 3.3.12 and the exact Waveshare ESP32-S3 FQBN recorded in the toolchain baseline.
- [ ] Upload and boot; confirm the header renders without a crash while the system clock is initially invalid or waiting for its first successful sync.
- [ ] With Wi-Fi and TimeAPI available, confirm the header changes from `--:--` to the correct Europe/London local time after a successful response.
- [ ] Confirm the clock advances and the header redraws only when its displayed minute changes; it must not trigger a TimeAPI request every loop or every header redraw.
- [ ] Confirm the retry policy remains five minutes after a failed initial/periodic attempt and the successful-sync interval remains six hours (verify by source/scheduler inspection rather than waiting six hours during a manual session).
- [ ] In demo mode, confirm the fixed 2026-10-28 18:00 Europe/London clock is set and advances normally without network access.
- [ ] Confirm forecast list/graph labels still use the weather provider's `locationTimezoneOffset`, not the device's Europe/London timezone or TimeService state.

TimeService deliberately does not own Wi-Fi association, header drawing/redraw cadence, or forecast timezone conversion. The Google Wi-Fi → IP location fallback is unchanged and is not part of PR #34.

## PR #35 — LocationService extraction regression (pending)

The LocationService extraction is implemented on `phase1/09-location-service`. No Arduino build, upload or physical provider/fallback test has yet been confirmed. Keep the checks below unmarked until each has been run and reported.

- [ ] Build with Arduino IDE 2.3.10, ESP32 core 3.3.12 and the exact Waveshare ESP32-S3 FQBN recorded in the toolchain baseline.
- [ ] Upload and boot in live mode; confirm the Weather screen, header and weather requests behave as before.
- [ ] With a valid Google Geolocation key and at least two eligible nearby BSSIDs, confirm Google Wi-Fi positioning succeeds and the returned coordinates are used for current weather and hourly forecast requests.
- [ ] Confirm the selected fix reports source `GoogleWifi`, a valid coordinate flag and an observation time from `millis()`; display an accuracy radius only when Google's response supplies one.
- [ ] Disable/omit the Google key or force the Wi-Fi positioning path to fail; confirm the service attempts IP geolocation next rather than skipping directly to configured coordinates.
- [ ] With ipwho.is available after Google fails, confirm source `IpGeolocation`, valid coordinates and no claimed accuracy radius; confirm the returned city name remains the fallback display name if reverse geocoding fails.
- [ ] Make both providers fail; confirm source `ConfiguredFallback` and the configured latitude/longitude remain the final fallback.
- [ ] Confirm reverse geocoding changes only the display name and never replaces the Google/IP coordinates.
- [ ] Confirm LocationService does not start, stop or reconfigure Wi-Fi; Weather continues to own Wi-Fi connection lifecycle.
- [ ] Confirm when the configured fallback is selected, the existing OpenWeather current-weather response may still supply its reporting-station coordinates, while a resolved Google/IP fix is not overwritten.
- [ ] Confirm forecast labels continue to use `locationTimezoneOffset` and are unaffected by LocationFix metadata or the Europe/London device clock.

LocationService deliberately retains the existing synchronous HTTPS clients and `setInsecure()` policy for this isolated extraction. A shared request executor, TLS-policy change and GNSS integration are not part of PR #35.

## 5. Clock and location

| Check | Procedure | Pass condition | Status |
|---|---|---|---|
| Time sync | Start live mode with internet access | TimeAPI sets a valid system clock; header displays UK local time | Existing baseline |
| Clock continuity | Leave device running across a minute change | Clock advances locally and updates without a request every second | Existing baseline |
| Offline reboot | Restart demo/offline or live mode with network unavailable | Demo clock is deterministic; live mode clearly indicates unavailable time until sync succeeds | Documented behaviour |
| Wi-Fi positioning | Restart live firmware with Google key and access points available | Scan and successful location result are logged; source and available accuracy metadata are retained in the LocationFix | Observed once in Phase 0; PR #35 regression pending |
| IP fallback | Test with Google lookup unavailable/disabled | IP location is attempted; configured coordinates remain the final fallback | Source behaviour retained; PR #35 regression pending |
| Location display | Inspect header and weather requests after location resolution | The selected location name and coordinates are used consistently | Existing baseline; PR #35 regression pending |

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
