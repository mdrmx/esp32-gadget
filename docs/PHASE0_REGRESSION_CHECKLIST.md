# Phase 0 — Weather firmware regression checklist

**Purpose:** preserve the existing Weather dashboard while platform responsibilities are extracted.  
**Reference implementation:** `main` at PR #35 merge commit `fbd87e9b73b4a21e4b2579a7e2d0c15708e410cb`.  
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

- [x] Arduino IDE Verify passed on 2026-11-02 with Arduino IDE 2.3.10, ESP32 core 3.3.12 and the exact Waveshare ESP32-S3 FQBN recorded in the toolchain baseline. Build output: 1,151,199 bytes program storage (36% of 3 MiB app partition); 97,228 bytes global dynamic memory (29% of 327,680 bytes), leaving 230,452 bytes. ESP32-S3 image and merged 16 MB flash image generation succeeded.
- [ ] Upload and boot; confirm the header renders without a crash while the system clock is initially invalid or waiting for its first successful sync.
- [ ] With Wi-Fi and TimeAPI available, confirm the header changes from `--:--` to the correct Europe/London local time after a successful response.
- [ ] Confirm the clock advances and the header redraws only when its displayed minute changes; it must not trigger a TimeAPI request every loop or every header redraw.
- [ ] Confirm the retry policy remains five minutes after a failed initial/periodic attempt and the successful-sync interval remains six hours (verify by source/scheduler inspection rather than waiting six hours during a manual session).
- [ ] In demo mode, confirm the fixed 2026-10-28 18:00 Europe/London clock is set and advances normally without network access.
- [ ] Confirm forecast list/graph labels still use the weather provider's `locationTimezoneOffset`, not the device's Europe/London timezone or TimeService state.

TimeService deliberately does not own Wi-Fi association, header drawing/redraw cadence, or forecast timezone conversion. The Google Wi-Fi → IP location fallback is unchanged and is not part of PR #34.

## PR #35 — LocationService extraction regression (passed)

The user confirmed on 2026-11-02 that the LocationService firmware works in physical testing on the Waveshare ESP32-S3 Touch LCD 2. The full provider, fallback, display, Wi-Fi ownership and forecast-timezone regression matrix below is recorded as passed.

**Arduino IDE Verify:** passed using Arduino IDE 2.3.10, ESP32 core 3.3.12 and the exact Waveshare FQBN above. The uploaded output reports 1,150,279 bytes program storage (36%), 97,228 bytes global dynamic memory (29%) and 230,452 bytes remaining. The output reused cached sketch/library objects, then completed the link and ESP32 image generation; it was not a forced clean rebuild.

- [x] Build with Arduino IDE 2.3.10, ESP32 core 3.3.12 and the exact Waveshare ESP32-S3 FQBN recorded in the toolchain baseline.
- [x] Upload and boot in live mode; confirm the Weather screen, header and weather requests behave as before.
- [x] With a valid Google Geolocation key and at least two eligible nearby BSSIDs, confirm Google Wi-Fi positioning succeeds and the returned coordinates are used for current weather and hourly forecast requests.
- [x] Confirm the selected fix reports source `GoogleWifi`, a valid coordinate flag and an observation time from `millis()`; display an accuracy radius only when Google's response supplies one.
- [x] Disable/omit the Google key or force the Wi-Fi positioning path to fail; confirm the service attempts IP geolocation next rather than skipping directly to configured coordinates.
- [x] With ipwho.is available after Google fails, confirm source `IpGeolocation`, valid coordinates and no claimed accuracy radius; confirm the returned city name remains the fallback display name if reverse geocoding fails.
- [x] Make both providers fail; confirm source `ConfiguredFallback` and the configured latitude/longitude remain the final fallback.
- [x] Confirm reverse geocoding changes only the display name and never replaces the Google/IP coordinates.
- [x] Confirm LocationService does not start, stop or reconfigure Wi-Fi; Weather continues to own Wi-Fi connection lifecycle.
- [x] Confirm when the configured fallback is selected, the existing OpenWeather current-weather response may still supply its reporting-station coordinates, while a resolved Google/IP fix is not overwritten.
- [x] Confirm forecast labels continue to use `locationTimezoneOffset` and are unaffected by LocationFix metadata or the Europe/London device clock.

