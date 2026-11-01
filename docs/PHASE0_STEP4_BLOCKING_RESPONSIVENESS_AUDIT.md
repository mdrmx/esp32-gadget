# Phase 0, Step 4 — Blocking-operation and responsiveness audit

**Status:** Static audit and initial physical baseline complete; extended fault-path and animation measurements outstanding  
**Roadmap:** [Platform Refactor Roadmap](../PLATFORM_REFACTOR_ROADMAP.md)  
**Repository:** `mdrmx/esp32-gadget`  
**Source file:** `weatherDashboard.ino`  
**Source branch:** `main`  
**Source commit:** `48734f4dc1443e0e830ce34154033d2e2a07f7e2`  
**Source blob:** `02ba63e385a36eb64d542df644126e7efea0b8b4`  
**Audit branch:** `audit/phase0-step4-responsiveness`  
**Review date:** 2026-11-01

---

## 1. Scope and method

This step examines operations in the current firmware that can prevent the Arduino `loop()` from returning promptly to touch polling, clock refresh and other scheduled work.

The audit is based on a complete static inspection of `weatherDashboard.ino` at the source commit recorded above. It identifies:

- Explicit delays and animation loops.
- Wi-Fi association and scanning operations.
- Synchronous HTTP/TLS requests and response-body reads.
- Call paths that can execute those operations during startup, refresh, rendering or touch handling.
- The configured timeout values and their limitations.
- A repeatable physical-device measurement procedure.

This is a **source-level audit supplemented by initial physical measurements** on the target board. The recorded timings are observations from the tested hardware and network environments, not guaranteed bounds or universal performance targets. Configured timeout values are not reported as observed durations or guaranteed end-to-end upper bounds. No firmware behaviour is changed by this audit.

## 2. Executive findings

The firmware is a single sequential application loop. In normal live mode, network requests and several animations execute synchronously on the same call stack as touch handling.

The highest-priority findings are:

1. **Wi-Fi association can hold the caller for up to 20 seconds.** `connectWiFi()` polls every 500 ms and does not return to the main loop during the wait.
2. **HTTPS requests execute synchronously.** The seven request paths have connect and socket/request timeout settings between 5 and 15 seconds. These settings apply to individual connection/read operations; they are not an overall deadline for a complete multi-request workflow.
3. **Startup performs a serial chain of network work.** It can connect to Wi-Fi, scan for access points, attempt Google Wi-Fi geolocation, fall back to IP geolocation, reverse-geocode the result, then fetch current weather and hourly forecast.
4. **A periodic refresh runs weather and forecast requests from `loop()`.** Touch polling is delayed until both functions return.
5. **Weather rendering can initiate an HTTPS icon download.** `drawWeather()` calls the icon renderer, which can download an uncached PNG and wait for its body.
6. **Touch-triggered animations deliberately block.** Several metric, graph and tab animations contain frame loops with 8–9 ms delays. Their configured delay totals are approximately 48–63 ms, excluding drawing and display-transfer time.
7. **Wi-Fi scanning has no application-level deadline in the sketch.** The synchronous `WiFi.scanNetworks(...)` call is used during startup location resolution.
8. **The icon body-read deadline is not a strict total operation deadline.** The loop checks an eight-second elapsed-time condition, but an individual `readBytes()` call may itself wait according to the stream timeout.

The source therefore supports a clear architectural conclusion: **network operations must not run on the input/render call path in the eventual platform design.** It does not yet establish the required task model, queue design or exact latency targets. Those decisions should follow device measurements.

## 3. Synchronous network operation inventory

All request paths below use `WiFiClientSecure` and `HTTPClient` directly. The current call sites use `client.setInsecure()`; TLS certificate validation is a separate security concern already recorded in the roadmap.

