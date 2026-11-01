# Phase 0 — Data ownership and Phase 1 module-boundary plan

**Status:** Proposed boundaries based on the Phase 0 source audit and hardware baseline  
**Source inspected:** `main` at `7657264db4fac8437d7b2cfc5a4b080239726ee3`  
**Primary source file:** `weatherDashboard.ino` (3,594 lines; 155 file-scope declarations in the Step 1 inventory)  
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
| `lastUpdate` and refresh intervals | `loop()` and service functions | Weather refresh coordinator | Own refresh cadence and in-flight state; no overlapping refreshes until explicit concurrency design |
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
2. **Gesture recogniser:** converts samples into semantic tap, directional swipe, drag and release information. Keep controller-specific numeric codes inside this layer.
3. **Interaction ownership/arbitration:** decides whether a metric panel, forecast list, graph or control owns the current contact, and suppresses duplicate controller events.
4. **Scroll controller:** owns bounded position, drag delta, overscroll resistance and snap calculation. It does not draw or decide which tab to open.

The Weather app remains responsible for hit regions and the actions associated with its metric/list/graph screens. The graph's hold/drag and selection behaviour is not identical to list scrolling; preserve a graph-specific controller or policy rather than forcing it into the same generic algorithm.

### 3.4 Connectivity and request execution

Keep these as separate responsibilities:

- **Connectivity manager:** network interface availability, Wi-Fi lifecycle, selected transport and reconnect policy. A Wi-Fi backend is sufficient in Phase 1/2; the interface should not encode Wi-Fi-only assumptions.
- **Request executor / HTTP client support:** HTTP/TLS client creation, timeout policy, response status and transport errors. Requests should return data/errors and must not draw loading or error screens.
- **Domain services:** Time, Location and Weather decide which endpoint to call and how to interpret its response; they do not start or stop a network interface.

The initial extraction remains synchronous and sequential. The current evidence demonstrates blocking but does not justify creating FreeRTOS tasks or concurrent requests yet. A later non-blocking design must define bounded request ownership, cancellation, response lifetime, memory limits and how results reach the main UI loop.

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

### 3.6 Time service

**Owns:** system clock validity, TimeAPI request/sync policy, retry/backoff scheduling and UK timezone configuration.

Keep two concepts distinct:
- Device civil time in `Europe/London`, used by the header clock.
- Forecast/provider timezone offset, used to format provider-local hourly timestamps.

The current implementation stores both concepts in related global state. The extraction must not merge them into one offset or change the displayed forecast times.

### 3.7 Weather data service and model

**Owns:** current-weather and One Call page requests, JSON parsing, forecast pagination, data validity and Weather data models.

**Does not own:** Wi-Fi association, location-provider selection, display calls or loading/error drawing.

Initially preserve:
- The existing current-weather fields and availability semantics.
- The fixed 48-record forecast capacity.
- Partial forecast behaviour if a later page fails after earlier data has been received.
- The existing refresh order and cadence.

Do not combine weather fetching and forecast fetching into a concurrent job in the first extraction. They currently share mutable location/time/UV/rain state and execute sequentially.

### 3.8 Weather application and views

**Owns:** Weather screen state, active Weather view, presentation layout, metric/list/graph rendering, and the mapping from semantic input to Weather actions.

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
│           └── WeatherViews.h
└── docs/
```

This is a candidate, not a mandate to create all files at once. Validate Arduino IDE handling of the proposed `src/` layout with a tiny compile-only spike before moving production functions. Do not add empty modules or reorganise the whole sketch in one PR.

## 6. Recommended extraction sequence

Each item should be its own reviewable PR and keep runtime behaviour unchanged unless a specific change is separately agreed.

1. **Build/layout spike and value types.** The Arduino `src/` compile/link spike has passed. The first real value-type extraction, [PR #27](https://github.com/mdrmx/esp32-gadget/pull/27), moved the data-only `TouchSample` into `src/input/TouchSample.h`. It passed compilation, upload and physical input regression on the Waveshare board and is merged into `main`.
2. **Display ownership.** Encapsulate ST7789 initialisation, logical dimensions, canvas allocation/lifetime/fallback and render-target selection. [PR #28](https://github.com/mdrmx/esp32-gadget/pull/28) implements this boundary in `src/hardware/DisplayManager.h`; Weather drawing and interaction remain in the sketch. Compile, upload, display/input regression and memory comparison are pending.
3. **Raw touch reader.** Move CST816 register access and raw sample decoding behind a small reader. Do not change gesture mappings.
4. **Gesture/interaction separation.** Extract gesture recognition and ownership incrementally. Retain the distinct metric, forecast-list and graph interaction states; test after each movement.
5. **Scroll mechanics.** Extract only common bounded drag/overscroll/snap calculations. Keep hit testing, graph hold/selection and per-view state in Weather.
6. **Time and location boundaries.** Introduce explicit service results and keep device time separate from forecast-local offset. Preserve provider priority and existing fallbacks.
7. **Network request support and Weather data.** Separate request transport from Weather/forecast parsing and presentation. Initially keep requests sequential and retain existing timeout/error behaviour.
8. **Weather application/views and asset cache.** Move screen state and rendering behind the Weather app boundary; remove network side effects from drawing as a separate, testable change.
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
- [ ] Each subsequent extraction PR documents its scope, behaviour-preservation checks, expected memory impact and rollback point.

Phase 1 acceptance remains: firmware compiles and runs on the existing board, Weather behaviour is unchanged, regression checks pass, credentials stay untracked and memory use is compared against the Phase 0 profile.