LocationService deliberately retains the existing synchronous HTTPS and `setInsecure()` policy for PR #35. The shared request-client extraction is tracked separately in PR #36; TLS policy and GNSS integration remain separate decisions.

## PR #36 — shared HTTP request client extraction (merged; Verify and device regression passed)

`HttpRequestClient` is a single-use synchronous request session used by TimeService and LocationService. PR #36 merged at `2cf68e08ddf7b569ea57777ca12147069f506b26`.

- [x] Final parser-follow-up Arduino IDE Verify passed using Arduino IDE 2.3.10, ESP32 core 3.3.12, esptool 5.3.1 and the exact Waveshare ESP32-S3 FQBN: `esp32:esp32:esp32s3:CDCOnBoot=cdc,UploadMode=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi`.
- [x] Build result: 1,151,215 bytes program storage (36% of 3 MiB); 97,228 bytes global dynamic memory (29% of 327,680 bytes); 230,452 bytes remaining. ESP32-S3 image and merged 16 MB flash image generation completed successfully. The output reused cached sketch/library objects and completed linking/image generation; it is recorded as a successful Verify with cache reuse, not a clean rebuild.
- [x] Upload and boot of the parser-follow-up firmware on the Waveshare ESP32-S3 Touch LCD 2.
- [x] Google Wi-Fi positioning succeeded: 15 scanned networks, 9 usable BSSIDs, coordinates 53.528682, -2.267461, reported accuracy radius about 18 m, selected source `GoogleWifi`, valid flag `1`, observed at 5,607 ms.
- [x] OpenWeather reverse geocoding returned `Bury` for the Google Wi-Fi fix.
- [x] Confirmed the device displays the correct UK local time.
- [x] User confirmed all tested touch interactions and navigation controls work.
- [x] User confirmed current-weather and hourly-forecast data load correctly.
- [x] Earlier failure-path test confirmed Google response parse failure falls back to IP geolocation: ipwho.is returned Manchester coordinates (`53.48095, -2.23743`) and reverse geocoding returned `Manchester`.
- [x] TimeService retains its 5,000 ms connect/response timeout; LocationService retains its existing per-request timeouts and response handling.
- [x] Google Geolocation retains the 10,000 ms connection timeout, 12,000 ms response timeout, JSON content-type header and bounded 220-byte error preview.
- [x] Migrated TimeService and LocationService calls explicitly retain the existing insecure-TLS compatibility policy.
 
## PR #37 — Weather requests through shared HTTP request client (merged; all device tests passed)

This slice migrates only transport ownership for the remaining Weather HTTPS calls. JSON parsing, Weather state, forecast pagination and icon cache ownership remain in `weatherDashboard.ino`.