| Operation / function | Endpoint purpose | Connect timeout | HTTP/socket timeout | Notes |
|---|---|---:|---:|---|
| `fetchTimeFromAPI()` | TimeAPI.io system time | 5 s | 5 s | Called by time-sync policy; reads response body with `getString()` |
| `loadOpenWeatherIconPNG()` | OpenWeather PNG icon | 10 s | 10 s | Manual body-read loop also checks an 8 s elapsed deadline |
| `resolveLocationNameFromCoordinates()` | OpenWeather reverse geocoding | 8 s | 8 s | Can follow either Google Wi-Fi or IP location |
| `resolveLocationFromWiFi()` | Google Geolocation POST | 10 s | 12 s | Includes a synchronous Wi-Fi scan before the HTTP request |
| `resolveLocationFromIP()` | ipwho.is IP geolocation | 8 s | 8 s | Calls reverse geocoding after a valid response |
| `fetchWeather()` | OpenWeather current conditions | 15 s | 15 s | Called during startup and periodic refresh |
| `fetchOneCallForecastPage()` | OpenWeather hourly timeline | 15 s | 15 s | Called by `fetchForecast()`; up to three pages |

### Timeout interpretation

The values above are the values passed to `HTTPClient::setConnectTimeout()` and `HTTPClient::setTimeout()` in the sketch.

They must not be added together and presented as a guaranteed request duration. A request can involve DNS resolution, TCP/TLS setup, server response, multiple socket reads, body parsing and cleanup. The configured socket timeout is not a single end-to-end transaction deadline. The actual behaviour also depends on the ESP32 core and HTTP/TLS library implementations.

The eight-second icon timer is checked around the manual stream-reading loop. A blocking `readBytes()` call can delay the next elapsed-time check, so the source does not prove a hard eight-second wall-clock maximum for the complete icon download.

## 4. Non-network blocking operations

### 4.1 Wi-Fi association

`connectWiFi()`:

- Returns immediately if Wi-Fi is already connected, subject to a due time-sync attempt.
- Otherwise calls `WiFi.mode(WIFI_STA)` and `WiFi.begin()`.
- Polls `WiFi.status()` until connected or 20,000 ms have elapsed.
- Calls `delay(500)` on each poll.
- Calls the time-sync policy after successful association.

The 20-second value is an application-level association wait limit, not a measurement of how long the Wi-Fi driver takes to associate. The caller remains blocked for the duration.

### 4.2 Wi-Fi scan

`resolveLocationFromWiFi()` calls:

```cpp
WiFi.scanNetworks(false, false, true, 120);
```

This is a synchronous scan call. The final argument requests a 120 ms per-channel dwell according to the API signature used by the sketch, but the sketch does not establish a total scan duration or an application-level timeout. The actual duration depends on the Wi-Fi core/driver and scan environment.

The scan is performed during live-mode startup after Wi-Fi association. It is not performed on every periodic weather refresh.

### 4.3 UI animation delays

The following are explicit frame delays in the source. Totals below are delay time only; rendering and LCD transfer time are additional.

| Function | Frames × delay | Configured delay total |
|---|---:|---:|
| `animateWeatherMetricsSnap()` | 6 × 8 ms | 48 ms |
| `animateWeatherMetricsScroll()` | 7 × 8 ms | 56 ms |
| `animateForecastGraphSnap()` | 6 × 8 ms | 48 ms |
| `animateTabTransition()` | 7 × 9 ms | 63 ms |
| `animateForecastSnap()` | 6 × 8 ms | 48 ms |

`animateForecastScroll()` does not contain an explicit delay loop; it updates the offset and redraws the viewport once.

These animations are initiated from touch/gesture handling. They intentionally trade input polling frequency for visual motion. The measured touch latency during an animation will include both the delay and the time taken to render each frame.

### 4.4 Other explicit delays

| Location | Delay | Purpose |
|---|---:|---|
| `setup()` | 500 ms | Startup settling delay after Serial initialisation |
| `setup()` display failure path | 1,000 ms repeatedly | Deliberate permanent failure loop if display initialisation fails |
| `loop()` | 30 ms each pass | Main-loop pacing |
| Icon stream loop | 1 ms when no data is available | Yield while waiting for PNG bytes |
| `connectWiFi()` | 500 ms per poll | Association wait |

The display-failure loop is an intentional terminal fault state, not normal UI scheduling. The 30 ms loop delay alone does not bound loop latency because synchronous work before it can take much longer.

## 5. Call paths and when the work runs

### 5.1 Startup path

In live mode, `setup()` runs the following sequence synchronously:

