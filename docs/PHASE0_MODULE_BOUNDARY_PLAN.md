# Phase 0 — Data ownership and Phase 1 module-boundary plan

**Status:** Proposed boundaries based on the Phase 0 source audit and hardware baseline  
**Source inspected:** `main` at PR #35 merge commit `fbd87e9b73b4a21e4b2579a7e2d0c15708e410cb`  
**Primary source file:** `weatherDashboard.ino` (2,994 lines at the PR #35 merge checkpoint; the Step 1 baseline inventory recorded 155 file-scope declarations)  
**Intent:** define ownership and a behaviour-preserving extraction sequence. This document does not claim that the proposed modules already exist.

## 1. Design constraints from the audit

The first refactor must preserve the working Weather app and fit the confirmed Arduino IDE / ESP32 toolchain.

The evidence points to several hard constraints:

1. The sketch currently has one global namespace, one `setup()`, one `loop()` and one sequential execution path.
2. The 155 inventoried file-scope declarations mix device configuration, hardware objects, location/time state, weather data, input ownership, presentation state and assets.
3. Touch/gesture state alone accounts for 50 declarations. Metric, forecast-list and graph interactions have distinct arbitration and suppression behaviour.
4. Weather services and rendering are coupled: fetch functions write global state and invoke UI feedback, while the Weather renderer can initiate an icon download.
5. Wi-Fi lifecycle, request clients, location and time services are interleaved. HTTPS clients are created by individual functions and currently use `setInsecure()`.
6. Three app-specific canvases request 292,652 bytes and were all verified in PSRAM on the recorded board configuration. The settled startup internal heap was 213,608 bytes free with a largest block of 118,772 bytes; the since-boot low-water mark was 110,532 bytes.
7. The repository has no checked-in automated test project or explicit dependency/build manifest. The verified Arduino IDE workflow and root sketch naming rule must be preserved.
8. The current display, graph and touch behaviour is a hardware-tested product baseline, not disposable prototype code.

The implementation should use explicit ownership and small interfaces. Do not introduce a generic framework, parallel request execution or a new display stack as part of the first extraction.

## 2. Current state ownership

The table groups current global state by who should own it after extraction. Names are representative; use the Step 1 inventory for the complete source-level list.

| Current state / data | Current location and mutation | Intended owner | Boundary rule |
|---|---|---|---|
| Wi-Fi SSID/password and API key pointers | Compile-time macros in local `secrets.h`, surfaced as globals | Device configuration / credential adapter | Secrets remain local and excluded from Git; apps never read credential macros directly |
| `locationLat`, `locationLon`, `locationDisplayName`, `locationCoordinatesResolved` | Written by Google Wi-Fi, IP and reverse-geocode paths; read by weather and forecast requests and header | Location service / location-fix value | Expose a fix with coordinates, source, accuracy and timestamp; consumers do not choose providers |
| `TIME_ZONE_INFO`, time validity and sync timestamps | Time helper functions and `maybeSyncTimeFromAPI()` | Time service | Own system-clock sync policy and UK civil-time conversion; do not own header drawing |
| `locationTimezoneOffset` | Updated from forecast response and used for forecast labels | Weather/forecast data context | Keep provider-local forecast offset distinct from the device's Europe/London system timezone |
| Weather current-condition strings, metrics and availability flags | Written by `fetchWeather()`; forecast page may update UV/rain values; read by Weather renderers | Weather model / repository | Data layer returns a result/model; it does not call drawing functions |
| `ForecastItem forecast[48]`, count and error | Filled by paginated forecast parser; read by list and graph renderers | Forecast model owned by Weather app/service | Preserve capacity and record ordering first; review dynamic `String` allocation only against measured memory |
| `currentTab`, list/graph mode, metric/list/graph offsets | Changed by gesture handlers and screen functions | Weather view state | Keep screen/view navigation out of the shared device scheduler |
| 50 touch/gesture/drag/suppression declarations | Read by `pollTouch()` and mutated by `updateTouchDrag()` / `handleGesture()` | Input reader + Weather interaction controllers | Separate raw sampling, gesture recognition, gesture ownership and view actions; do not collapse all state into one generic gesture object initially |
| `gfx`, SPI bus, dimensions and three canvas pointers | Global hardware objects; canvases allocated/freed in `setup()`; tab transition temporarily swaps global `gfx` | Display manager / render-target owner | Own initialisation, dimensions, buffer lifetime and fallback; render target must not be changed through unscoped global mutation |
| PNG decoder, icon buffer pointer/length and cached icon code | Download/cache function and drawing path | Weather asset cache + renderer | Download/cache replacement must not occur inside a renderer; keep decode and display operations on the display-owning execution path |
| Weather refresh / clock cadence | `WeatherApp` lifecycle shell via injected millis callback | Weather application shell | Own periodic update timing; keep synchronous request sequencing and no overlap until explicit concurrency design |
| UI colours and layout constants | Global constants used by multiple Weather renderers | Theme / Weather view configuration | Keep app-specific layout values in Weather; move only genuinely shared visual tokens into a small shared theme |
| Persistent settings | No Preferences/NVS store exists in current sketch | Future Settings service (Phase 2) | Do not create a partial settings layer during Phase 1; avoid a premature storage schema |

## 3. Proposed responsibility boundaries

These are logical boundaries first. Exact C++ classes and filenames should be chosen only when the first extraction is compiled with the verified Arduino toolchain.

### 3.1 Device composition and runtime

**Owns:** board-level startup order, long-lived platform objects, the main update cadence and active application dispatch in the future.

**Does not own:** weather parsing, location-provider policy, gesture state or individual screen rendering.

For Phase 1, keep `setup()` and `loop()` as the composition root. Do not introduce the Application Manager yet; that belongs to Phase 4 once Weather is an independently bounded application.

### 3.2 Display and render-target management

**Owns:**
- SPI bus and ST7789 initialisation.
- Logical display dimensions and rotation assumptions.
- Canvas allocation, actual memory-region checks, fallback and destruction.
- A stable render-target API for full display and off-screen buffers.

**Does not own:** Weather data, network requests, gesture interpretation or app navigation.

The tab-transition routine currently swaps the global `gfx` pointer while composing output. Replace this with an explicit target or scoped render context when extracting the display boundary. Avoid exposing a mutable global graphics pointer to every module.

The existing canvases are all in PSRAM on the measured board, but they remain Weather-specific buffers today. Do not automatically allocate an equivalent set for each future app. Decide whether buffers are shared, reused on app entry, or allocated per active view only after measuring lifecycle and transition requirements.

### 3.3 Touch input and interaction

Split the existing responsibility into four layers:

1. **CST816 reader:** owns I²C register access and produces a raw `TouchSample`.
2. **Gesture recogniser:** converts samples into typed controller gesture observations and generic contact lifecycle events. PR #31 adds `src/input/GestureRecognizer.h` to track raw-register transitions and touch-present/start/release edges without deciding which view owns the touch or classifying a view-specific drag. Controller-specific numeric codes stay inside this layer.
3. **Interaction ownership/arbitration:** `src/input/InteractionArbiter.h` ([PR #32](https://github.com/mdrmx/esp32-gadget/pull/32)) tracks a single active contact owner and exposes whether a controller gesture must be suppressed to avoid duplicating a coordinate-tracked interaction. Weather still performs the hit-test and selects the owner. Metric/list candidates claim ownership only once vertical drag intent activates; horizontal intent releases a candidate without claiming. Graph and forecast-control contacts retain their existing immediate ownership. Contact release clears ownership. Release grace periods and graph quiet-period timing remain Weather policy.
4. **Scroll calculations:** `src/ui/ScrollController.h` centralises stateless drag-delta application, elastic overscroll resistance, elastic-bound clamping and nearest-step snapping. The caller supplies the maximum position, overscroll cap, resistance divisor and step size. It owns no per-view offset, active drag, release timer, animation or rendering state. Metric cards, forecast list and graph retain separate offsets and lifecycles; graph hold/quick-swipe/point-selection policy remains in Weather.

The Weather app remains responsible for hit regions and the actions associated with its metric/list/graph screens. The graph's hold/drag and selection behaviour is not identical to list scrolling; preserve a graph-specific controller or policy rather than forcing it into the same generic algorithm.

### 3.4 Connectivity and request execution

Keep these as separate responsibilities:

- **Connectivity manager:** network interface availability, Wi-Fi lifecycle, selected transport and reconnect policy. A Wi-Fi backend is sufficient in Phase 1/2; the interface should not encode Wi-Fi-only assumptions.
- **Request executor / HTTP client support:** HTTP/TLS client creation, timeout policy, response status and transport errors. Requests should return data/errors and must not draw loading or error screens.
- **Domain services:** Time, Location and Weather decide which endpoint to call and how to interpret its response; they do not start or stop a network interface.

The initial extraction remains synchronous and sequential. The current evidence demonstrates blocking but does not justify creating FreeRTOS tasks or concurrent requests yet. A later non-blocking design must define bounded request ownership, cancellation, response lifetime, memory limits and how results reach the main UI loop.

**PR #36 — shared HTTP request transport (merged):** `src/services/HttpRequestClient.h` owns one synchronous HTTP request session, including `WiFiClientSecure` / `HTTPClient` lifetime, per-request connect and response timeouts, GET/POST operations, headers and response access. TimeService and LocationService use it for their requests; endpoint construction, status/error interpretation, JSON parsing and domain policy remain with those services. Each caller explicitly selects the existing insecure-TLS compatibility flag. PR #36 merged at `2cf68e08ddf7b569ea57777ca12147069f506b26`. The final parser-follow-up Arduino IDE Verify passed on the recorded Waveshare ESP32-S3 FQBN and ESP32 core 3.3.12: 1,151,215 bytes program storage, 97,228 bytes global dynamic memory and 230,452 bytes remaining. The user confirmed Google Wi-Fi positioning, correct clock display, all tested interactions and successful current-weather/hourly-forecast data loading. Weather icon/current/forecast calls remained direct and are the focus of PR #37.

Certificate validation is a separate security decision. Do not carry `setInsecure()` forward as an unexamined platform default.

### 3.5 Location service

**Owns:** Wi-Fi positioning, IP fallback, reverse-geocoding coordination and the resulting location value.

A future location-fix value should contain at least:

- Latitude and longitude.
- Provider/source (Google Wi-Fi, IP geolocation, configured fallback, future GNSS).
- Accuracy radius when the provider supplies one.
- Observation/update timestamp.
- A validity indicator.

IP geolocation must remain marked as coarse; it must not be represented as equivalent to a Wi-Fi or future GNSS fix. Weather and Maps consume the same location value and do not call provider endpoints directly.

**PR #35 — LocationService first slice (merged after physical regression):** `src/services/LocationService.h` owns the existing Google Wi-Fi request, public-IP fallback and OpenWeather reverse-geocoding coordination. It returns a `LocationFix` containing latitude, longitude, source (`GoogleWifi`, `IpGeolocation`, `ConfiguredFallback`; `Gnss` is reserved), optional accuracy radius, monotonic observation time in milliseconds since boot and a coordinate-validity flag. Only Google's supplied radius is reported as accuracy; IP geolocation remains coarse and has no claimed radius. The existing provider order remains Google Wi-Fi → IP → configured coordinates.

Weather supplies the current Wi-Fi-connected state, API keys and configured fallback coordinates. The service reuses the existing Wi-Fi connection and does not manage association. Weather copies the returned fix into the existing coordinate/display state so weather requests, header naming, the OpenWeather reporting-station fallback and forecast timezone handling retain their existing behaviour.

### 3.6 Time service

**Owns:** system clock validity, TimeAPI request/sync policy, retry/backoff scheduling and UK timezone configuration.

Keep two concepts distinct:
- Device civil time in `Europe/London`, used by the header clock.
- Forecast/provider timezone offset, used to format provider-local hourly timestamps.

The current implementation stores both concepts in related global state. The extraction must not merge them into one offset or change the displayed forecast times.

**PR #34 — TimeService (merged):** `src/services/TimeService.h` owns the Europe/London timezone/DST configuration, minimum valid system epoch, device-clock validity check, header clock value, TimeAPI request/JSON validation/epoch conversion/system-clock update, and the existing five-minute retry / six-hour resynchronisation policy. Weather supplies the current network-connected state and scheduler time; TimeService does not start or stop Wi-Fi. Header redraw cadence/cache remain Weather presentation state. `locationTimezoneOffset` remains separate in Weather forecast data. PR #36 now routes the TimeAPI request through the shared synchronous `HttpRequestClient`; TimeService retains endpoint construction, response parsing, clock-update policy and retry scheduling. On non-200 HTTP status the service ends the request directly without buffering the discarded error body. PR #34 merged at `b11416b62ff91b7f917f16c8a484d0d2e6b7f02d`; the user confirmed the physical test passed on 2026-11-02, while Arduino IDE Verify remains unconfirmed.

### 3.7 Weather data service and model

**Owns:** current-weather and One Call page requests via `HttpRequestClient`, JSON parsing, sequential forecast pagination, bounded 48-record forecast storage, data validity/error state and the offline demo-data fixture.

**Does not own:** Wi-Fi association/connection lifecycle, LocationService/provider selection, device location coordinates, icon PNG fetching/cache ownership, display calls, Weather screen state or loading/error drawing. The caller supplies API key and selected coordinates; the service does not obtain location or connect Wi-Fi.

**PR #38 — WeatherDataService (merged):** `src/services/WeatherDataService.h` owns current-weather and One Call request/parsing, current/forecast models, forecast pagination, validity/error state and offline demo data. The sketch retains Wi-Fi lifecycle, LocationService/provider selection, location coordinates, icon fetching/cache, loading/error UI, Weather presentation, rendering and input. Temporary sketch-facing references into service-owned models preserve the existing renderers. Merged at `716670c118cec37488cd895117d533a69c31d407` (2026-11-12); the user confirms hardware regression passed. Under the project convention, Verify and upload/boot are treated as passed for merged PRs. PR #40 later moves the persistent view-state fields to a separate app-owned value object.

Initially preserve:
- The existing current-weather fields, update semantics and availability flags.
- The fixed 48-record forecast capacity and duplicate timestamp filtering.
- Partial forecast behaviour if a later page fails after earlier data has been received.
- One Call's provider timezone offset as separate state from the Europe/London device clock.
- The existing current-weather-before-forecast request order and refresh cadence.
- Offline demo-data behaviour, including 24 forecast entries without network access.

The Weather app remains responsible for `connectWiFi()`, location coordinate ownership, location-provider selection, icon fetching/cache, loading/error drawing and presentation. Do not introduce concurrent current/forecast jobs in this extraction; they share mutable location/time/UV/rain context and currently execute sequentially.

### 3.8 Weather application and views

**Owns:** Weather screen state, active Weather view, presentation layout, metric/list/graph rendering, and the mapping from semantic input to Weather actions. `WeatherViewState` is the persistent state boundary; renderer and interaction policies remain separate owners.

**PR #39 — WeatherApp shell (merged and hardware-validated):** `src/apps/weather/WeatherApp.h` owns begin/update/end lifecycle, startup current-weather-then-forecast coordination, initial render selection, input/time/clock callback orchestration and the existing ten-minute refresh cadence. The sketch supplies fixed function-pointer adapters to its current services and renderers; the shell is independent of Arduino_GFX, Wi-Fi, HTTP and concrete Weather data models. It preserves the existing rendering and platform boundaries while moving application runtime out of the device `loop()`. C++11 host lifecycle checks and user-confirmed Waveshare physical validation passed. PR #39 merged at `3c613363f3b57bebe4245350cc57a3d6aa7a52d6`.

**PR #40 — WeatherViewState (merged and hardware-validated):** `src/apps/weather/WeatherViewState.h` owns the persistent active-tab index, metric offset, forecast-list offset, forecast-graph hour/pixel offsets and List/Graph mode. The sketch retains temporary references under the former global names. `resetScrollablePositions()` preserves the selected tab and forecast mode while returning canonical offsets to zero; `selectForecastMode()` resets both forecast scroll representations before changing mode. The module is plain C++11 with no Arduino, display, input or network dependencies. Host tests pass and the user confirmed physical Waveshare regression before merge; by project convention Verify and upload/boot are recorded as passed. Merged at `266ef646022353dd57bb552fc17a7ba523ad8708`.

**PR #41 — TextRenderer (merged):** `src/ui/TextRenderer.h` owns shared direct/centred text drawing, width/height measurement, bounded two-line wrapping and the centred Weather-line helper. It retains a reference to `DisplayManager` and resolves the active target for each operation, preserving tab-transition composition. The final merged revision includes null-target guards for all public methods and the corrected left clamp for over-wide centred text. Strict C++11 host tests passed. PR #41 merged at `108805abfeb290e0b87b497960acff8e85bc7a3d`; by project convention the merged revision is treated as having passed Arduino Verify, upload/boot and Waveshare regression. The published build figures (1,164,219 bytes program storage; 97,380 bytes global dynamic memory; 230,300 bytes remaining) apply only to the pre-review revision and are not carried forward to the final merge.

**PR #42 — HeaderRenderer (merged at `ba92b8b53da8457caa18bb454f53c0d51ca6687a`):** `src/ui/HeaderRenderer.h` owns the fixed header background, location/clock placement, horizontal divider and clock-only redraw region. Weather continues to resolve location text, read `TimeService`, manage the last-clock cache and supply theme colours. The renderer uses `TextRenderer` and `DisplayManager::target()`; it owns no Weather/time state, display resources, canvas lifecycle or networking. Focused C++11 host coverage has been added for full-header geometry, clock-only repaint, wide clock clamping, null-target handling and transition-canvas routing; equivalent local strict-warning checks passed. Arduino IDE compilation and upload/flash passed on the current PR head using ESP32 core 3.3.12 and the recorded 16 MB / OPI PSRAM configuration: 1,164,667 bytes program storage (37% of 3 MiB), 97,396 bytes global dynamic memory (29% of 327,680 bytes) and 230,284 bytes remaining. The merged 16 MB image was generated, flash hashes verified and the board hard-reset. The user confirmed successful application boot and full Waveshare physical regression on 2026-11-13; header, clock refresh, transition composition, Weather/Forecast, loading/error and existing interaction behaviour remain preserved. The user ran both repository host scripts after the shared graphics stub update; `test-header-renderer.sh` reported `HeaderRenderer tests passed.` and `test-text-renderer.sh` reported `TextRenderer tests passed.`

**Next extraction after PR #42:** inspect the remaining drawing helpers and choose one self-contained view-specific block with explicit display-target, data, colour and canvas dependencies (for example, an isolated loading/error or Weather/Forecast section). Keep metric, list and graph render paths separate where their behaviour differs. Do not combine renderer movement with gesture-policy or asset-cache/network changes.

The Weather app consumes platform Display, Input, Location and Time capabilities plus Weather data results. It must not configure Wi-Fi, call HTTP clients or fetch assets from inside drawing functions.

Use small rendering helpers and data-driven components where they reduce actual duplication. Keep graph-specific drawing and interaction separate where the behaviour is materially different.

### 3.9 Asset cache

**Owns:** weather icon identity, download lifecycle, byte-buffer ownership, replacement/freeing and cache status.

The renderer may request an already-available asset or draw a safe fallback. It must not synchronously perform an HTTPS request as a side effect of rendering. Decode/display operations must execute on the display-owning path unless the PNG library is explicitly shown to be safe elsewhere.

## 4. Genuine reuse versus lookalike code

| Area | Audit conclusion | Phase 1–3 treatment |
|---|---|---|
| Metric and forecast-list scrolling | Both use coordinate drags, bounds, overscroll and release snapping, but have distinct hit regions and state | Extract common scroll mathematics only after preserving separate view interaction state |
| Forecast graph interaction | Has hold timing, point selection, horizontal graph translation, quick-swipe handling and suppression | Keep graph-specific policy; share only lower-level helpers where proven |
| Text and layout helpers | Shared drawing utilities are genuinely reused | Move only after renderer/display target ownership is stable |
| Current and forecast API calls | Both build URL, use HTTPS, parse JSON and report errors, but return different domain data and pagination semantics | Share transport/request execution, not domain parsing |
| Wi-Fi positioning and IP geolocation | Both resolve a location, but use different data and confidence | Share a LocationFix result model and provider orchestration, not provider request logic |
| Header clock and forecast time labels | Both format time, but one uses device UK time and the other provider-local forecast offset | Share low-level formatting utilities only; keep timezone semantics separate |
| Header, tabs and transition rendering | Related visuals, but tab transition uses a separate off-screen composition path | Extract common theme/layout tokens first; do not force one generic screen renderer |

## 5. Proposed source organisation

Keep `weatherDashboard.ino` at the sketch root and retain the `weatherDashboard` local sketch-folder name required by the recorded Arduino workflow. A possible initial organisation is:

```text
weatherDashboard/
├── weatherDashboard.ino          # composition root: setup() / loop()
├── secrets.h                      # local only, ignored
├── secrets.example.h
├── src/
│   ├── core/
│   │   ├── DeviceConfig.h
│   │   └── AppRuntime.h            # only if a real composition need exists
│   ├── hardware/
│   │   ├── DisplayManager.h
│   │   └── Cst816Reader.h
│   ├── input/
│   │   ├── TouchSample.h
│   │   ├── GestureRecognizer.h
│   │   └── ScrollController.h
│   ├── services/
│   │   ├── ConnectivityManager.h
│   │   ├── HttpRequestClient.h
│   │   ├── LocationService.h
│   │   └── TimeService.h
│   └── apps/
│       └── weather/
│           ├── WeatherModels.h
│           ├── WeatherService.h
│           ├── WeatherAssetCache.h
│           ├── WeatherApp.h
│           ├── WeatherViewState.h
│           └── WeatherViews.h
└── docs/
```

This is a candidate, not a mandate to create all files at once. Validate Arduino IDE handling of the proposed `src/` layout with a tiny compile-only spike before moving production functions. Do not add empty modules or reorganise the whole sketch in one PR.

## 6. Recommended extraction sequence

Each item should be its own reviewable PR and keep runtime behaviour unchanged unless a specific change is separately agreed.

1. **Build/layout spike and value types.** The Arduino `src/` compile/link spike has passed. The first real value-type extraction, [PR #27](https://github.com/mdrmx/esp32-gadget/pull/27), moved the data-only `TouchSample` into `src/input/TouchSample.h`. It passed compilation, upload and physical input regression on the Waveshare board and is merged into `main`.
2. **Display ownership.** Encapsulate ST7789 initialisation, logical dimensions, canvas allocation/lifetime/fallback and render-target selection. [PR #28](https://github.com/mdrmx/esp32-gadget/pull/28) implements this boundary in `src/hardware/DisplayManager.h`; Weather drawing and interaction remain in the sketch. The initial extraction and subsequent resource-lifecycle review fixes have both passed compilation, upload and physical display/input regression on the Waveshare board. Exact post-change heap figures were not supplied, so the numeric comparison with the Phase 0 memory profile remains unrecorded.
3. **Raw touch reader.** [PR #29](https://github.com/mdrmx/esp32-gadget/pull/29) moves CST816 I2C setup, presence probing and six-byte raw sample decoding into `src/hardware/Cst816Reader.h`. The Weather polling loop delegates to the reader; coordinate conversion, gesture codes, interaction state and suppression remain in the sketch. PR #29 is merged at `5ee7ce7d1a42e31d275cc7fa2e2899544d6d9a42`. After the review fixes, the user confirmed successful compilation, upload and physical input regression on the Waveshare board. Review follow-ups add an optional `begin(bool initBus = true)` bus-initialisation switch and use `size_t` for the `requestFrom()` result.
4. **Gesture/interaction separation.** Extract gesture recognition and ownership incrementally. Retain the distinct metric, forecast-list and graph interaction states; test after each movement. **PR #30 — typed gesture codes (merged):** adds `src/input/GestureCode.h` with a pure raw-byte decoder and replaces the sketch's integer gesture constants and handled-gesture state with `GestureCode`. The raw CST816 byte is retained for transition logs; values `0x00`–`0x04` retain their existing meaning and other values map to `Unknown`. The user reports that the latest code was run and tested and all gestures remain intact.

   **PR #31 — controller-level gesture recognition (merged):** adds `src/input/GestureRecognizer.h`. It converts a `TouchSample` into a `GestureObservation` containing the raw value, typed gesture, raw-register transition information and touch-present/start/release edges. It replaces the sketch's `previousRawGesture` and `touchSampleWasDown` tracking and supplies the observation to `updateTouchDrag()`. Arduino IDE Verify passed with 1,149,427 bytes program storage and 97,212 bytes global dynamic memory; the user confirmed upload and physical regression. The review cleanup was committed, replied to and resolved.

   **PR #32 — interaction ownership/arbitration (merged):** adds `src/input/InteractionArbiter.h` with one enum-backed active owner: metric panel, forecast list, graph or forecast control. The sketch chooses the owner using its existing hit regions and intent thresholds. Metric/list candidates remain unclaimed until vertical drag intent activates; horizontal candidates remain available to tab navigation. Graph/control contacts claim immediately, matching the previous suppression conditions. The arbiter replaces the active-interaction OR list in `pollTouch()`; Weather retains release grace timing and the graph quiet-period suppression latch. No dynamic allocation is introduced. Arduino Verify passed with 1,149,179 bytes program storage and 97,212 bytes global dynamic memory; the user confirmed upload/boot and physical regression. Merged at `343eb7ed0120d947990cab48cec0c688177f4ec5`.
5. **Scroll mechanics (PR #33 merged):** `src/ui/ScrollController.h` extracts only stateless drag-delta, elastic-resistance, elastic-bound and nearest-step snap calculations. Weather supplies its existing 20 px overscroll cap, resistance divisor 3, maximum positions and per-view step sizes. Per-view offsets, drag lifecycles, release timers, animation and rendering remain separate. Graph hold timing, quick-swipe classification, point selection and horizontal paging remain graph-specific Weather policy. Compile-time assertions, a standalone C++14 header compile, Arduino IDE Verify, upload/boot and physical metric-card, forecast-list and graph scroll regression passed; the user confirmed the firmware and interactions on 2026-11-02. Merged at `4f3aeae4713b8c6252266112e476a9e50cb86c52`. Do not turn this into a generic UI scrolling framework.
6. **Time and location boundaries:** PR #34 extracts `TimeService` and is merged. PR #35 extracts `LocationService` and is merged, preserving Google Wi-Fi → IP → configured-coordinate priority and returning a source/accuracy/observation-time/validity-bearing fix. The user confirmed the physical regression passed on 2026-11-02.
7. **Network request support and Weather data.** **PR #36 — shared HTTP transport (merged)** routes TimeService and LocationService through `HttpRequestClient`. **PR #37 — remaining Weather HTTP calls (merged at `65b3019ef6ae5a89ba25591430532bf646e3e51e`)** routes icon/current/One Call requests through the shared transport; Verify, upload and full physical regression passed. **PR #38 — WeatherDataService (merged at `716670c118cec37488cd895117d533a69c31d407`)** moves current-weather/forecast request parsing, pagination, error/validity state and bounded models into `src/services/WeatherDataService.h`. The sketch retains Wi-Fi lifecycle, location selection, coordinates, icons/cache, rendering and interaction. A compatibility facade keeps renderers stable. Review fixes include a bounded 240-byte error preview, unused alias removal, first-valid-hour UV/rain semantics and service ownership of forecast-unavailable state. The user confirms PR #38 hardware regression passed; Verify and upload/boot are treated as passed under the project convention. Keep requests synchronous and sequential; preserve current-before-forecast order, provider-local timezone, partial-data behaviour and 48-record cap.
8. **Weather application/views and asset cache.** PR #39 starts the WeatherApp lifecycle shell; PR #40 extracts persistent view state. PR #41 establishes the first shared text-rendering primitive, with target routing delegated to DisplayManager. Follow with incremental ownership of individual view renderers; keep asset-cache ownership and the removal of any network side effects from drawing as separate, testable changes.
9. **Application Manager.** Defer until Phase 4. Phase 1 should leave Weather as the only application.

The exact ordering of steps 2–8 may be refined when the first compile spike reveals the least disruptive Arduino source layout. Do not begin Phase 2 cellular or Phase 4 app lifecycle work early.

## 7. Decisions and non-goals

The audit does not establish enough evidence to decide the following yet:

- FreeRTOS task versus cooperative state machine for future asynchronous requests.
- Cellular modem SKU, AT versus PPP mode, power arrangement or GNSS integration.
- Final persistent Wi-Fi profile schema or settings UI.
- Shared versus per-app canvas reuse policy after multiple applications exist.
- TLS certificate trust store and certificate-validation approach.
- Whether every scroll path can use one implementation.

Non-goals for the first refactor:
- No UI redesign or feature additions.
- No simultaneous weather and forecast fetching.
- No broad gesture rewrite.
- No new general-purpose UI framework.
- No unconditional large buffer allocation for future apps.
- No commitment to a cellular transport implementation before hardware is selected.

## 8. Phase 1 acceptance and ongoing checks

Phase 1 has started. The agreed ownership plan and regression checklist are the working baseline for each extraction PR.

- [x] The Arduino `src/` layout compile/link spike passed under the recorded IDE and FQBN; see [the compile spike record](PHASE0_ARDUINO_SRC_LAYOUT_SPIKE.md).
- [x] The Step 4 diagnostic touch-gap reporting caveat is retained in the permanent audit and memory profile. The 31 ms full-refresh result is not treated as an acceptance measurement.
- [x] PR #27 — `TouchSample` value-type extraction compiled, uploaded and passed physical input regression before merge.
- [x] PR #28 display ownership extraction and resource-lifecycle review fixes compiled, uploaded and passed the reported physical display/input regression.
- [ ] Record the post-change canvas memory-region and internal-heap figures for PR #28 against the Phase 0 memory profile when those diagnostic readings are available.
- [x] PR #29 — CST816 reader extraction compiled, uploaded and passed the user's full physical input regression after the review fixes. Gesture mapping, raw-to-screen conversion, drag behaviour and suppression remain unchanged.
- [x] PR #30 — typed `GestureCode` decoder merged; the user reports the latest code was run and tested and all gestures remain intact.
- [x] PR #31 — Arduino IDE Verify passed on 2026-11-02 with the recorded ESP32-S3 FQBN and ESP32 core 3.3.12. Build uses 1,149,427 bytes program storage (36% of 3 MiB) and 97,212 bytes global dynamic memory (29% of 327,680 bytes).
- [x] PR #31 — upload and physical regression passed, as confirmed by the user. Raw register transitions, touch-start/release edges, gesture values and existing metric/list/graph interactions and suppression were included in the reported regression.
- [x] PR #31 review follow-up — removed the unused `rawGesture` local in the production configuration; debug logging uses `observation.rawGesture` directly. Review comment replied to and resolved.
- [x] PR #32 — Arduino Verify using the exact recorded FQBN and ESP32 core 3.3.12. Build uses 1,149,179 bytes program storage (36% of 3 MiB) and 97,212 bytes global dynamic memory (29% of 327,680 bytes), leaving 230,468 bytes; ESP32-S3 image generation completed successfully.
- [x] PR #32 — upload to the Waveshare ESP32-S3 Touch LCD 2 and confirm normal boot, as reported by the user.
- [x] PR #32 — physical metric/list candidate, vertical ownership, horizontal release, graph/quick-swipe, selector tap/cancel, controller-gesture suppression and fresh-contact regression passed, as confirmed by the user.
- [x] PR #33 — the stateless scroll-math helper compiles independently under C++14 and its compile-time checks cover drag direction/zero movement, overscroll resistance and caps, elastic bounds, nearest-step rounding, final bounds and graph spacing.
- [x] PR #33 — host-side parity comparison against the previous arithmetic passed 294,063 cases across metric/list/graph step sizes, scroll bounds, overscroll ranges and drag-coordinate deltas.
- [x] PR #33 — Arduino IDE Verify using Arduino IDE 2.3.10, the recorded ESP32-S3 FQBN and ESP32 core 3.3.12; passed as confirmed by the user on 2026-11-02.
- [x] PR #33 — upload/boot and physical metric-card, forecast-list and graph scroll regression on the Waveshare ESP32-S3 Touch LCD 2; passed as confirmed by the user on 2026-11-02.
- [ ] PR #34 — Arduino IDE Verify, upload/boot and header clock regression on the Waveshare ESP32-S3 Touch LCD 2. Confirm UK local time after successful sync, placeholder/last-valid behaviour when time is unavailable, offline demo clock behaviour, and unchanged forecast-local time labels.
- [x] PR #36 — Arduino IDE Verify passed on 2026-11-02 with the exact recorded Waveshare ESP32-S3 FQBN and ESP32 core 3.3.12: 1,151,199 bytes program storage (36% of 3 MiB app partition), 97,228 bytes global dynamic memory (29% of 327,680 bytes), 230,452 bytes remaining; ESP32-S3 image generation succeeded.
- [ ] PR #36 — upload/boot and physical time/location regression remain outstanding.
- [ ] Each subsequent extraction PR documents its scope, behaviour-preservation checks, expected memory impact and rollback point.

Phase 1 acceptance remains: firmware compiles and runs on the existing board, Weather behaviour is unchanged, regression checks pass, credentials stay untracked and memory use is compared against the Phase 0 profile.