- [x] Arduino IDE Verify passed using Arduino IDE 2.3.10, ESP32 core 3.3.12, esptool 5.3.1 and FQBN `esp32:esp32:esp32s3:CDCOnBoot=cdc,UploadMode=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi`. Build: 1,151,707 bytes program storage (36% of 3 MiB); 97,228 bytes global dynamic memory (29% of 327,680 bytes); 230,452 bytes remaining. ESP32-S3 image and merged 16 MB flash image generated successfully.
- [x] User uploaded and booted the PR #37 firmware on the Waveshare ESP32-S3 Touch LCD 2; startup completed and proceeded into location resolution.
- [x] Location smoke test passed after upload: 14 Wi-Fi networks scanned, 9 eligible BSSIDs, Google Wi-Fi returned 53.528695, -2.267449 with an 18 m accuracy radius, OpenWeather reverse geocoded `Bury`, and `Google Wi-Fi` was selected as a valid fix observed at 6,537 ms.
- [x] User confirmed all remaining PR #37 device regression tests passed, including icon PNG fetch/cache, current-weather display/data, One Call hourly forecast, UV/rain metrics, page loading/pagination, partial-data behaviour and full touch/navigation regression.
- [x] Confirm OpenWeather icon download still validates content length and PNG signature, caches the image and reuses a matching cached icon.
- [x] Confirm icon download retains the 10,000 ms connect/response timeouts, 20,000-byte maximum and 8-second stream-read budget.
- [x] Confirm current-weather GET retains its URL parameters, 15,000 ms connect/response timeouts, HTTP status mapping, JSON parse and weather-field updates.
- [x] Confirm One Call page GET retains its URL/pagination handling, 15,000 ms connect/response timeouts, bounded 240-character non-200 body diagnostic and error mapping.
- [x] Confirm One Call JSON parsing retains timezone offset, next-page URL, weather fields, UV/rain availability and duplicate-timestamp handling.
- [x] Confirm forecast requests remain sequential, follow at most three pages and never exceed 48 records.
- [x] Confirm partial forecast data remains available if a later page fails after earlier pages succeeded.
- [x] Confirm current weather is fetched before the hourly forecast at startup and on scheduled refresh; no concurrent requests are introduced.
- [x] Confirm all three migrated call sites explicitly select the existing insecure-TLS compatibility policy.
- [x] Confirm all request sessions end on success and each failure path, including icon stream-unavailable and incomplete-download paths.
- [x] Confirm the UI, touch interactions, clock, location provider selection and current/forecast data remain unchanged after the transport migration.

The wrapper supports root-CA configuration before request initialisation, but PR #36 does not install a certificate or change the existing insecure-TLS behaviour.

## PR #38 — WeatherDataService and data model extraction (merged and hardware-validated)

`WeatherDataService` owns current-weather and One Call requests/parsing, forecast pagination, weather-data validity and fixed-capacity data models. Wi-Fi association, LocationService/provider selection, icon PNG download/cache, loading/error UI and rendering remain outside the service. The One Call non-200 diagnostic reads a maximum of 240 bytes into a fixed stack buffer. Unused sketch aliases `locationTimezoneOffset` and `weatherMain` were removed. The service preserves first-valid-hour UV/rain selection order, including when an initial record is skipped as a duplicate. The sketch delegates Wi-Fi-unavailable forecast state to the service. PR #38 merged at `716670c118cec37488cd895117d533a69c31d407` on 2026-11-12. The user confirms hardware regression passed. Per project convention, Verify and upload/boot are treated as passed on merged PRs; exact build-size/memory figures were not supplied.

- [x] Arduino IDE Verify using the recorded Waveshare ESP32-S3 16 MB / OPI PSRAM FQBN and ESP32 core 3.3.12 after the review-fix commits.
- [x] Upload and boot on the Waveshare ESP32-S3 Touch LCD 2.
- [x] Confirm current conditions, weather availability, descriptions, icon code and all metric values match the pre-extraction firmware.
- [x] Confirm One Call forecast page parsing, timezone offset, next-page URL and forecast record mapping remain correct.
- [x] Confirm sequential pagination stops after at most three pages or 48 records and filters duplicate timestamps.
- [x] Confirm partial forecast data remains available if a later page fails after earlier records have been received.
- [x] Confirm a first-page failure marks the forecast unavailable and preserves the existing error mapping.
- [x] Confirm a later-page failure marks the forecast as available and retains the earlier records while exposing the page error in `FetchResult`.
- [x] Confirm forecast timezone remains separate from the Europe/London device clock.
- [x] Confirm UV index/rain probability values and their availability flags retain existing semantics, including first-valid-hour selection before duplicate timestamp filtering.
- [x] Confirm the offline demo fixture still loads the same current weather and 24 forecast entries without network calls.
- [x] Confirm Wi-Fi association and LocationService/provider selection remain outside WeatherDataService.
- [x] Confirm icon PNG download/cache stays in the sketch and continues to work.
- [x] Confirm loading/error rendering, screen state, metric/list/graph rendering, clock and all touch interactions remain unchanged.
- [x] Confirm current weather is fetched before hourly forecast; no concurrent requests are introduced.