1. Display, canvas and touch initialisation.
2. Timezone initialisation.
3. `connectWiFi()`.
4. If connected, `resolveLocationFromWiFi()`:
   - Synchronous Wi-Fi scan.
   - Google Geolocation POST if at least two usable BSSIDs and an API key are available.
   - OpenWeather reverse geocoding after a valid Google result.
5. If Wi-Fi positioning fails, `resolveLocationFromIP()`:
   - ipwho.is request.
   - OpenWeather reverse geocoding after a valid IP result.
6. `fetchWeather()`.
7. `fetchForecast()`, which can request up to three pages.
8. Initial screen rendering.

If Wi-Fi association fails, location resolution is skipped, but `fetchWeather()` and `fetchForecast()` are still called; each checks connectivity through `connectWiFi()`. If association is still unavailable, those functions fail through their existing UI paths.

This startup sequence has no single overall deadline. It is expected to block setup while network operations complete or fail. Since the main loop has not started, touch polling is not yet available during this work.

### 5.2 Main loop and periodic refresh

The live main loop executes:

1. `pollTouch()`.
2. `maybeSyncTimeFromAPI()`.
3. Clock refresh check.
4. If the ten-minute update interval has elapsed:
   - `fetchWeather()`.
   - `fetchForecast()`.
   - Redraw the active screen.
5. `delay(30)`.

A due time sync can perform HTTPS work directly from the loop. A periodic weather refresh performs current-weather and forecast requests serially. During those calls, `pollTouch()` is not called again.

### 5.3 Network work initiated by a render path

The call path is:

```text
drawWeather()
  -> drawOpenWeatherIcon()
      -> loadOpenWeatherIconPNG()
          -> HTTP GET + PNG body read
```

The icon cache avoids a download when the current icon code is already cached. On a cache miss, drawing can therefore perform network I/O and heap allocation. This is an especially undesirable boundary: rendering should consume already-available assets, not fetch them.

### 5.4 Time synchronisation from connectivity helpers

`connectWiFi()` invokes `maybeSyncTimeFromAPI()` both when it finds Wi-Fi already connected and after a successful new connection. The main loop also invokes `maybeSyncTimeFromAPI()` every pass.

The time-sync policy throttles attempts using the recorded retry and sync intervals, so these call sites do not imply a request on every invocation. However, when a sync is due, the HTTPS request runs synchronously on whichever call path triggered it. This means a weather fetch can also incur a time request before its own request begins.

## 6. Responsiveness impact and priority

| Priority | Finding | Why it matters | Direction to investigate |
|---|---|---|---|
| P0 | Network requests execute on the main loop/call stack | Touch and clock work cannot run until the request path returns | Move request execution out of input/render scheduling; deliver completed results through bounded state/events |
| P0 | Weather renderer can download an icon | A redraw can unexpectedly become a network operation | Fetch/cache assets in a data/update path; renderer only draws cached data or a placeholder |
| P1 | Wi-Fi association waits up to 20 s | Connection recovery can freeze interaction | State-driven connection manager with progress/failure state and retry scheduling |
| P1 | Wi-Fi scan is synchronous | Startup location resolution waits for scan completion | Evaluate asynchronous scan mode and explicit scan lifecycle |
| P1 | Multiple forecast pages are fetched serially | A refresh can hold the caller across several requests | Paginate incrementally and make partial results available |
| P2 | Touch-triggered animations use delay loops | Touch sampling is paused during animation frames | Time-based animation updated incrementally from the main loop |
| P2 | Main loop includes a 30 ms delay | Adds a baseline scheduling pause after each pass | Retain or adjust only after measuring frame/input cadence; it is not the main source of long stalls |

These priorities identify architectural risk, not a prescribed implementation. The eventual design should avoid introducing an unbounded request queue, uncontrolled background allocations or concurrent writes to the same weather/forecast state.

## 7. Physical-device measurement plan

The following initial measurements were captured on the Waveshare ESP32-S3 Touch LCD 2 using the temporary Phase 0 diagnostic build. They establish a useful baseline, but do not constitute exhaustive fault-path testing or justify fixed responsiveness targets.

### 7.1 Test conditions to record

For every run, record:

- Firmware commit and branch.
- Board FQBN/options and ESP32 core version.
- Demo/live mode.
- Wi-Fi signal conditions and whether the network is reachable.
- Whether the weather icon is already cached or a new icon is expected.
- Serial log start/end timestamps or added elapsed-time diagnostics.
- Test repetition count and any anomalous result.

Use the same board, power arrangement and Wi-Fi environment for comparative runs.

### 7.2 Test matrix

| Test | Procedure | Measurements to capture |
|---|---|---|
| Idle baseline | Leave the live dashboard on the weather screen with no interaction | Main-loop cadence and ordinary touch response |
| Touch during metric animation | Trigger metric scroll/snap repeatedly while touching the screen again during motion | Animation duration and time until the next touch sample is handled |
| Touch during tab transition | Switch between Weather and Forecast while applying a second deliberate touch | Transition duration and touch response after transition |
| Healthy periodic refresh | Observe a normal refresh on a stable Wi-Fi network | Current-weather duration, each forecast-page duration, total refresh duration, touch gap |
| Failed HTTP request | Temporarily make the API request fail in a controlled test environment, without committing credential changes | Failure duration and touch gap |
| Wi-Fi unavailable at startup | Start with the access point unavailable, then restore it | Association wait, setup completion, recovery behaviour |
| Wi-Fi loss while running | Disconnect the access point during normal use and attempt interaction | Time until UI responds, reconnect behaviour and stale-data presentation |
| Location scan | Capture startup with Wi-Fi positioning enabled | Scan duration, Google request duration and fallback path |
| Icon cache miss | Cause a different valid weather icon to be requested, then observe render | Icon request/body-read duration and touch gap |
| Cached icon redraw | Redraw the same icon after it is cached | Confirm redraw does not initiate another download |

For network failure tests, use a controlled test network or temporary test configuration. Do not publish real credentials or API keys, and restore the normal configuration after testing.

### 7.3 Suggested measurements

At minimum capture:

- **Touch service gap:** maximum elapsed time between successive entries to `pollTouch()` while the UI is expected to remain interactive.
- **Touch-to-response latency:** elapsed time from a deliberate physical touch to the first visible response.
- **Operation duration:** elapsed time around Wi-Fi association, Wi-Fi scan, each HTTP request, icon body read, each animation and the full refresh workflow.
- **Refresh total:** elapsed time from the start of `fetchWeather()` to the return of `fetchForecast()`.
- **Failure recovery:** elapsed time from network loss to a responsive UI and from network restoration to successful data refresh.

Use `millis()` for millisecond-scale durations and `micros()` where a shorter display/input operation needs finer resolution. Keep diagnostic output concise; Serial printing itself can perturb timings, especially when logging every loop iteration. Prefer one summary line per operation rather than continuous per-frame logs.

### 7.4 Measurement record

Measurements below were captured on the Waveshare ESP32-S3 Touch LCD 2 using the temporary Phase 0 diagnostic build. Values are observed run results, not guaranteed upper bounds.

#### Startup

| Scenario | Runs | Reset to usable screen | Notes |
|---|---:|---:|---|
| Fast mesh network with multiple repeaters | 1 reported measurement | 9.6 s | User described the network as very fast; mesh topology and repeater selection make this environment-specific |
| Second, slower network | 1 reported measurement | 10 s ± 0.5 s | Wi-Fi connection appeared almost immediate despite lower network speed |

The near-identical startup times across these two networks suggest that internet throughput and the perceived Wi-Fi association delay are not the dominant determinants of total startup time. This is an inference, not a measured breakdown. The stopwatch measurement covers reset through the screen becoming usable; it does **not** isolate Wi-Fi association, DHCP, time sync, location resolution, weather requests or first render. Stage-level startup instrumentation would be needed to attribute the delay.

#### Main-loop and network operations