## PR #39 — WeatherApp application shell (merged and hardware-validated)

`WeatherApp` coordinates the application lifecycle while the sketch retains all current view, render and input implementations. The shell is injected with fixed function pointers and has no Arduino, networking, display or concrete Weather-model dependencies. Startup remains current weather then forecast; scheduled refresh remains synchronous, every ten minutes, and redraws the active tab. The device loop retains its 30 ms pacing delay.

- [x] C++11 host compile with `-Wall -Wextra -Werror`.
- [x] Host lifecycle test: reject incomplete hooks and do not activate.
- [x] Host lifecycle test: initial current-weather fetch precedes forecast fetch and initial Weather render.
- [x] Host lifecycle test: clock refresh cadence is driven by the injected millis callback.
- [x] Host lifecycle test: periodic refresh runs current weather then forecast and redraws the active Forecast tab.
- [x] Host lifecycle test: `end()` makes the app inactive and subsequent updates invoke no callbacks.
- [x] Review follow-up: start the one-second clock cadence from the timestamp recorded after initial Weather rendering, avoiding a redundant immediate first-loop check.
- [x] Review follow-up: error-render adapter uses an `Unknown error` fallback for a null message.
- [x] Arduino IDE Verify using the recorded Waveshare ESP32-S3 16 MB / OPI PSRAM FQBN and ESP32 core 3.3.12.
- [x] Upload and boot on the Waveshare ESP32-S3 Touch LCD 2.
- [x] Confirm current-weather/forecast startup, time sync and clock refresh remain unchanged.
- [x] Confirm metric cards, forecast list/graph, scrolling, tab navigation and touch interactions remain unchanged.
- [x] Confirm the 10-minute refresh redraws whichever Weather tab is active.
- [x] Confirm offline demo mode still starts and runs without network calls.

The user confirms the PR #39 physical validation checks passed on the Waveshare hardware. Arduino Verify, upload/boot and all physical regression checks in this section are therefore recorded as passed. PR #39 merged at `3c613363f3b57bebe4245350cc57a3d6aa7a52d6`; exact build-size and memory figures were not supplied.

## PR #40 — WeatherViewState extraction (passed)

`WeatherViewState` owns the active tab, canonical Weather metric and forecast scroll offsets, and List/Graph mode. The user confirmed the physical Waveshare regression passed before merge. By project convention, Arduino IDE Verify and upload/boot are also recorded as passed. Exact build-size and memory figures were not supplied.

- [x] Compile the standalone state test with C++11, `-Wall -Wextra -Werror -pedantic`.
- [x] Confirm default state is Weather tab, first metric/hour, and List mode.
- [x] Confirm main-tab reset clears metric/list/graph offsets while preserving the selected tab and forecast mode.
- [x] Confirm selecting Graph resets list and graph offsets and selects Graph mode.
- [x] Confirm selecting List resets list and graph offsets and selects List mode.
- [x] Run `sh scripts/test-weather-view-state.sh`; host test reports `WeatherViewState tests passed.`
- [x] Arduino IDE Verify, upload and boot on the Waveshare ESP32-S3 Touch LCD 2 (recorded by project convention after user-confirmed physical validation).
- [x] Confirm startup, Weather/Forecast drawing and active-tab refresh remain unchanged.
- [x] Confirm metric-card scrolling, forecast-list drag/gesture scrolling and graph drag/scroll remain unchanged.
- [x] Confirm switching the main tab resets canonical scroll positions while retaining the selected List/Graph forecast mode.
- [x] Confirm List/Graph selector taps still reset forecast position to the first hour and update only the selected presentation.
- [x] Confirm the ten-minute refresh and offline demo mode remain unchanged.