| Scenario | Runs | Operation duration | Maximum touch-poll gap / loop duration | Notes |
|---|---:|---:|---:|---|
| Idle baseline | 2 summaries | — | 32–33 ms | 323 loops per 10 s in one summary; 323 in the other |
| Weather refresh | 2 | 718 ms; 726 ms | 820 ms; 829 ms | Wi-Fi already connected (0 ms); icon cache hit (0 ms) |
| Forecast refresh | 1 | 907 ms total | 1,048 ms | Page 1: 846 ms; page 2: 61 ms |
| Full refresh | 1 | 1,369 ms combined | 1,509 ms | Weather: 724 ms; forecast: 645 ms (pages 578 ms + 66 ms) |
| Wi-Fi location lookup | 1 startup log | 3,076 ms | Not separately recorded | Includes synchronous scan and Google positioning request; scan found 12 networks, 6 usable BSSIDs |
| Reverse geocoding | 1 startup log | 389 ms | Not separately recorded | Followed successful Google Wi-Fi positioning |
| Time API request | 1 startup log | 507 ms | Not separately recorded | Captured during startup connection flow |

Loop/touch figures are diagnostic maximum gaps over the reported 10-second summary windows. The diagnostic measures loop-entry/touch-poll opportunity cadence; it is not a direct measurement of physical touch-to-visible-response latency.

#### Observed interaction during network tests

The user continuously touched the screen during the weather, forecast and full-refresh tests and reported **no noticeable interruption to touchscreen interaction**. This is an important qualitative observation, but it does not negate the measured loop gaps. Touch-controller behaviour, the type/timing of gestures and the display's retained image may affect perceived responsiveness. No instrumented touch-to-response latency was captured.

#### Remaining physical tests

| Scenario | Status | Notes |
|---|---|---|
| Metric animation | Pending | Measure animation duration and touch gap |
| Tab transition | Pending | Measure transition duration and touch gap |
| Failed HTTP request | Pending | Controlled failure path not tested |
| Wi-Fi unavailable at startup | Pending | Association timeout / recovery not tested |
| Wi-Fi loss while running | Pending | Runtime reconnection not tested |
| Icon cache miss | Measured once | Forced cache-miss operation: 564 ms; maximum loop duration and reported touch-poll gap were both 667 ms |
| Cached icon redraw | Observed | Cache check reported 0 ms during weather and forecast tests; full redraw behaviour was not separately timed |

The healthy-operation baseline is sufficient to document current behaviour and proceed with Phase 0 close-out. Fault-path and animation tests remain follow-up validation rather than being treated as complete. The memory and cache-allocation observations are recorded separately in [the Phase 0 memory profile](PHASE0_MEMORY_PROFILE.md).

## 8. Recommended boundaries before Phase 1

The audit supports the following responsibility boundaries. It does not yet prescribe exact classes, tasks or filenames.

1. **Connectivity lifecycle:** connection state, scan state, association attempts, retries and network selection should advance through bounded state transitions rather than long waits in an application call.
2. **Request execution:** HTTP/TLS work should be isolated from touch polling and rendering. Results should be delivered as completed data or explicit errors.
3. **Weather and forecast update orchestration:** current conditions and paginated forecast requests should be scheduled independently, allowing partial progress and clear stale-data policy.
4. **Asset loading/cache:** icon download and PNG cache replacement should occur outside drawing functions. Rendering should be safe when the asset is missing.
5. **Time service:** time synchronisation should be scheduled independently of `connectWiFi()` and should not unexpectedly run inside a weather fetch call.
6. **Animation scheduling:** animation state should advance based on elapsed time rather than delay loops, allowing touch sampling to continue between frames.
7. **Input scheduling:** preserve frequent CST816 sampling and measure the maximum service gap under all operations before selecting a target cadence.

Potential implementation approaches include a cooperative state machine, a bounded request worker or an ESP32 task. The correct choice depends on measured latency, library thread-safety, memory headroom and ownership of shared Arduino/graphics state. Do not start parallelising HTTP calls or sharing mutable weather objects across tasks without an explicit ownership design.

## 9. Acceptance status

### Static audit

- [x] Inspect all explicit delay loops and their call paths.
- [x] Inventory HTTP/TLS request sites and configured timeout values.
- [x] Identify synchronous Wi-Fi association and scanning.
- [x] Trace startup, periodic refresh, time-sync and render-triggered network paths.
- [x] Separate configured timeout values from measured durations.
- [x] Define a repeatable physical measurement matrix.
- [x] Record candidate responsibility boundaries and priorities.

### Physical verification

- [x] Record initial idle loop/touch-poll baseline.
- [ ] Measure animation and tab-transition touch gaps.
- [x] Measure healthy weather, forecast and full-refresh durations.
- [ ] Measure failed HTTP request duration and touch gap.
- [ ] Isolate Wi-Fi association and scan durations; startup stopwatch result is end-to-end only.
- [x] Measure full startup and healthy periodic refresh durations.
- [x] Record qualitative touchscreen interaction during healthy network operations.
- [x] Measure one forced icon cache miss (564 ms); repeated misses and failed/short downloads remain untested.
- [x] Record initial results and identify limits of the measurements.

**Step 4 static audit and initial healthy-operation baseline are complete.** One forced icon cache miss has now also been measured. Extended fault-path, animation and instrumented touch-to-response measurements remain follow-up validation. The recorded healthy-operation timings are observations from the tested setup, not universal targets.

## 10. Follow-up hardware measurements — 2026-11-01

The temporary diagnostics revision was subsequently uploaded and exercised on the physical Waveshare ESP32-S3 Touch LCD 2. The tested revision was PR #23 head `59f2011f83b1ac8ce57d51d0f2760d84ecf4f6cd`. The following are additional observations, not changes to the original baseline runs.

### 10.1 Additional request timings

| Scenario | Operation duration | Diagnostic summary | Notes |
|---|---:|---:|---|
| Manual weather | 462 ms | Not included in the supplied 10-second summary | Icon cache hit: 0 ms |
| Manual forecast | 742 ms | 845 ms maximum loop and touch-poll gap; 297 loops in the summary | Page 1: 645 ms; page 2: 95 ms; cache hit: 0 ms |
| Forced icon cache miss | 564 ms | 667 ms maximum loop and reported touch-poll gap; 302 loops | Temporary internal-heap pressure recovered after the operation |
| Manual full refresh | Weather 448 ms; forecast 655 ms; approximately 1,103 ms combined request time | 1,207 ms maximum loop; 31 ms maximum touch-poll gap | The two maxima are not aligned reliably; see the diagnostic caveat below |

The full-refresh forecast pages took 558 ms and 95 ms. Rendering returned internal free heap to within 48 bytes of the pre-refresh value.

### 10.2 Touch-poll summary caveat

The full-refresh result of 1,207 ms maximum loop alongside 31 ms maximum touch-poll gap is a diagnostic reporting-window defect, not evidence that the touch input remained responsive during that request.

In PR #23, the touch gap is sampled at the next loop start, whereas the loop duration is recorded and the 10-second summary may be emitted/reset at the end of the current loop. A long iteration can therefore be included in the loop maximum before its corresponding touch-poll gap is recorded in the next reporting window.

Do not use the 31 ms figure as the touch gap for that full-refresh operation. Correct the instrumentation's sample/report alignment before using it for further acceptance measurements. Physical touch-to-visible-response latency remains unmeasured.

### 10.3 Memory profile

The full on-device heap/PSRAM record, including canvas placement, startup headroom, weather/forecast refresh, icon cache miss and final post-refresh snapshot, is in [PHASE0_MEMORY_PROFILE.md](PHASE0_MEMORY_PROFILE.md).

## 11. Change history

- **2026-11-01 — Version 1.0:** Initial static blocking-operation and responsiveness audit against `main` commit `48734f4`. Records synchronous network paths, Wi-Fi waits/scanning, animation delays, render-triggered icon downloads, a physical measurement plan and provisional architecture priorities. No firmware code changed.
- **2026-11-01 — Version 1.1:** Added initial target-device measurements: reset-to-usable startup of 9.6 s on a fast mesh and approximately 10 s ± 0.5 s on a slower network; idle loop gaps of 32–33 ms; weather, forecast and full-refresh timings; Wi-Fi location and time-sync observations; and the user's report of no noticeable touchscreen interruption during network tests. Clarified that startup timing is end-to-end and that maximum loop gaps are not direct touch-to-response measurements. Extended fault-path, animation and icon-cache-miss tests remain outstanding.

- **2026-11-01 — Version 1.2:** Added follow-up weather, forecast, forced icon-cache-miss and combined-refresh timings from the updated temporary diagnostic revision. Recorded the memory profile separately and identified a diagnostic reporting-window mismatch that makes the full-refresh touch-poll figure unreliable. Fault-path and animation tests remain follow-up work.