Merged at `266ef646022353dd57bb552fc17a7ba523ad8708`. No firmware build-size or memory figures are recorded.

## PR #41 — TextRenderer extraction (merged at `108805abfeb290e0b87b497960acff8e85bc7a3d`)

`TextRenderer` moves shared text primitives from `weatherDashboard.ino` into `src/ui/TextRenderer.h`: direct/centred drawing, width/height measurement, bounded two-line wrapping and the centred Weather-line helper. It resolves `DisplayManager::target()` at each operation, preserving physical-display and tab-transition drawing. The final merged revision includes null-target guards and the corrected over-wide centred-text clamp.

- [x] Strict C++11 host tests, including null-target behaviour and text wider than the display.
- [x] Both Gemini inline review comments replied to and resolved.
- [x] PR #41 merged at `108805abfeb290e0b87b497960acff8e85bc7a3d`.
- [x] Under the project convention, Arduino IDE Verify, upload/boot and required Waveshare regression are recorded as passed on the final merged revision.
- [x] No build-size figures were inferred for the final review-fixed merge. The previously recorded 1,164,219 bytes program storage, 97,380 bytes global dynamic memory and 230,300 bytes remaining are tied only to pre-review revision `36c4d634dc9ee3a66ed6c9ca4286d41b0b216700`.

## PR #42 — HeaderRenderer extraction (merged at `ba92b8b53da8457caa18bb454f53c0d51ca6687a`)

`HeaderRenderer` moves the fixed 36 px header background, location/clock text placement, divider and clock-only redraw region into `src/ui/HeaderRenderer.h`. Weather retains location resolution, Weather data access, `TimeService`, `lastHeaderClock` and the refresh decision; colours are passed into the renderer. The component uses `TextRenderer` and the active `DisplayManager::target()`.

- [x] Add the renderer and route full-header / clock-only draw calls through it.
- [x] Add strict C++11 host coverage for header geometry, location/clock text and colours, divider, clock-only repaint, long-clock x-clamp, null target and transition-canvas routing.
- [x] User ran `sh scripts/test-text-renderer.sh` after the shared graphics stub change; output: `TextRenderer tests passed.`
- [x] Equivalent local C++11 strict-warning host checks passed.
- [x] User ran `sh scripts/test-header-renderer.sh`; output: `HeaderRenderer tests passed.`
- [x] Arduino IDE compile / Verify succeeded on the current PR #42 head using ESP32 core 3.3.12 and FQBN `esp32:esp32:esp32s3:CDCOnBoot=cdc,UploadMode=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi`.
- [x] Build output: 1,164,667 bytes program storage (37% of 3 MiB); 97,396 bytes global dynamic memory (29% of 327,680 bytes); 230,284 bytes remaining. ESP32-S3 image and merged 16 MB flash image generated successfully.
- [x] Upload/flash completed on `/dev/cu.usbmodem101`; written-data hashes verified and final hard reset completed.
- [x] User confirmed the board booted successfully and physical validation passed on 2026-11-13.
- [x] Confirm normal header location/clock layout is preserved.
- [x] Confirm clock-only redraw at minute change, long clock text clamp and header drawing through tab-transition composition.
- [x] Confirm Weather/Forecast navigation, loading/error screens and all existing touch/scroll/graph behaviour remain unchanged; user reports everything is preserved.

Arduino build, upload/flash, both repository host tests and the complete physical regression are recorded as passed. No physical timing or runtime memory-profile measurements are claimed.


## PR #44 — StatusScreenRenderer extraction (merged at `4a567bef43a07a8f50ecd9deb434c67051104b70`)

`StatusScreenRenderer` moves only the Weather loading/error screen composition into `src/ui/StatusScreenRenderer.h`. Weather continues to select status messages, manage Wi-Fi and data outcomes, resolve the header location, read `TimeService`, maintain the clock redraw cache and supply theme colours. The renderer composes the background, existing header and status text using the active `DisplayManager::target()`.

- [x] Add `StatusScreenRenderer` with explicit location/clock strings and a supplied colour palette.
- [x] Route `drawLoading()` and `drawError()` through the new renderer without moving status/message policy out of Weather.
- [x] Add strict C++11 host coverage for loading/error layout, geometry, text sizes/colours, null-target no-op and transition-canvas routing.
- [x] Add `sh scripts/test-status-screen-renderer.sh`.
- [x] User ran `sh scripts/test-status-screen-renderer.sh`; output: `StatusScreenRenderer tests passed.`
- [x] User ran `sh scripts/test-header-renderer.sh`; output: `HeaderRenderer tests passed.`
- [x] User ran `sh scripts/test-text-renderer.sh`; output: `TextRenderer tests passed.`
- [x] User confirmed firmware compilation and upload succeeded.
- [x] User confirmed all physical Waveshare tests passed and all existing interactions are preserved.

PR #44 is merged at `4a567bef43a07a8f50ecd9deb434c67051104b70`. All three repository renderer suites passed; the user confirmed firmware compilation and upload succeeded, all physical Waveshare tests passed and all existing interactions were preserved. No build-size or memory figures were supplied for this firmware revision.

## PR #45 — ForecastListItemRenderer extraction (in progress)

`ForecastListItemRenderer` moves hourly forecast list-card composition, compact vector condition icons, precipitation percentage formatting/clamping and clipped edge-card drawing into `src/ui/ForecastListItemRenderer.h`. Weather retains forecast data selection, compact time formatting, list/graph selection, viewport placement, scroll state, canvas lifecycle and screen composition.

- [ ] Run `sh scripts/test-forecast-list-item-renderer.sh` with strict C++11 warnings.
- [ ] Arduino IDE Verify on the recorded Waveshare ESP32-S3 16 MB / OPI PSRAM configuration and ESP32 core 3.3.12.
- [ ] Upload and confirm successful boot.
- [ ] Physically check forecast List mode: time, condition icons (clear/cloud/rain/snow/thunder/mist), temperature, feels-like and precipitation labels.
- [ ] Check first/last partially visible cards, vertical scrolling, overscroll/snap, List/Graph switching and tab transitions.
- [ ] Confirm Graph mode, Weather screen, loading/error screens and all touch interactions remain unchanged.

No host-test execution, Arduino build/upload or physical validation is claimed yet. No build-size or memory figures are available for this branch.

## 5. Clock and location

| Check | Procedure | Pass condition | Status |
|---|---|---|---|
| Time sync | Start live mode with internet access | TimeAPI sets a valid system clock; header displays UK local time | Existing baseline |
| Clock continuity | Leave device running across a minute change | Clock advances locally and updates without a request every second | Existing baseline |
| Offline reboot | Restart demo/offline or live mode with network unavailable | Demo clock is deterministic; live mode clearly indicates unavailable time until sync succeeds | Documented behaviour |
| Wi-Fi positioning | Restart live firmware with Google key and access points available | Scan and successful location result are logged; source and available accuracy metadata are retained in the LocationFix | PR #35 physical regression passed (user-confirmed 2026-11-02) |
| IP fallback | Test with Google lookup unavailable/disabled | IP location is attempted; configured coordinates remain the final fallback | PR #35 physical regression passed (user-confirmed 2026-11-02) |
| Location display | Inspect header and weather requests after location resolution | The selected location name and coordinates are used consistently | PR #35 physical regression passed (user-confirmed 2026-11-02) |

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
