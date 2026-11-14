# ESP32 Gadget — Platform Refactor Roadmap & Handover

**Document status:** Working roadmap — agreed  
**Version:** 4.29  
**Last reviewed:** 2026-11-14  
**Repository:** mdrmx/esp32-gadget  
**Default branch:** main  
**Current roadmap status:** Phase 0 baseline complete. Phase 1 PRs #27–#42 are merged; PR #42 (`phase1/16-header-renderer`) merged at `ba92b8b53da8457caa18bb454f53c0d51ca6687a`. PR #44 (`phase1/17-status-screen-renderer`) is open and extracts the Weather loading/error screen composition into `src/ui/StatusScreenRenderer.h`. Weather retains status/message selection, Wi-Fi lifecycle, location/time resolution, clock-cache policy and theme definitions; the renderer receives resolved strings and a colour palette and delegates header/text drawing to existing renderers. A local strict C++11 compatibility-harness test passed. The repository runner and existing HeaderRenderer/TextRenderer regression scripts still need to be run on the development checkout; Arduino IDE Verify, upload/boot and Waveshare physical regression are pending for PR #44. No firmware build-size, memory or device-validation figures are claimed for PR #44.

---

## 1. Purpose of this document

This document is the working architectural reference and continuity handover for evolving the ESP32 Gadget from a single weather-dashboard sketch into a small, multi-application device platform.

It records:

- The direction and design principles agreed for the refactor.
- The staged implementation plan and completion criteria.
- The current technical baseline and known constraints.
- Decisions that must remain open until the code or hardware has been investigated.
- Instructions for resuming work in a future development session, including after conversation context has been lost.

Treat this as the canonical roadmap unless the user explicitly agrees to change it. Keep it updated as decisions are confirmed and phases are completed.

**Important:** this document records an agreed direction, not an assertion that the proposed architecture has already been implemented. The exact class interfaces, directory layout, modem integration approach and memory strategy must be validated during Phase 0.

## 2. Project direction

The device is intended to support several small applications on one portable ESP32-S3 platform. Weather is the first working application. Maps / route planning is a planned future application, with further applications possible later.

The platform should provide shared hardware access, connectivity, location, time, storage, input and UI mechanisms. Applications should concentrate on their own data and user experience rather than reimplementing device-level functions.

### Core principles

1. **Applications must be transport-independent.** Weather, Maps and future apps must not need to know whether internet access is supplied by Wi-Fi or cellular.
2. **Hardware and shared services belong to the platform.** Apps should consume common services rather than configure the display, touch controller, modem or network interfaces themselves.
3. **Reuse behaviour, not just code fragments.** Common interactions such as gestures, scrolling, card/list presentation and navigation should have reusable implementations with app-specific rendering and configuration.
4. **Keep the implementation appropriate for an embedded device.** Prefer small, predictable components, bounded memory use and explicit lifetimes. Avoid building a heavyweight generic UI framework.
5. **Preserve working behaviour during refactoring.** Existing weather, forecast, graph, gestures, scrolling and display behaviour are the regression baseline.
6. **Refactor incrementally.** Each stage should be reviewable, testable and independently recoverable.
7. **Do not make hardware assumptions prematurely.** Cellular hardware, power delivery, memory allocation and networking details must be checked against the actual board and libraries before being committed to code.

## 3. Current baseline

This section captures the state observed when the roadmap was created. It is a snapshot, not a substitute for checking the live repository before future changes.

### Repository and firmware

- Repository: mdrmx/esp32-gadget
- Default branch: main
- Baseline commit inspected for this roadmap: 21ee5e3d44297fe4d4cdd944e623865368d0d14f
- Main firmware sketch: weatherDashboard.ino
- At the time of review, the sketch was approximately 3,594 lines and contained around 76 functions.
- The current implementation is largely contained in the one sketch rather than split into reusable application and platform modules.

### Hardware confirmed in project use

- Waveshare ESP32-S3 Touch LCD 2.
- 320 × 240 landscape UI using an ST7789 display.
- CST816 capacitive touch controller.
- Existing power arrangement reported by the user: 2,000mAh single-cell battery, Adafruit PowerBoost 1000C, with its 5V output powering the Waveshare board.

The battery and power details are user-reported hardware context, not values inferred from the firmware. Reconfirm the exact battery rating and PowerBoost wiring before designing cellular power.

### Existing Weather application features

The current firmware includes:

- Current weather view and hourly forecast view.
- Weather metric cards with vertical scrolling.
- Hourly forecast list and graph modes.
- Touch-based navigation, dragging, scrolling and graph interaction.
- Header location and clock.
- OpenWeather condition icon download and PNG decoding.
- Periodic weather refresh.
- Offline demo data mode for UI development.
- Heap/canvas diagnostic logging.

The current UI and interactions are valuable working functionality. They should be retained unless a specific behaviour change is agreed.

### Existing connectivity and location implementation

The firmware currently uses ESP32 Wi-Fi and makes network requests directly from application functions.

Observed characteristics include:

- Wi-Fi credentials are currently compile-time values supplied through secrets.h.
- secrets.h is excluded from version control by .gitignore. Never add credentials or API keys to tracked files.
- connectWiFi() performs a blocking connection attempt with a timeout of up to 20 seconds.
- Weather, forecast, time and geolocation functions instantiate and use WiFiClientSecure / HTTPClient directly.
- Functions check WiFi.status() themselves.
- Startup location resolution attempts Google's Wi-Fi Geolocation API first, followed by IP geolocation.
- There is no dedicated GNSS receiver confirmed in the current hardware baseline.
- Location, weather and time state is held in global variables.

This is the principal reason to introduce a shared connectivity service and a separate location service.

### Existing input and rendering implementation

- CST816 sampling, coordinate handling, drag tracking, gesture handling and suppression logic are implemented in the weather sketch.
- Current metric scrolling, forecast list scrolling and graph scrolling each have app-specific logic.
- The code uses Arduino_GFX and several Arduino_Canvas buffers.
- Three canvases are allocated during setup for the forecast viewport, weather metrics viewport and tab transitions.
- Heap and canvas allocation diagnostics already exist and should be retained or improved during the refactor.

The graph interaction and touch suppression logic have been refined through hardware testing. Treat this as sensitive, regression-prone functionality.

### Documentation discrepancy to check

At the time of the roadmap review, weatherDashboard.ino defined USE_DEMO_WEATHER_DATA as 0, while DEVELOPMENT_MODE.md described 1 as the current default. Resolve this source/documentation mismatch during Phase 0; do not assume either value is correct without checking the live sketch and intended workflow.

## 4. Target architecture

The following is the intended separation of responsibilities. Exact filenames and interfaces are provisional until Phase 0.

~~~text
Device / Platform
│
├── Application Manager
│   ├── Weather application
│   ├── Maps application
│   └── Future applications
│
├── Shared platform services
│   ├── Connectivity Manager
│   │   ├── Wi-Fi interface
│   │   └── Cellular interface (future)
│   ├── Location Service
│   ├── Time Service
│   ├── Storage / Settings Service
│   └── Network request support
│
├── Shared UI and interaction
│   ├── Touch input and gesture recognition
│   ├── Scroll controller
│   ├── Reusable card / list patterns
│   ├── Header, buttons and status indicators
│   └── Loading, error and dialog components
│
└── Hardware layer
    ├── Display
    ├── Touch controller
    ├── ESP32-S3
    └── Cellular modem (future)
~~~

### 4.1 Connectivity Manager

**Agreed direction:** one shared service manages connectivity. Applications and their data services must not independently connect to Wi-Fi or cellular networks.

Expected responsibilities:

- Initialise and monitor available network interfaces.
- Expose connection state and the active network type.
- Start, stop and reconnect interfaces.
- Apply a configurable network preference and fallback policy.
- Report signal / connection information where available.
- Handle transitions and reconnection without long blocking waits in the application loop.
- Provide a common basis for network requests so Weather and Maps do not duplicate transport-specific code.

Initial policy proposal:

1. Prefer a saved Wi-Fi network.
2. Use cellular when Wi-Fi is unavailable, once cellular hardware is integrated.
3. Retry failed connections with bounded backoff.
4. Optionally switch back to preferred Wi-Fi when it becomes available.
5. Make preference and automatic switching behaviour configurable where useful.

The platform should work with Wi-Fi alone before the modem is purchased. Cellular support must be an additional backend, not a prerequisite for the refactor.

**Open technical decision:** determine how the chosen modem exposes data networking. An AT-command-based modem HTTP approach and a PPP data connection have different implications for the ESP32 networking stack and TLS clients. Investigate the exact modem board, ESP32 Arduino core and supported libraries before choosing. Do not assume every modem can be substituted transparently without an adapter.

Application-level services should remain meaningful: for example, Weather uses a WeatherService and Maps uses map/routing services. They should not contain Wi-Fi or modem setup code.

### 4.2 Wi-Fi network management and persistent settings

**Agreed direction:** known networks must be manageable on the device without recompiling and reflashing.

Use ESP32 Preferences / NVS as the initial persistent store, subject to Phase 0 validation.

Required user-facing capabilities:

- Scan for available networks.
- Show available networks and basic signal information.
- Select a network and enter its password.
- Save multiple network profiles.
- Connect to a saved network.
- Select or change the preferred network.
- Forget a saved network.
- Reconnect to saved networks after reboot.

A small on-screen keyboard will be needed for entering passwords.

Migration and security requirements:

- Provide a safe migration path from existing secrets.h Wi-Fi credentials into persistent settings, if appropriate.
- Never log, display unnecessarily, or commit network passwords.
- Keep secrets.h excluded from version control.
- Do not erase working credentials during migration or a failed connection attempt.
- Consider storage schema versioning, profile limits and a recovery / reset path.

### 4.3 Location Service

Location is a platform capability, not a Weather-only feature. Weather and Maps should obtain location through the same service.

Represent a location fix with enough metadata to judge its usefulness, for example:

- Latitude and longitude.
- Source (future GNSS, Wi-Fi geolocation, IP geolocation, or another provider).
- Accuracy estimate when supplied.
- Timestamp / freshness.

The current source order is Google Wi-Fi Geolocation followed by IP geolocation. A future GNSS receiver could become the preferred source when it provides a valid, sufficiently fresh fix. Exact source priority and stale-fix rules should be confirmed during implementation.

Do not treat IP geolocation as precise positioning. Preserve source and accuracy information rather than presenting every coordinate as equally reliable.

### 4.4 Other shared services

- **Time Service:** centralise system time initialisation, timezone behaviour and time synchronisation. Avoid each app implementing its own clock.
- **Storage / Settings Service:** provide a single place for persistent device and application settings, initially using Preferences / NVS where appropriate.
- **Display Manager:** own display initialisation, shared drawing access and any shared buffers or display resources.
- **Touch / Input Manager:** own CST816 sampling and expose semantic input to applications and reusable UI components.

### 4.5 Reusable UI and interaction components

Use small C++ classes, structs, helper functions and factory functions where they genuinely reduce duplication. Reusability should come from clear ownership and configuration, not from adding abstractions for their own sake.

Priority candidates:

- Gesture recogniser: convert raw CST816 samples into semantic tap, swipe and drag events.
- Scroll controller: position, bounds, drag tracking, elastic overscroll and release / snap behaviour.
- Card and list patterns: repeated content layout with app-provided data and rendering.
- Header and status indicators.
- Buttons and hit-testing.
- Loading, error and modal/dialog patterns.
- Navigation helpers.

**Important separation:** input and interaction logic should be independent of drawing. For example, ScrollController should calculate and expose scroll position; a Weather list or metric-card renderer decides what to draw at that position.

Use the current metric cards, forecast list and graph as the first real reuse cases. Keep their visual appearance and touch behaviour working while extracting common mechanisms.

Embedded constraints:

- Prefer predictable, bounded memory use.
- Prefer static or explicitly owned allocations where practical.
- Avoid unnecessary heap churn and repeated String allocations in hot paths.
- Use templates or callbacks only where they improve the actual design.
- Do not create a large generic UI framework.
- Keep rendering compatible with the existing Arduino_GFX approach unless Phase 0 demonstrates a compelling reason to change it.

### 4.6 Application Manager and application lifecycle

Weather should become the first application running on the platform. Maps and future applications should use the same lifecycle and shared services.

The Application Manager should eventually support:

- Launching and switching applications.
- Giving only the active app input and render ownership.
- Updating the active app without duplicating the device main loop.
- Entering and leaving an app cleanly.
- Preserving or restoring app-specific state where appropriate.
- Keeping shared hardware and platform services alive independently of the active app.

A lightweight lifecycle may include enter, update, render, input/event handling and exit (or pause/resume if required). The precise interface should be chosen after inspecting the existing sketch and memory implications.

Applications own their data and app-specific state. The platform owns hardware and shared services. Weather must not become a dependency of Maps.

## 5. Proposed source organisation

This is a starting proposal only. Confirm Arduino IDE / Arduino CLI build behaviour and existing library constraints during Phase 0 before moving files.

~~~text
esp32-gadget/
├── weatherDashboard.ino
├── secrets.h                  (local only; ignored by Git)
├── src/
│   ├── core/
│   │   ├── App.h
│   │   ├── AppManager.h
│   │   └── DeviceConfig.h
│   ├── services/
│   │   ├── ConnectivityManager.h
│   │   ├── WiFiManager.h
│   │   ├── CellularManager.h  (future)
│   │   ├── LocationService.h
│   │   ├── TimeService.h
│   │   └── StorageService.h
│   ├── hardware/
│   │   ├── DisplayManager.h
│   │   └── TouchManager.h
│   ├── ui/
│   │   ├── components/
│   │   │   ├── ScrollController.h
│   │   │   ├── Card.h
│   │   │   ├── ListView.h
│   │   │   ├── Button.h
│   │   │   └── Header.h
│   │   └── navigation/
│   │       └── NavigationManager.h
│   └── apps/
│       ├── weather/
│       │   ├── WeatherApp.h
│       │   ├── WeatherService.h
│       │   └── WeatherScreens.h
│       └── maps/
│           └── MapsApp.h         (future)
└── documentation/
~~~

The final structure may differ. Do not create empty placeholder modules simply to match this diagram.

## 6. Staged implementation roadmap

The phases below are agreed. Keep the order unless an audit finding provides a concrete reason to change it. Each phase should have a clear PR boundary and acceptance checks.

### Phase 0 — Codebase audit and baseline

**Status: COMPLETE — baseline, hardware evidence, regression procedure and module-boundary review recorded; follow-up fault-path tests remain documented**

Phase 0 established the baseline for behaviour-preserving platform extraction. Some extended failure and animation tests remain useful follow-up work, but they are explicitly recorded and do not block the first low-risk Phase 1 extraction.

**Completed deliverables:**

- [x] **Step 1 — Source inventory:** functions, file-scope state, custom structures, direct dependencies and coupling documented in [docs/PHASE0_STEP1_CODE_INVENTORY.md](docs/PHASE0_STEP1_CODE_INVENTORY.md).
- [x] **Step 2 — Runtime data-flow map:** startup, loop, periodic refresh, touch, rendering, network and location/time paths documented in [docs/PHASE0_STEP2_RUNTIME_DATA_FLOW.md](docs/PHASE0_STEP2_RUNTIME_DATA_FLOW.md).
- [x] **Step 3 — Build/toolchain baseline:** repository layout, dependencies, IDE/core/compiler/library versions, board options, clean-checkout build and physical upload/boot documented in [docs/PHASE0_STEP3_BUILD_TOOLCHAIN.md](docs/PHASE0_STEP3_BUILD_TOOLCHAIN.md).
- [x] **Step 4 — Blocking-operation audit and initial physical baseline:** synchronous network paths, Wi-Fi waits/scanning, animation delays and measured hardware timings documented in [docs/PHASE0_STEP4_BLOCKING_RESPONSIVENESS_AUDIT.md](docs/PHASE0_STEP4_BLOCKING_RESPONSIVENESS_AUDIT.md).
- [x] **Memory/PSRAM profile:** canvas placement, internal heap headroom, refresh stability and icon-cache recovery recorded in [docs/PHASE0_MEMORY_PROFILE.md](docs/PHASE0_MEMORY_PROFILE.md).
- [x] **Regression procedure:** build, display, touch, Weather, forecast, network, responsiveness, memory and credential checks documented in [docs/PHASE0_REGRESSION_CHECKLIST.md](docs/PHASE0_REGRESSION_CHECKLIST.md).
- [x] **Data ownership and module boundaries:** current state ownership, logical module responsibilities, reuse decisions and extraction sequence documented in [docs/PHASE0_MODULE_BOUNDARY_PLAN.md](docs/PHASE0_MODULE_BOUNDARY_PLAN.md).
- [x] **Arduino source-layout compile spike:** header inclusion and recursive `.cpp` compile/link verified with the recorded Arduino IDE 2.3.10 / ESP32 core 3.3.12 / Waveshare FQBN. Result recorded in [docs/PHASE0_ARDUINO_SRC_LAYOUT_SPIKE.md](docs/PHASE0_ARDUINO_SRC_LAYOUT_SPIKE.md).

**Follow-up validation carried into Phase 1:**

- Metric/tab animation touch gaps and physical touch-to-visible-response latency remain unmeasured.
- Controlled HTTP failure, Wi-Fi unavailable-at-startup and Wi-Fi-loss recovery remain untested.
- The temporary diagnostic's full-refresh touch-poll reporting window was found to be misaligned. Its 31 ms result is not used as an acceptance measurement.
- Longer memory soak and repeated varied-icon failure-path tests remain useful later.

These limitations are documented in the relevant audit and checklist. They should be addressed when a Phase 1 change touches the corresponding subsystem, not by delaying the initial data-only extraction.

### Phase 1 — Project structure and shared foundations

**Status: IN PROGRESS — PRs #27–#42 are merged and validated; PR #44 is open with the loading/error screen renderer extracted, while repository and device validation remain pending.** `WeatherApp` owns application lifecycle and refresh coordination. `WeatherViewState` owns persistent active-tab, metric/list/graph offsets and forecast presentation mode. `TextRenderer` owns shared text drawing, measurement and bounded two-line wrapping. `HeaderRenderer` owns fixed header and clock-region drawing; Weather supplies location/clock strings and colours and owns TimeService calls and clock-cache policy. PR #44 adds `StatusScreenRenderer` for loading/error screen composition. Weather retains status/message selection, Wi-Fi lifecycle, location/time resolution, clock-cache policy and theme definitions. View-specific Weather/Forecast/graph drawing, detailed touch policy and asset-cache ownership remain in the sketch.

The Arduino `src/` layout has passed a compile/link spike using the recorded board configuration. The `TouchSample`, display ownership, CST816 reader, typed gesture-code, controller-level gesture-recognition, interaction-ownership, scroll-calculation and TimeService extractions are merged. PR #31 and PR #32 were verified, uploaded and physically regression-tested successfully. PR #32 adds a small `InteractionArbiter` for the active contact owner and the decision to suppress a duplicate controller gesture. Weather continues to choose the owner through its existing hit-tests and retains all view-specific drag, timing, graph and navigation policies. PR #33 adds a stateless `ScrollController` math helper; the user confirmed Arduino IDE Verify, upload/boot and physical metric-card, forecast-list and graph scroll regression on 2026-11-02. PR #34 moves system clock validity, UK timezone setup, TimeAPI synchronisation and retry policy into `src/services/TimeService.h`; the user confirmed its physical test, while IDE Verify remains unconfirmed. Weather retains the header redraw cache and forecast timezone offset. PR #35 moves Google Wi-Fi positioning, IP fallback and reverse-geocoding into `src/services/LocationService.h`, returning an explicit source/accuracy/timestamp-bearing fix while retaining Weather-facing coordinates and display behaviour. The user confirmed the physical provider/fallback and Weather regression passed before merge.

**Current task:**

- [x] **PR #27 — Extract `TouchSample`:** the five-field data-only type now lives in `src/input/TouchSample.h`. The firmware compiled, uploaded and passed physical input regression on the Waveshare board before merge.
- [x] **PR #28 — Extract display ownership:** `src/hardware/DisplayManager.h` owns ST7789 initialisation, logical dimensions, the three existing canvases and tab-transition render-target selection. It is merged at `31029cc4f3b9902bdc038ad1aab721b397ef4bc6` after the extraction and resource-lifecycle review fixes passed compile, upload and physical display/input regression.
- [ ] Record exact post-change canvas memory-region and internal-heap figures against the Phase 0 baseline when those diagnostic readings are available.
- [x] **PR #29 — Extract CST816 raw reader:** `src/hardware/Cst816Reader.h` owns I2C setup, device detection and raw sample decoding. The user confirmed successful compile, upload and physical input regression after review fixes. `Cst816Reader::begin(bool initBus = true)` supports already-initialised shared buses; `requestFrom()` uses `size_t`.
- [x] **PR #30 — Typed gesture codes:** `src/input/GestureCode.h` provides the typed `GestureCode` values and `constexpr` raw-byte decoder. The user reports the latest code was run and tested and all gestures remain intact.
- [x] **PR #31 — Gesture recogniser:** added `src/input/GestureRecognizer.h` to convert each `TouchSample` into a `GestureObservation` containing the typed controller gesture, raw-register transition details and touch-present/start/release edges. Weather hit testing, metric/list/graph ownership, drag policy, suppression, debounce and navigation actions remain in the sketch.
  - [x] Arduino IDE Verify passed on 2026-11-02 using ESP32 core 3.3.12 and the recorded 16 MB / OPI PSRAM FQBN. Build output: 1,149,427 bytes program storage (36% of 3 MiB app partition); 97,212 bytes global dynamic memory (29% of 327,680 bytes).
  - [x] Upload and physical regression passed, as confirmed by the user.
  - [x] Review follow-up removed the production-only unused `rawGesture` local; debug output now reads `observation.rawGesture` directly.
  - [x] PR #31 merged at `3568e34cc397b5ad42827d9bdce2ee2c8908b6b9` after the user's successful Verify, upload and physical regression.
- [x] **PR #32 — Interaction ownership/arbitration:** adds `src/input/InteractionArbiter.h` to track one active touch owner (metric panel, forecast list, graph or forecast control) and expose the controller-gesture suppression decision. Weather retains hit-testing and all view-specific policies. Metric/list candidates claim ownership only after vertical drag intent is established; graph/control ownership follows existing immediate-claim behaviour. Merged at `343eb7ed0120d947990cab48cec0c688177f4ec5`.
  - [x] Arduino Verify passed with the recorded FQBN and ESP32 core 3.3.12. Build output: 1,149,179 bytes program storage (36% of 3 MiB app partition); 97,212 bytes global dynamic memory (29% of 327,680 bytes), leaving 230,468 bytes. ESP32-S3 image generation completed successfully.
  - [x] Uploaded and booted on the Waveshare ESP32-S3 Touch LCD 2; confirmed by the user.
  - [x] User confirmed the physical interaction regression passed, including metric/list candidate and axis-intent handling, drag ownership and duplicate-gesture suppression, graph ownership/quick swipe/hold drag, selector tap/cancellation and release/fresh-contact behaviour.
- [x] **PR #33 — Shared scroll calculations:** adds `src/ui/ScrollController.h` for drag delta, elastic resistance, bounded elastic position and nearest-step snapping. Metric, forecast-list and graph offsets, drag lifecycles, rendering, animations and graph-specific gesture policy remain in Weather. Standalone C++14 compilation, compile-time arithmetic checks, Arduino IDE Verify, upload/boot and physical scroll regression passed; the user confirmed the firmware and metric-card, forecast-list and graph behaviour on 2026-11-02.
- [x] **PR #34 — Time Service boundary (merged at `b11416b62ff91b7f917f16c8a484d0d2e6b7f02d`):** adds `src/services/TimeService.h` for Europe/London timezone configuration, clock validity/formatting, TimeAPI response validation and system-clock application, plus the existing retry/resynchronisation policy. Weather retains header redraw cadence/cache and `locationTimezoneOffset`; Wi-Fi lifecycle and the Google Wi-Fi → IP location chain remain unchanged. The user confirmed the physical test passed on 2026-11-02. Review follow-up removes allocation of discarded non-200 HTTP response bodies. Arduino Verify status was not separately confirmed.


- [x] **PR #35 — LocationService boundary:** `src/services/LocationService.h` owns Google Wi-Fi positioning, IP fallback and reverse-geocoding coordination. Physical provider/fallback and Weather regression passed; Arduino IDE Verify completed successfully at 1,150,279 bytes program storage and 97,228 bytes global dynamic memory.
- [x] **PR #36 — Shared HTTP request client (merged at `2cf68e08ddf7b569ea57777ca12147069f506b26`):** `src/services/HttpRequestClient.h` provides a single-use synchronous session for HTTP/TLS setup, timeout configuration, request methods, headers and response access. TimeService and LocationService use it for their requests. The final parser-follow-up Verify passed using the recorded Waveshare ESP32-S3 FQBN and ESP32 core 3.3.12: 1,151,215 bytes program storage (36%), 97,228 bytes global dynamic memory (29%), 230,452 bytes remaining, with ESP32-S3 and merged 16 MB flash image generation successful. The user confirmed upload/boot, successful Google Wi-Fi positioning (53.528682, -2.267461; reported accuracy about 18 m), reverse-geocoded place `Bury`, correct local clock, all tested interactions and current-weather/hourly-forecast data loading.
- [x] **PR #37 — Weather requests through shared HTTP client (merged at `65b3019ef6ae5a89ba25591430532bf646e3e51e`):** migrated icon download, current-weather GET and One Call forecast-page GET to `HttpRequestClient`. Arduino Verify passed on the exact Waveshare ESP32-S3 16 MB / OPI PSRAM FQBN with ESP32 core 3.3.12: 1,151,707 bytes program storage (36%), 97,228 bytes global dynamic memory (29%), 230,452 bytes remaining; ESP32-S3 and merged 16 MB images generated. The user uploaded the firmware and confirmed all tests passed, including icon fetch/cache, current-weather data, One Call hourly forecast, UV/rain metrics, pagination/partial data and full interaction regression.
- [x] **PR #38 — WeatherDataService and data models (merged at `716670c118cec37488cd895117d533a69c31d407` on 2026-11-12):** `src/services/WeatherDataService.h` owns current-weather and One Call requests/parsing, `WeatherCurrentData`, `WeatherForecastData`, fixed 48-record `ForecastItem` storage, sequential three-page pagination, duplicate filtering, provider-timezone forecast labels, UV/rain metrics, partial-data/error state and offline demo fixture. The sketch retains Wi-Fi association, LocationService/provider selection, location coordinates, icon download/cache, loading/error drawing, view offsets, screen state, rendering and input; compatibility references keep existing renderers stable. Review follow-ups bound One Call non-200 diagnostics to 240 bytes in a fixed buffer, remove unused `locationTimezoneOffset` / `weatherMain` aliases, preserve first-valid-hour UV/rain semantics and delegate Wi-Fi-unavailable forecast state to the service. The user confirms hardware regression passed; per project convention Verify and upload/boot are treated as passed. Exact build/memory figures were not supplied. Preserve current-before-forecast order, timeout/TLS policy, three-page maximum, 48-record cap, duplicate filtering, provider timezone distinct from Europe/London clock and partial-data/error semantics.

- [x] **PR #39 — WeatherApp lifecycle shell (merged at `3c613363f3b57bebe4245350cc57a3d6aa7a52d6`):** `WeatherApp` begin/update/end coordination moves initial current-weather-then-forecast retrieval, initial render selection, touch/time/clock orchestration and ten-minute refresh cadence behind fixed callback hooks. C++11 host lifecycle checks passed and the user confirmed physical validation on Waveshare hardware. Per project convention, Verify/upload/boot are recorded as passed; exact build-size/memory figures were not supplied.
- [x] **PR #40 — WeatherViewState owner (merged at `266ef646022353dd57bb552fc17a7ba523ad8708`):** `src/apps/weather/WeatherViewState.h` owns the active Weather tab, metric/list/graph scroll positions and List/Graph mode. C++11 host tests passed; the user confirmed physical Waveshare regression before merge. By project convention, Arduino Verify and upload/boot are recorded as passed. Exact build-size/memory figures were not supplied.
- [x] **PR #41 — TextRenderer boundary (merged at `108805abfeb290e0b87b497960acff8e85bc7a3d`):** `src/ui/TextRenderer.h` owns shared text drawing, centring, width/height measurement, bounded two-line wrapping and the centred Weather-line helper. `weatherDashboard.ino` routes these calls through one renderer tied to `DisplayManager`; the renderer resolves the current target for every operation, including tab-transition composition. The final merged revision includes null-target guards for all public operations and corrected over-wide text clamping. The C++11 strict-warning host tests pass, including null-target and over-wide text cases. By project convention, the merge records Arduino IDE Verify, upload/boot and Waveshare regression as passed for the merged revision. The previously recorded Verify figures (1,164,219 bytes program storage; 97,380 bytes global dynamic memory; 230,300 bytes remaining) apply only to pre-review revision `36c4d634dc9ee3a66ed6c9ca4286d41b0b216700`; no build-size figures are claimed for the final merged head.
- [x] **PR #42 — HeaderRenderer boundary (merged at `ba92b8b53da8457caa18bb454f53c0d51ca6687a`):** `src/ui/HeaderRenderer.h` owns the fixed 36 px header background, location and clock text placement, divider and clock-only redraw region. Weather continues to resolve the location label, read `TimeService`, cache the last displayed clock and decide when to redraw; theme colours are passed into the renderer. The component uses `TextRenderer` and the current `DisplayManager::target()`, including transition composition. Focused C++11 strict-warning host tests passed. Arduino IDE compilation and upload/flash passed on the tested branch revision: 1,164,667 bytes program storage, 97,396 bytes global dynamic memory and 230,284 bytes remaining. The 16 MB image was generated, flash hashes verified and the board hard-reset. The user confirmed successful boot and full Waveshare physical regression on 2026-11-13, preserving header/clock, transition composition, Weather/Forecast, loading/error and existing interaction behaviour. Both HeaderRenderer and TextRenderer repository host runners passed. These build figures apply only to firmware revision `af09b7cf8388f60db81bcdce19fc5563d70b7d0c`, not to subsequent firmware revisions.
- [ ] **PR #44 — StatusScreenRenderer boundary (implementation and focused test added; awaiting repository/device validation):** `src/ui/StatusScreenRenderer.h` owns loading/error screen composition only: background clear, existing HeaderRenderer composition, loading title/message, error title/message and retry guidance. Weather supplies resolved location and clock strings and a seven-colour palette; it retains status/message selection, network lifecycle, TimeService access, location resolution, clock-cache policy and colour definitions. The renderer owns no Weather data, services, application state or display resources and follows the active `DisplayManager::target()`. A strict C++11 test and runner cover both layouts, positions, text sizes/colours, null-target no-op and transition-canvas routing. A local compatibility-harness compile/test passed; the repository runner and HeaderRenderer/TextRenderer regression runners remain to be executed on the development checkout. Arduino IDE Verify, upload/boot and physical Waveshare validation are pending. No firmware build or memory figures are claimed.

**Phase 1 tasks:**

- [ ] Split the monolithic sketch into logical modules using the structure validated in Phase 0.
- [ ] Separate platform / hardware responsibilities from Weather application logic.
- [ ] Establish clear ownership of shared state and resources.
- [ ] Introduce common data structures and service boundaries where justified.
- [ ] Keep the existing weather application as the first and only app during this phase.

**Acceptance criteria:**

- Firmware builds and runs on the existing hardware after each extraction.
- No intended functional or visual changes.
- Existing weather, forecast, graph, scrolling and touch behaviour pass the [regression checklist](docs/PHASE0_REGRESSION_CHECKLIST.md).
- No credentials enter tracked source files.
- Memory use is compared against the [Phase 0 memory baseline](docs/PHASE0_MEMORY_PROFILE.md).

### Phase 2 — Wi-Fi management and shared connectivity

**Status: NOT STARTED**

Tasks:

- Implement persistent Wi-Fi profiles using Preferences / NVS.
- Build the network scan, selection, password-entry, save, preferred-network and forget flows.
- Provide a safe migration path from the existing compile-time Wi-Fi credentials.
- Introduce Connectivity Manager and a Wi-Fi backend.
- Make connection management non-blocking from the application loop, with timeouts and bounded reconnection.
- Route existing Weather, time and geolocation requests through the shared network boundary.
- Remove app-level assumptions that Wi-Fi is the only possible transport.

Initially, the Connectivity Manager can have only a Wi-Fi backend. Its interface should allow a future cellular backend without requiring application rewrites.

**Acceptance criteria:**

- A user can change between saved Wi-Fi networks without recompiling or reflashing.
- Saved networks survive reboot.
- Incorrect or unavailable networks fail gracefully without locking up the UI.
- Weather and existing network-dependent features work through the shared service.
- The Weather app does not own Wi-Fi connection setup or modem-specific behaviour.

### Phase 3 — Reusable UI and interaction components

**Status: NOT STARTED**

Tasks:

- Extract raw touch sampling and semantic gesture recognition.
- Implement a reusable scroll controller for bounds, drag, elastic overscroll and snap behaviour.
- Separate scrolling state from screen-specific rendering.
- Reuse the scroll mechanism for weather metrics and forecast list where the audit confirms the common behaviour.
- Extract reusable card/list, header, button, status, loading and error patterns where justified.
- Preserve graph-specific interaction and rendering where it is genuinely unique.

**Acceptance criteria:**

- Weather metric and forecast interactions use the shared mechanisms where appropriate.
- Existing directional gestures, drag ownership, graph behaviour, overscroll and snapping remain correct.
- UI components do not own hardware setup.
- Memory usage and render responsiveness remain acceptable.
- Reusable components have clear configuration and ownership; no unnecessary framework is introduced.

### Phase 4 — Application Manager

**Status: NOT STARTED**

Tasks:

- Introduce the Application Manager and agreed application lifecycle.
- Convert Weather into the first independent application.
- Move app-specific data and screen state into the Weather application boundary.
- Keep display, touch, connectivity, location, time and storage services platform-owned.
- Add a simple application launcher / switching mechanism.
- Define how apps preserve state and suspend periodic work when inactive.

**Acceptance criteria:**

- Weather launches and behaves as it did before.
- App switching does not reinitialise or corrupt shared hardware services.
- Inactive applications do not continue unnecessary refresh or rendering work.
- App state is handled predictably when leaving and returning.
- The platform can host another app without making Weather its dependency.

### Phase 5 — Cellular integration

**Status: NOT STARTED — HARDWARE NOT YET PURCHASED**

The A7670E Cat-1 / GNSS HAT has been discussed as a budget candidate, but it is not a final hardware decision. Confirm the exact board revision, modem variant, regional bands, interface and library support before implementation.

Tasks:

- Verify the purchased modem's supported LTE bands for intended networks and region.
- Confirm UART / USB wiring, logic levels, antenna requirements and control pins.
- Decide between supported AT-based networking and PPP / data-session integration.
- Design and validate the battery and power arrangement before connecting the modem.
- Implement the cellular backend behind Connectivity Manager.
- Add connection status, signal information, reconnection and fallback policy.
- Integrate GNSS as an optional Location Service provider if the chosen hardware supports it and the antenna setup permits it.
- Test TLS / HTTPS behaviour through the cellular path.

**Power constraint:** the currently reported 2,000mAh battery and Adafruit PowerBoost 1000C supply the existing Waveshare board at 5V. Do not assume this arrangement can also supply a cellular modem. Cellular transmit bursts can require substantial peak current. Confirm battery discharge rating, regulator capability, wiring, protection, charging behaviour and thermal constraints. Do not connect the modem directly to a battery or existing rail until the design is verified.

**Acceptance criteria:**

- Wi-Fi and cellular are both represented by the shared connectivity layer.
- Weather works without app-specific cellular code.
- Wi-Fi failure can fall back to cellular as configured.
- Reconnection and network switching do not freeze touch interaction.
- GNSS fixes, when available, are reported with source, accuracy and freshness.
- Battery operation is stable under cellular transmission and the charging arrangement is safe.

### Phase 6 — Maps / route-planning application

**Status: NOT STARTED**

The strategic direction discussed is an OpenStreetMap-based data foundation with a lightweight custom renderer designed for the small ESP32 display, rather than attempting to run the Google Maps application/SDK on the device.

Tasks to investigate and implement:

- Consume the shared Location Service.
- Select an appropriate map tile / map data provider and follow its usage and attribution rules.
- Implement a memory-conscious map renderer and viewport.
- Add route retrieval through a suitable routing service.
- Evaluate address/place search and route selection.
- Draw route geometry and the user's location.
- Present concise route distance, estimated time and turn instructions appropriate to the small screen.
- Cache data responsibly and define behaviour when offline.
- Reuse app launcher, connectivity, location, input, scroll and UI components.

Potential routing and geocoding services discussed include OSRM or GraphHopper, and Nominatim or Photon. No provider is selected yet. Confirm service limits, licensing, costs, coverage and connectivity requirements before committing.

**Acceptance criteria:**

- Maps works through shared platform services, not its own Wi-Fi/modem implementation.
- The user can view their position, search/select a destination and display a route.
- Route and map rendering fit device memory and display constraints.
- Location accuracy and freshness are represented honestly.
- Network failure and unavailable map data have clear fallback behaviour.
- Required map-data attribution and provider policies are respected.

## 7. Validation and regression principles

Every phase must protect the working device.

### Existing Weather regression checklist

At minimum, re-test the following on the physical Waveshare board after relevant changes:

- Display initialisation, orientation, backlight and overall rendering.
- Touch controller detection and raw touch sampling.
- Swipe between current weather and forecast.
- Vertical scrolling of metric cards.
- Forecast list scrolling and boundaries.
- Switching between forecast list and graph.
- Graph drag, point selection/positioning and gesture suppression.
- Return navigation and tab transition behaviour.
- Clock display and time synchronisation.
- Weather and hourly forecast requests.
- Google Wi-Fi geolocation and IP fallback.
- Offline/demo behaviour and graceful network failure.
- Periodic refresh without interaction becoming unresponsive.

Use the offline demo mode for UI and interaction work where possible. Use live API mode only when validating network integrations. Keep the documented demo-mode setting aligned with the actual firmware.

### Memory and performance

- Record free heap, largest free block and PSRAM use where available.
- Compare canvas allocation and buffer lifetime before and after refactors.
- Avoid allocating large per-app buffers without a demonstrated need.
- Investigate buffer reuse through shared display services.
- Check that connection attempts and API operations do not starve touch polling or rendering.
- Measure rather than assume that an abstraction is lighter or faster.

### Credentials and security

- Never commit secrets.h, Wi-Fi passwords, API keys or SIM credentials.
- Do not paste secrets into source comments, PR descriptions or logs.
- Review current TLS certificate-validation behaviour as a separate security improvement; current insecure TLS configuration should not be silently propagated as the final platform standard.
- Keep cellular APN and SIM configuration out of application code.

## 8. Development workflow and change control

- Work from a dedicated feature branch created from an up-to-date main.
- Keep changes grouped into small, understandable PRs.
- Do not push directly to main.
- Do not merge a PR unless the user explicitly asks.
- Preserve a known-good firmware baseline and make rollback straightforward.
- Each PR should state what changed, what did not change, what was tested and any manual hardware checks still required.
- Avoid combining architecture extraction with unrelated UI redesign or new application features.
- Prefer a behaviour-preserving extraction before optimising or redesigning the extracted code.
- Update this roadmap when a phase is completed, a decision is made, or the agreed direction changes.

## 9. Decisions intentionally left open

These questions should be resolved through Phase 0 or the phase where the relevant hardware becomes available:

1. Exact source-file and folder layout compatible with the current Arduino toolchain.
2. Exact C++ interfaces and whether selected components should use classes, plain structs, templates or function callbacks.
3. The boundary between Connectivity Manager, network-interface backends and HTTP/TLS request services.
4. Exact A7670E board / modem SKU, regional band support and supported networking mode.
5. Cellular power architecture, including whether the existing battery is adequate or must be upgraded.
6. Wi-Fi profile capacity, ordering, hidden-network support and recovery/reset UX.
7. Application launcher design and app state-persistence policy.
8. Shared framebuffer strategy and memory budget.
9. Map tile, routing and geocoding providers, including their terms and operational limits.

Do not resolve these by guesswork simply to make progress. Record the evidence and decision in the relevant PR or update this document.

## 10. Handover instructions for future development sessions

### Current handover — 2026-11-14 (PR #42 merged; PR #44 in progress)

Recheck GitHub before acting. PR #42 merged into `main` at `ba92b8b53da8457caa18bb454f53c0d51ca6687a`. Its validated Arduino build/upload and full physical Waveshare regression are recorded above and in the PR history. The build figures of 1,164,667 bytes program storage, 97,396 bytes global dynamic memory and 230,284 bytes remaining apply only to tested firmware revision `af09b7cf8388f60db81bcdce19fc5563d70b7d0c`; do not attribute them to later revisions. The current branch is `phase1/17-status-screen-renderer`, with PR #44 open: https://github.com/mdrmx/esp32-gadget/pull/44. It adds `src/ui/StatusScreenRenderer.h` and routes the existing loading/error wrappers through it. Weather continues to select status messages and supply resolved location/clock strings and the colour palette; the renderer owns visual composition only and delegates header/text drawing to `HeaderRenderer` and `TextRenderer`. A focused strict C++11 host test and runner are added. A local compatibility-harness compile/test passed, but the repository runner and existing HeaderRenderer/TextRenderer runners have not yet been run from the development checkout. Arduino IDE Verify, upload/boot and physical Waveshare regression remain pending. No build-size or memory figures are available for PR #44.

**Next validation sequence:** run `sh scripts/test-status-screen-renderer.sh`, `sh scripts/test-header-renderer.sh` and `sh scripts/test-text-renderer.sh`; then Arduino IDE Verify with the recorded Waveshare ESP32-S3 configuration, upload/boot and status-screen/header/Weather/Forecast/touch regression. Keep PR #44 open until the user reports the device checks.

- **Repository:** `mdrmx/esp32-gadget`; default branch `main`.
- **Latest firmware merge checkpoint:** `ba92b8b53da8457caa18bb454f53c0d51ca6687a`, PR #42 merge commit. PR #42's branch validation, upload/flash and user-confirmed Waveshare regression are recorded above. PR #44 is the current open extraction and remains pending repository/device validation.
- **PR #27:** `TouchSample` value-type extraction is merged after successful compile, upload and physical input regression.
- **PR #28:** `DisplayManager` extraction and resource-lifecycle fixes are merged after successful compile, upload and physical display/input regression. Exact post-change heap comparison remains unrecorded.
- **PR #29:** `Cst816Reader` extraction and review fixes are merged at `5ee7ce7d1a42e31d275cc7fa2e2899544d6d9a42`. The user confirmed successful compile, upload and full physical input regression. The feature branch was deleted.
- **PR #22 / #24 / #25:** permanent Step 4 audit, handover refresh and Phase 0 memory/profile documentation are merged.
- **PR #23 / #26:** temporary diagnostics and Arduino source-layout spike were closed without merging; reusable results are recorded in Phase 0 docs.
- **Phase 0:** complete. Extended fault-path, animation and touch-to-visible-response checks remain documented follow-ups.
- **Phase 1:** in progress. PRs #27–#42 are merged and validated. PR #42 adds the merged fixed Weather header/clock renderer; its recorded Arduino build/flash, boot and Waveshare regression passed. PR #44 adds `StatusScreenRenderer` for loading/error screen composition. A local strict C++11 compatibility-harness test passed; the repository host runners, Arduino Verify, upload/boot and physical Waveshare regression remain pending for PR #44. No PR #44 build-size or memory figures are available.
- **PR #30 scope/result:** `src/input/GestureCode.h` defines the typed CST816 values and a `constexpr` decoder. The existing `0x00`–`0x04` mapping and `Unknown` fallback are unchanged. The user reports running and testing the latest code with all gestures intact.
- **PR #31 scope (at merge):** `src/input/GestureRecognizer.h` converts a raw `TouchSample` into `GestureObservation`: raw gesture value, typed `GestureCode`, raw-register transition flag/previous value, and touch-present/start/release flags. At that stage, Weather still owned contact arbitration and metric/list/graph drag behaviour. PR #32 now extracts only the active-owner latch; hit-testing and view-specific interaction policy remain in the sketch.
- **PR #32 result:** [merged pull request](https://github.com/mdrmx/esp32-gadget/pull/32), merge commit `343eb7ed0120d947990cab48cec0c688177f4ec5`. Arduino Verify passed (1,149,179 bytes program storage; 97,212 bytes global dynamic memory); upload/boot and physical interaction regression were confirmed passed by the user.
- **PR #31 validation/result:** merged at `3568e34cc397b5ad42827d9bdce2ee2c8908b6b9`. Arduino IDE Verify passed on 2026-11-02 with the recorded ESP32 core 3.3.12 and 16 MB / OPI PSRAM configuration. Build result: 1,149,427 bytes program storage (36%) and 97,212 bytes global dynamic memory (29%). The user confirmed upload and full physical regression passed. The debug-only unused-local cleanup was committed as `e556e13ae38c42224946a5a67b3b320303644cfd`; it does not alter gesture handling.
- **PR #32 scope:** `src/input/InteractionArbiter.h` tracks a single active contact owner: `MetricPanel`, `ForecastList`, `ForecastGraph` or `ForecastControl`. The sketch retains region hit-testing, candidate thresholds, horizontal-intent release, graph hold/quick-swipe policy, release grace periods, graph quiet-period suppression and Weather actions. Metric/list owners are claimed only after vertical drag intent activates, preserving the existing controller-gesture pass-through while a contact is still only a candidate. Owner state is cleared on contact release.
- **PR #32 validation/result:** merged at `343eb7ed0120d947990cab48cec0c688177f4ec5`. Arduino Verify passed with ESP32 core 3.3.12 and the recorded FQBN: 1,149,179 bytes program storage and 97,212 bytes global dynamic memory; the build generated the ESP32-S3 image successfully. The user confirmed upload/boot and all physical interaction regression tests passed.
- **Toolchain:** Arduino IDE 2.3.10, ESP32 core 3.3.12, GCC 14.2.0 and the recorded ESP32-S3 FQBN with 16 MB flash and OPI PSRAM. Keep the local sketch folder named `weatherDashboard` and preserve the ignored local `secrets.h`.
- **Memory baseline:** settled startup internal 8-bit heap was 213,608 bytes free with a largest block of 118,772 bytes and a since-boot minimum of 110,532 bytes. The three display canvases were in PSRAM in the pre-extraction baseline.

### Latest merged Phase 1 extraction — PR #33: shared scroll calculations

- **Base:** live `main` at `05da9c718702f5c94838e09f7b44a75060835a20` when the branch was created.
- **Branch:** `phase1/07-scroll-controller`.
- **New helper:** `src/ui/ScrollController.h` provides stateless drag-delta, elastic-resistance, elastic-bound and nearest-step snap calculations. It has no per-view offsets, heap state, drawing or timing.
- **Sketch integration:** metric cards, forecast list and graph delegate their shared arithmetic to the helper. The existing overscroll cap (20 px), resistance divisor (3), row/point spacing, C++ integer-division rounding and per-view maximum-scroll calculations remain supplied by Weather.
- **Preserved Weather responsibilities:** per-view offsets and drag state, hit-testing, candidate/intent thresholds, interaction ownership, rendering and animation cadence, metric/list release grace periods, graph hold/quick-swipe classification, graph point selection and horizontal paging.
- **Deterministic checks:** compile-time assertions cover drag direction and zero movement, lower/upper overscroll resistance, elastic bounds, negative and positive snap rounding, final-position clamping and graph point spacing. The header compiled independently with a C++14 host compiler. This is a helper-level check, not an Arduino firmware Verify result.
- **Validation result:** the user confirmed on 2026-11-02 that Arduino IDE Verify passed with IDE 2.3.10, ESP32 core 3.3.12 and the recorded Waveshare FQBN; upload/boot passed on the Waveshare ESP32-S3 Touch LCD 2; and physical metric-card, forecast-list and graph scrolling regression passed, including edge resistance, snapping and graph-specific hold/quick-swipe behaviour. Exact PR #33 build-size figures were not supplied and are not recorded here.
- Keep this extraction isolated; do not combine it with view lifecycle, rendering, navigation, networking or service extraction.

PR #33 merged at the commit recorded above. Its scope remains limited to shared scroll arithmetic; all per-view interaction and rendering policy stays in Weather.

### Latest merged Phase 1 extraction — PR #34: TimeService boundary

- **Branch:** `phase1/08-time-service`, based on current `main` after PR #33 and its handover update.
- **New module:** `src/services/TimeService.h` owns the Europe/London TZ/DST rule, minimum valid system epoch, header clock value, TimeAPI fetch/JSON validation/epoch conversion/system-clock update, and five-minute retry / six-hour resynchronisation schedule.
- **Composition boundary:** Weather supplies the current Wi-Fi-connected state and current `millis()` value. TimeService never starts/stops Wi-Fi. Header redraw timing and last-drawn string remain Weather presentation state.
- **Timezone separation:** `locationTimezoneOffset` remains owned by Weather forecast data and is not read or written by TimeService. Google Wi-Fi location → IP fallback and reverse-geocoding remain untouched for the next Location Service slice.
- **Request transport:** TimeService still uses the existing synchronous `WiFiClientSecure` / `HTTPClient` path and current `setInsecure()` behaviour. On non-200 status, it now calls `http.end()` directly rather than allocating an unused error-body `String`. A common request executor and TLS policy remain a later, separate boundary; do not silently expand this PR.
- **Merge:** PR #34 merged at `b11416b62ff91b7f917f16c8a484d0d2e6b7f02d` on 2026-11-02.
- **Validation:** the user confirmed the physical test passed on 2026-11-02. The PR review follow-up removes `http.getString()` from the non-200 error path to avoid buffering a discarded response body; the inline review thread is replied to and resolved. Arduino IDE Verify status is not separately confirmed.

### Latest completed extraction — PR #35: LocationService boundary

- **Merged branch:** `phase1/09-location-service`, merged into `main` at `fbd87e9b73b4a21e4b2579a7e2d0c15708e410cb`.
- **Module:** `src/services/LocationService.h` owns Google Wi-Fi positioning, IP geolocation fallback and OpenWeather reverse-geocoding coordination.
- **Result contract:** `LocationFix` carries latitude, longitude, `Source` (`GoogleWifi`, `IpGeolocation`, `ConfiguredFallback`, with `Gnss` reserved for future work), optional accuracy radius, `observedAtMs` from the monotonic `millis()` clock and a coordinate-validity flag. IP geolocation remains coarse and never claims an accuracy radius.
- **Composition boundary:** Weather supplies network-connected state, API keys and configured fallback coordinates. LocationService reuses the existing Wi-Fi connection and never starts, stops or reconfigures it. Provider priority remains Google Wi-Fi → IP geolocation → configured coordinates.
- **Preserved Weather behaviour:** existing latitude/longitude and resolved-location state, display-name preference, OpenWeather reporting-station fallback and forecast `locationTimezoneOffset` remain separate and unchanged.
- **Validation:** the user confirmed the physical provider/fallback and Weather regression passed on the Waveshare ESP32-S3 Touch LCD 2 on 2026-11-02. Arduino IDE Verify completed successfully using the exact recorded FQBN: 1,150,279 bytes program storage (36%), 97,228 bytes global dynamic memory (29%) and 230,452 bytes remaining. The log reused cached sketch/library objects and completed link/image generation; it does not establish a forced clean rebuild. See `docs/PHASE0_REGRESSION_CHECKLIST.md` for the recorded test matrix.

### Completed extraction — PR #36: shared HTTP request client

- **Branch:** `phase1/10-http-request-client`, based on `main` at PR #35 merge checkpoint `fbd87e9b73b4a21e4b2579a7e2d0c15708e410cb` (including the post-merge documentation updates).
- **New module:** `src/services/HttpRequestClient.h` owns one synchronous request session: the secure socket, `HTTPClient` lifetime, connection/response timeouts, HTTP GET/POST, headers and response access.
- **Migrated callers:** `TimeService` TimeAPI GET, and `LocationService` reverse-geocode GET, Google Geolocation POST and ipwho.is GET.
- **Preserved boundaries:** URL construction, status interpretation, domain parsing, location-provider fallback, clock update and UI decisions stay in their existing domain owners. TimeAPI buffers its complete response into a `String`; reverse-geocode and IP-location responses remain parsed from their streams; Google Geolocation now reads its small success body through `getString()` so HTTPClient can handle transfer encoding, while its non-200 preview remains capped at 220 bytes.
- **TLS policy:** each migrated call explicitly opts into the existing `setInsecure()` compatibility mode. The wrapper exposes root-CA configuration for a future reviewed certificate-validation change; this PR does not change TLS trust policy.
- **Not migrated yet:** Weather icon, current-conditions and One Call forecast requests remain directly owned by `weatherDashboard.ino`. Their migration and Weather data/model separation are follow-on work.
- **Merge:** PR #36 merged at `2cf68e08ddf7b569ea57777ca12147069f506b26`.
- **Validation:** final parser-follow-up Arduino IDE Verify passed using FQBN `esp32:esp32:esp32s3:CDCOnBoot=cdc,UploadMode=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi`, ESP32 core 3.3.12 and esptool 5.3.1. Build: 1,151,215 bytes program storage (36% of 3 MiB), 97,228 bytes global dynamic memory (29%), 230,452 bytes remaining. ESP32-S3 image and merged 16 MB image were generated successfully. The output reused cached sketch/library objects and completed the link/image stage; record it as a successful Verify with cache reuse, not as a clean rebuild.
- **Physical regression:** the initial parser test returned Google `InvalidInput` and correctly fell back to IP geolocation. After the parser fix, the user uploaded and booted the updated firmware. Startup confirmed 15 Wi-Fi networks scanned, 9 eligible BSSIDs, Google Wi-Fi coordinates 53.528682, -2.267461 with an 18 m reported accuracy radius, OpenWeather reverse-geocoded place `Bury`, and selected `GoogleWifi` with a valid fix observed at 5,607 ms. The user also confirmed the local clock displays correctly, all tested interactions work and all current-weather/hourly-forecast data loads correctly.

### Latest completed extraction — PR #37: Weather HTTP requests through shared client

- **Branch:** `phase1/11-weather-http-requests`; merged into `main` at `65b3019ef6ae5a89ba25591430532bf646e3e51e`.
- **Scope:** icon PNG download, current-weather GET and One Call hourly forecast page GET now use `HttpRequestClient`; the sketch no longer directly includes or instantiates `WiFiClientSecure` / `HTTPClient`.
- **Preserved behaviour:** explicit insecure-TLS compatibility policy, 10,000 ms icon and 15,000 ms weather/forecast timeouts, PNG content-length/signature validation, 20,000-byte upper bound, 8-second stream-read budget, cache replacement/freeing, HTTP error mapping, JSON parsing, current-weather-before-forecast order, sequential pagination, three-page maximum, fixed 48-record capacity, duplicate filtering and partial forecast retention.
- **Validation:** Arduino Verify passed (1,151,707 bytes program storage; 97,228 bytes global dynamic memory; 230,452 bytes remaining) on the recorded 16 MB / OPI PSRAM Waveshare FQBN and ESP32 core 3.3.12. The user uploaded the firmware and confirmed all tests passed, including icon download/cache, current conditions, One Call hourly forecast, UV/rain metrics, paging/partial-data behaviour, navigation and full touch interaction.

### Latest completed extraction — PR #38: WeatherDataService and data model extraction

- **Branch:** `phase1/12-weather-data-service`, based on `main` after PR #37 merge.
- **Merge:** `716670c118cec37488cd895117d533a69c31d407` on 2026-11-12.
- **New module:** `src/services/WeatherDataService.h` owns current-weather and One Call requests/parsing, `WeatherCurrentData` / `WeatherForecastData`, forecast pagination, validity/error state and the deterministic offline demo fixture.
- **Sketch retains:** Wi-Fi association, `connectWiFi()`, LocationService/provider selection, selected coordinates, icon PNG download/cache, loading/error rendering, Weather screen state, offsets, drawing and input handling.
- **Compatibility facade:** temporary references keep existing renderer field names readable; the obsolete `locationTimezoneOffset` and `weatherMain` aliases have been removed. Forecast provider timezone remains in `WeatherForecastData`, separate from TimeService's Europe/London device clock.
- **Preserved behaviour:** current-weather defaults and availability; current-weather-before-forecast order; 15 s data request timeouts; existing TLS compatibility; 48-record capacity; three-page maximum; duplicate filtering; partial-data retention; first-valid-hour UV/rain selection; forecast timestamp formatting; 24-record offline demo fixture; no concurrent requests.
- **Review fixes:** One Call non-200 diagnostics read at most 240 bytes using `NetworkClient::readBytes()` into a 241-byte stack buffer. Unused aliases were removed and all three review threads addressed/resolved.
- **Validation:** the user confirms PR #38 hardware testing passed. Under the project's convention, merge also records Verify and upload/boot as passed. Exact build-size/memory figures were not supplied.

### Merged implementation — PR #39: WeatherApp application shell

- **Branch:** `phase1/13-weather-app-shell`, based on the PR #38 merged `main` checkpoint.
- **New module:** `src/apps/weather/WeatherApp.h` owns Weather application begin/update/end lifecycle, initial current-weather-before-forecast retrieval, initial rendering decision, input/time/clock callback orchestration, and the ten-minute data refresh cadence.
- **Sketch integration:** `weatherDashboard.ino` supplies fixed function-pointer adapters for its existing `pollTouch()`, TimeService synchronisation, header clock, fetch methods, active tab and draw functions. `setup()` still performs platform display/touch/time setup and live location resolution before calling `WeatherApp::begin()`; `loop()` delegates to `WeatherApp::update()` then retains the existing 30 ms pacing delay.
- **Ownership deliberately not moved in PR #39:** Weather view state, individual renderers, canvas/display resources, asset cache, input policies and domain service implementations remained in their existing owners. PR #40 subsequently moves the persistent view-state fields into `WeatherViewState`; renderer/input ownership remains separate.
- **Embedded constraints:** static function pointers only; no `std::function`, dynamic callback allocation, async work or new dependencies. Current-weather then forecast order and the existing synchronous refresh behaviour are retained.
- **Review follow-ups:** clock refresh timing now starts from the timestamp recorded after initial render (avoids an immediate redundant clock check); the error-render adapter safely handles a null message with an `Unknown error` fallback. Both inline review threads were replied to and resolved.
- **Host checks:** C++11 host lifecycle test passed for missing-hook rejection, startup call order, one-second clock cadence, scheduled refresh, forecast-tab redraw and shutdown.
- **Validation:** user confirms physical validation checks passed on the Waveshare hardware. Per project convention, Verify and upload/boot are recorded as passed. Live Weather/Forecast, touch, ten-minute refresh and offline demo checks are marked passed in the regression checklist. Exact build-size/memory figures were not supplied. PR #39 merged at `3c613363f3b57bebe4245350cc57a3d6aa7a52d6`.

### Merged implementation — PR #40: WeatherViewState

- **Branch:** `phase1/14-weather-view-state`; merge commit `266ef646022353dd57bb552fc17a7ba523ad8708`.
- **New module:** `src/apps/weather/WeatherViewState.h` owns the persistent current-tab index, Weather metric offset, forecast-list offset, forecast-graph hour/pixel offsets and List/Graph mode.
- **Sketch integration:** `weatherDashboard.ino` creates one `WeatherViewState` and exposes temporary references with the former global names. Main-tab reset and List/Graph switching delegate canonical state changes to the module. Transient drag pixels, contact state, hit-testing, gesture arbitration and animation remain in the sketch.
- **Host checks:** `sh scripts/test-weather-view-state.sh` passed using C++11 with `-Wall -Wextra -Werror -pedantic`; defaults, tab reset and mode-switch reset semantics are covered.
- **Validation:** the user confirmed physical Waveshare regression passed before merge. By project convention Arduino Verify and upload/boot are also recorded as passed. Exact firmware build-size and memory figures were not supplied.

### Merged implementation — PR #41: TextRenderer

- **Branch:** `phase1/15-text-renderer`; merge commit `108805abfeb290e0b87b497960acff8e85bc7a3d`.
- **New module:** `src/ui/TextRenderer.h` owns direct/centred text drawing, width/height measurement, bounded two-line wrapping and the centred Weather-line helper.
- **Render-target handling:** the renderer holds a reference to `DisplayManager` and resolves `target()` on each operation, following the physical display and tab-transition canvas while owning no display or canvas resources.
- **Review fixes:** all public target-dependent methods guard a null target; drawing is a safe no-op and measurement/wrapping returns 0. Over-wide centred text is clamped at x = 4 after the right-edge adjustment. Both Gemini review threads were answered and resolved.
- **Validation:** strict C++11 host tests passed. The user merged the final review-fixed branch; under the project convention Arduino Verify, upload/boot and Waveshare regression are treated as passed. The recorded figures (1,164,219 bytes program storage, 97,380 bytes global dynamic memory, 230,300 bytes remaining) are for pre-review revision `36c4d634dc9ee3a66ed6c9ca4286d41b0b216700` only; no metrics are attributed to the final merge.

### Merged implementation — PR #42: HeaderRenderer

- **Merge checkpoint:** PR #42 merged into `main` at `ba92b8b53da8457caa18bb454f53c0d51ca6687a`.
- **Module:** `src/ui/HeaderRenderer.h` owns the fixed header background, location and clock text placement, divider and clock-only redraw region.
- **Ownership:** Weather continues to resolve the location string, read `TimeService`, maintain `lastHeaderClock` and decide when to redraw; theme colours are supplied to the renderer.
- **Validation:** strict C++11 host checks, Arduino IDE build/upload/flash and complete user-confirmed Waveshare physical regression passed before merge. Build figures (1,164,667 bytes program storage, 97,396 bytes global dynamic memory and 230,284 bytes remaining) apply only to tested firmware revision `af09b7cf8388f60db81bcdce19fc5563d70b7d0c`. Both HeaderRenderer and TextRenderer repository scripts passed. No physical timing or runtime memory-profile figures are claimed.

### Current implementation — PR #44: StatusScreenRenderer

- **PR:** [#44 — refactor: extract Weather StatusScreenRenderer](https://github.com/mdrmx/esp32-gadget/pull/44).
- **Branch:** `phase1/17-status-screen-renderer`, based directly on PR #42 merge commit `ba92b8b53da8457caa18bb454f53c0d51ca6687a`.
- **New module:** `src/ui/StatusScreenRenderer.h` owns loading/error screen composition: background clear, existing header composition via `HeaderRenderer`, loading title/message and error title/message/retry guidance via `TextRenderer`.
- **Sketch integration:** Weather retains message/status selection, Wi-Fi lifecycle, location resolution, `TimeService` access, clock-cache policy and theme definitions. It supplies resolved location and clock strings, message text and a colour palette.
- **Render-target handling:** the renderer resolves the active `DisplayManager::target()`, safely avoids partial drawing when no target is available, and follows the tab-transition canvas.
- **Host coverage:** strict C++11 test and runner cover loading/error geometry, text positions/sizes/colours, null-target no-op and transition-canvas routing. A local compatibility-harness compile/test passed.
- **Pending validation:** run the repository StatusScreenRenderer runner and the existing HeaderRenderer/TextRenderer runners from the development checkout; then Arduino IDE Verify, upload/boot and complete physical Waveshare regression. No firmware build-size, memory or physical-device results are claimed yet.
- **Status:** implementation open for review; repository/device validation pending.

### Standing handover procedure

When resuming work after a context reset:

1. Read this roadmap first.
2. Check the live repository, current `main` commit, relevant branches and all open PRs; this handover can become stale.
3. Check the current firmware and related documentation rather than assuming recorded snapshots are unchanged.
4. Identify the latest completed phase and next incomplete acceptance criteria.
5. Confirm implementation decisions in code, PR discussion and user instruction.
6. Report the current state and next small task before making changes.
7. Continue from an existing branch/PR where appropriate; do not overwrite work or create a duplicate branch.
8. Use complete, copy-paste-ready code when code is requested; use feature branches and PRs; **never merge without the user's explicit instruction**. Treat a merged PR as having passed Arduino Verify, upload/boot and required hardware regression unless the user explicitly says otherwise. Record exact build/memory figures only when supplied.

**Source of truth:** the live repository is authoritative for implementation. This roadmap is authoritative for the agreed direction and staged plan. If they differ, explain the discrepancy and update the roadmap deliberately rather than silently assuming either is current.

## 11. Roadmap status

| Phase | Description | Status |
|---|---|---|
| 0 | Codebase audit and baseline | Complete — source/runtime/build audits, hardware baselines, regression checklist, module plan and Arduino `src/` compile spike recorded; follow-up fault-path tests carried forward |
| 1 | Project structure and shared foundations | In progress — PRs #27–#42 merged/validated; PR #44 StatusScreenRenderer implementation open, repository/device validation pending |
| 2 | Wi-Fi management and shared connectivity | Not started |
| 3 | Reusable UI and interaction components | Not started |
| 4 | Application Manager | Not started |
| 5 | Cellular integration | Waiting for hardware decision / purchase |
| 6 | Maps / route-planning application | Not started |

The roadmap document itself is a planning and handover artefact. Its creation does not count as completion of Phase 0.

## 12. Change history

- **2026-10-31 — Version 1.0:** Initial roadmap agreed. Records the platform-refactor direction, connectivity abstraction, persistent Wi-Fi management, reusable UI/interaction components, application lifecycle, cellular integration and Maps application. Phase 0 remains the first implementation stage.
- **2026-10-31 — Version 1.1:** Phase 0, Step 1 marked complete and linked to `docs/PHASE0_STEP1_CODE_INVENTORY.md`. Phase 0 remains in progress; the runtime data-flow map and remaining audit work are outstanding.
- **2026-10-31 — Version 1.2:** Phase 0, Step 2 marked complete and linked to `docs/PHASE0_STEP2_RUNTIME_DATA_FLOW.md`. The map records startup, loop scheduling, touch arbitration, rendering, network / location / time flow and source-configured blocking operations. Remaining Phase 0 verification is still outstanding.
- **2026-10-31 — Version 1.3:** Added the Step 3 repository-side build/toolchain audit and a placeholder-only `secrets.example.h`. Documented the Arduino sketch-folder naming constraint and clean-checkout procedure.
- **2026-10-31 — Version 1.4:** Completed Phase 0 Step 3 after capturing the working IDE, ESP32 core, GCC toolchain, board configuration and direct library versions; a successful compile, physical upload and boot were verified on the Waveshare ESP32-S3 Touch LCD 2.
- **2026-11-01 — Version 1.5:** Added the Phase 0 Step 4 static blocking-operation audit. Identified synchronous Wi-Fi and HTTP/TLS paths, render-triggered icon downloads and delay-based animations; added a physical measurement matrix.
- **2026-11-01 — Version 1.6:** Recorded initial physical startup, idle, weather, forecast and full-refresh measurements in the Step 4 audit. Captured the user's observation that touchscreen interaction remained subjectively uninterrupted during network tests, while retaining measured loop gaps as a technical finding. Marked the initial physical baseline complete; extended fault-path, animation and remaining Phase 0 audit work stay open.
- **2026-11-01 — Version 1.7:** Updated the handover after PR #22 merged. Recorded the verified main checkpoint, temporary PR #23 status, measured Step 4 baseline, current live/demo-mode behaviour and the next task (on-device memory/PSRAM profiling). Corrected stale Step 1/2 handover notes and reconciled `DEVELOPMENT_MODE.md` with the live firmware default.
- **2026-11-01 — Version 1.8:** Updated the handover after PR #24 merged and the temporary PR #23 diagnostics branch gained live-mode-safe heap/PSRAM snapshots, canvas allocation reporting and repeatable icon cache-miss testing. Recorded the exact pending branch head and hardware validation steps; no diagnostics code was merged into production.
- **2026-11-01 — Version 1.9:** Recorded the completed on-device memory/PSRAM profile, including PSRAM canvas placement, startup/internal-heap observations, stable weather/forecast refreshes and icon-cache recovery. Added the reproducible regression checklist and proposed data-ownership/module-boundary plan, documented the diagnostic touch-gap reporting-window caveat, and corrected two source/documentation mismatches. Phase 0 remains open pending a compile-only Arduino source-layout spike and final review.
- **2026-11-01 — Version 2.0:** Recorded the successful Arduino `src/` compile/link spike using the exact IDE 2.3.10, ESP32 core 3.3.12 and Waveshare board configuration. The minimal `.cpp` object was included in the final link and an ESP32-S3 image was generated. No firmware upload or production extraction was performed. Phase 0 now requires final review before Phase 1.
- **2026-11-01 — Version 2.1:** Recorded Phase 0 as complete with documented follow-up tests, reconciled the PR #23 / #26 closed-without-merge state, and started Phase 1 with PR #27 extracting the data-only `TouchSample` type into `src/input/TouchSample.h`. Build and hardware input regression remain pending.

- **2026-11-01 — Version 2.2:** Recorded successful compile, upload and physical input regression for PR #27 and its merge into `main`. Started the next isolated Phase 1 extraction in PR #28, moving ST7789 setup, dimensions, canvas allocation/lifetime and tab-transition target selection behind `DisplayManager`; build and physical display regression remain pending.

- **2026-11-01 — Version 2.3:** Recorded the user's successful compile, upload and physical display/input regression for PR #28 and marked the PR ready for review. Exact post-change canvas memory-region and internal-heap figures were not supplied, so the numeric baseline comparison remains unrecorded.

- **2026-11-01 — Version 2.4:** Recorded the PR #28 review follow-up: `DisplayManager` now owns and releases its resources, cleans up failed initialisation and replaces canvases without retaining the previous framebuffer. Concrete Arduino_GFX display and SPI bus pointer types are retained for safe deletion because the installed library's abstract base types do not declare virtual destructors. The initial extraction had passed board tests; the lifecycle follow-up awaits fresh Verify and hardware revalidation.

- **2026-11-01 — Version 2.5:** Recorded successful revalidation of PR #28 after the resource-lifecycle review fixes. The updated firmware compiled, uploaded and passed the reported physical display/input regression; PR #28 is ready to merge. Exact post-change memory diagnostic figures remain unrecorded.

- **2026-11-01 — Version 2.6:** Recorded PR #28 merge at `31029cc4f3b9902bdc038ad1aab721b397ef4bc6` and started PR #29, extracting CST816 I2C setup, device detection and raw sample decoding into `src/hardware/Cst816Reader.h` while leaving gesture and view interaction logic in the sketch.

- **2026-11-01 — Version 2.7:** Confirmed PR #28 is merged and `main` is at merge commit `31029cc4f3b9902bdc038ad1aab721b397ef4bc6`. Opened draft PR #29 for the isolated CST816 raw-reader extraction; compilation and physical input regression are pending.

- **2026-11-01 — Version 2.8:** Addressed PR #29 review comments in commit `d787bef1c8a112f9c59a1c9c7bfefd35faa5575d`: added optional bus initialisation with `begin(bool initBus = true)` and changed the `requestFrom()` result variable to `size_t`. Both inline review comments received replies. Fresh compile/upload/physical regression after these changes remains pending.

- **2026-11-01 — Version 2.9:** Recorded the user's successful Verify, upload and full physical input regression for PR #29 after review fixes. PR #29 is ready to merge; the next Phase 1 task remains gesture/interaction separation and is intentionally deferred to the next session.

- **2026-11-01 — Version 3.0:** Recorded PR #29 merge at `5ee7ce7d1a42e31d275cc7fa2e2899544d6d9a42` after the user's successful compile, upload and physical regression of the final review-fixed branch. The next task is gesture/interaction separation, deferred to the next session.

- **2026-11-02 — Version 3.1:** Corrected the handover after PR #29 merge and remote branch deletion. Set the next Phase 1 task to gesture/interaction separation; the first proposed slice is a typed `GestureCode` and pure raw-byte decoder, leaving polling arbitration, drag/suppression state and Weather actions in the sketch.

- **2026-11-02 — Version 3.2:** Started PR #30 on `phase1/04-gesture-code`, adding the typed `GestureCode` enum and pure raw-byte decoder while retaining raw transition diagnostics, polling arbitration, drag/suppression ownership and Weather actions in the sketch.

- **2026-11-02 — Version 3.2:** Started PR #30 on `phase1/04-gesture-code`, adding typed `GestureCode` values and a pure raw-byte decoder while retaining raw transition diagnostics, polling arbitration, drag/suppression ownership and Weather actions in the sketch. Build and hardware validation are pending.
- **2026-11-02 — Version 3.3:** Recorded PR #30 merge at `630ab67bc37528f0c46159b118b019e6760f7c73`. The user reports that the latest code was run and tested and all gestures remain intact. Started PR #31 on `phase1/05-gesture-recognizer`, extracting controller-level gesture observation and touch contact edges while preserving Weather interaction arbitration.

- **2026-11-02 — Version 3.4:** Recorded a successful Arduino IDE Verify for PR #31 using the specified ESP32-S3 FQBN and ESP32 core 3.3.12. The reported build uses 1,149,427 bytes program storage and 97,212 bytes global dynamic memory. Upload and physical input regression remain pending.

- **2026-11-02 — Version 3.5:** Recorded the user's confirmation that PR #31 upload and physical regression testing passed. Addressed the review comment about an unused production-build local by logging `observation.rawGesture` directly (commit `e556e13ae38c42224946a5a67b3b320303644cfd`); replied to and resolved the review thread.
- **2026-11-02 — Version 3.6:** Recorded PR #31 merge at `3568e34cc397b5ad42827d9bdce2ee2c8908b6b9` after the user confirmed Verify, upload and physical regression. Updated the next task from gesture recognition to the separate interaction-ownership/arbitration boundary.
- **2026-11-02 — Version 4.0:** Started PR #32 on `phase1/06-interaction-arbiter`. Added `src/input/InteractionArbiter.h` to track one current touch owner and centralise the decision to suppress duplicate controller gestures. Weather retains region hit-testing, drag intent, graph policy, release timing and navigation.
- **2026-11-02 — Version 4.1:** Recorded PR #32 merge at `343eb7ed0120d947990cab48cec0c688177f4ec5`. Arduino Verify passed at 1,149,179 bytes program storage and 97,212 bytes global dynamic memory; the user confirmed upload/boot and physical interaction regression passed. Updated the next Phase 1 task to the isolated shared scroll-mechanics extraction; metric/list/graph state and policies remain separate.
- **2026-11-02 — Version 4.2:** Started PR #33 on `phase1/07-scroll-controller`. Added a stateless `src/ui/ScrollController.h` for shared drag-delta, elastic-overscroll, bounded-position and nearest-step snap calculations, with compile-time arithmetic checks. Metric/list/graph offsets, drag lifecycles, animations, rendering and graph-specific policies remain in Weather. Standalone C++14 header compilation passed.
- **2026-11-02 — Version 4.3:** Recorded the user's confirmation that PR #33 passed Arduino IDE Verify with the recorded toolchain/FQBN, upload/boot on the Waveshare ESP32-S3 Touch LCD 2 and physical metric-card, forecast-list and graph scroll regression. No unreported build-size figures were inferred.
- **2026-11-02 — Version 4.4:** Recorded PR #33 merge at `4f3aeae4713b8c6252266112e476a9e50cb86c52`. Phase 1 now has PRs #27–#33 merged; the next work remains the subsequent module extraction sequence.
- **2026-11-02 — Version 4.5:** Started PR #34 on `phase1/08-time-service`. Extracted system clock validity, Europe/London TZ/DST configuration, TimeAPI sync/validation and retry scheduling into `src/services/TimeService.h`. Weather retains header redraw state and the separate forecast timezone offset; the existing Wi-Fi location provider chain remains unchanged.
- **2026-11-02 — Version 4.6:** Recorded the user's physical-test confirmation, the OOM review fix removing discarded HTTP error-body buffering, and the PR #34 merge at `b11416b62ff91b7f917f16c8a484d0d2e6b7f02d`. The review reply was posted and the inline thread resolved. Arduino IDE Verify status remains unconfirmed.
- **2026-11-02 — Version 4.9:** Reviewed the uploaded PR #35 Arduino Verify output: successful ESP32-S3 link/image generation, 1,150,279 bytes program storage, 97,228 bytes global dynamic memory and 230,452 bytes remaining. The output reused cached sketch/library objects, so it is an incremental Verify rather than a forced clean rebuild. Started PR #36 on `phase1/10-http-request-client`, adding `HttpRequestClient` and migrating TimeService/LocationService without changing endpoint parsing, timeout values or TLS policy.
- **2026-11-02 — PR #35 review follow-up:** Google Geolocation checks HTTP status before reading the response. Non-200 diagnostics are capped at 220 bytes in a fixed stack buffer; successful JSON is deserialized directly from the response stream. The reviewer reply was posted and the outdated inline thread was resolved; the user subsequently confirmed the physical regression passed.

- **2026-11-02 — Version 4.10:** Recorded successful Arduino IDE Verify for PR #36 on the exact Waveshare ESP32-S3 FQBN and ESP32 core 3.3.12. The build reports 1,151,199 bytes program storage, 97,228 bytes global dynamic memory and 230,452 bytes remaining; ESP32-S3 image and merged flash image generation completed.
- **2026-11-03 — Version 4.11:** Recorded the first PR #36 device startup: 14 Wi-Fi networks scanned, 8 usable BSSIDs, Google success-response deserialization returned `InvalidInput`, and IP geolocation plus OpenWeather reverse geocoding successfully selected Manchester. Committed `71763848dea2cd745faced3157d3a09a553568d1` to parse Google's small success body using `HTTPClient.getString()` instead of the raw response stream.
- **2026-11-03 — Version 4.13:** Recorded successful upload/boot of the parser follow-up. Google Wi-Fi positioning returned 53.528682, -2.267461 with an 18 m accuracy radius from 9 usable BSSIDs; OpenWeather reverse geocoded `Bury`. The user confirmed the dashboard displays the correct local time and all tested interactions work. Standalone Verify of this follow-up commit and broader current-weather/forecast data regression remain pending.

- **2026-11-03 — Version 4.14:** Recorded PR #36 merge at `2cf68e08ddf7b569ea57777ca12147069f506b26` and the final parser-follow-up Verify: 1,151,215 bytes program storage, 97,228 bytes global dynamic memory and 230,452 bytes remaining using the exact 16 MB / OPI PSRAM Waveshare FQBN on ESP32 core 3.3.12. The user confirmed current weather and forecast data load, correct local clock, Google Wi-Fi positioning and all tested interactions. Started PR #37 on `phase1/11-weather-http-requests` to migrate icon/current/One Call requests to the shared transport without changing domain parsing or UI ownership.
- **2026-11-03 — Version 4.16:** Recorded PR #37 merge at `65b3019ef6ae5a89ba25591430532bf646e3e51e`. The user confirmed the uploaded firmware passed all physical regression tests. Verify figures remain 1,151,707 bytes program storage, 97,228 bytes global dynamic memory and 230,452 bytes remaining on the 16 MB / OPI PSRAM Waveshare FQBN with ESP32 core 3.3.12. Started PR #38 on `phase1/12-weather-data-service` to extract the Weather data service/model while keeping Wi-Fi lifecycle, location selection, icon cache and rendering in the sketch.

- **2026-11-12 — Version 4.18:** Recorded PR #38 merge at `716670c118cec37488cd895117d533a69c31d407`. `WeatherDataService` now owns current-weather and One Call request/parsing, bounded models, pagination, validity/error state and offline fixture data. Review fixes include the 240-byte bounded diagnostic, unused sketch-alias removal, preserved first-valid-hour UV/rain semantics and service-owned forecast-unavailable state. PR documentation lists Verify/upload/physical regression as pending; confirm these separately. Next Phase 1 slice: design and extract the Weather application/view boundary while preserving existing renderers and input behaviour.
- **2026-11-12 — Version 4.19:** Recorded the user's confirmation that PR #38 hardware regression passed. Clarified the working convention that merged PRs imply Arduino Verify, upload/boot and required hardware regression passed unless explicitly stated otherwise. Exact PR #38 build-size and memory values were not supplied and remain unrecorded.
- **2026-11-13 — Version 4.20:** Started PR #39 on `phase1/13-weather-app-shell`. Added `src/apps/weather/WeatherApp.h` as a callback-injected application lifecycle shell, moving initial fetch/render coordination, touch/time/clock orchestration and the ten-minute refresh cadence out of the sketch loop while retaining the existing views, renderers, display resources and touch policies. C++11 host lifecycle tests passed.
- **2026-11-13 — Version 4.21:** Recorded the user's confirmation that PR #39 physical validation checks passed on the Waveshare hardware. Updated the regression checklist to passed, including Verify/upload/boot and the physical Weather/Forecast, touch, refresh and demo checks. PR #39 had not yet been merged at the time of this entry; exact build-size/memory figures were not supplied.
- **2026-11-13 — Version 4.22:** Recorded PR #39 merge at `3c613363f3b57bebe4245350cc57a3d6aa7a52d6`. Started PR #40 on `phase1/14-weather-view-state`, extracting persistent Weather tab, scroll and forecast-mode state into `WeatherViewState`; added C++11 host coverage for defaults and reset/switch behaviour.
- **2026-11-13 — Version 4.23:** Recorded PR #40 merge at `266ef646022353dd57bb552fc17a7ba523ad8708` after user-confirmed physical regression. Started PR #41 on `phase1/15-text-renderer`, moving shared text drawing/measurement/wrapping into `src/ui/TextRenderer.h`; host GFX-stub tests cover drawing, layout, truncation and render-target switching. Arduino Verify passed with 1,164,219 bytes program storage and 97,380 bytes global dynamic memory.
- **2026-11-13 — Version 4.24:** Recorded user-confirmed upload/boot and Waveshare physical regression on PR #41 pre-review revision `36c4d634dc9ee3a66ed6c9ca4286d41b0b216700`. Addressed Gemini review feedback with null-target guards and corrected wide-text clamping; both inline threads were replied to and resolved. Latest C++11 host tests pass; re-Verify and hardware retest for the review-fix head remain pending.
- **2026-11-13 — Version 4.25:** Recorded PR #41 merge at `108805abfeb290e0b87b497960acff8e85bc7a3d`; under the project convention, the final merged revision is treated as Verify/upload/boot and Waveshare regression validated. The previously recorded build figures remain explicitly tied to the earlier pre-review revision. Started PR #42 on `phase1/16-header-renderer`, extracting fixed header/clock drawing into `src/ui/HeaderRenderer.h`, with a focused host test and runner; local C++11 smoke checks passed, while repository-runner, Arduino Verify and device regression remain pending.
- **2026-11-13 — Version 4.26:** Recorded PR #42 Arduino IDE compile and upload/flash success on the current head using ESP32 core 3.3.12 and the recorded Waveshare ESP32-S3 16 MB / OPI PSRAM configuration: 1,164,667 bytes program storage (37% of 3 MiB), 97,396 bytes global dynamic memory (29% of 327,680 bytes), 230,284 bytes remaining. The merged 16 MB image was generated; upload completed, written-data hashes verified and the board hard-reset. The repository host runner and functional Waveshare display/interaction regression remain outstanding.
- **2026-11-13 — Version 4.27:** Recorded the user's confirmation that PR #42 firmware booted and passed complete Waveshare physical validation. Header location/clock layout, clock-only refresh, wide clock handling, tab-transition composition, Weather/Forecast navigation, loading/error screens and existing touch/scroll/graph behaviour are preserved. The repository host test runner remains pending.
- **2026-11-13 — Version 4.28:** Recorded successful user-run strict C++11 host test scripts for HeaderRenderer and TextRenderer after the shared graphics stub change. Both reported their respective tests passed. PR #42 now has build, flash, physical regression and host-test validation recorded; no outstanding validation items are known.

- **2026-11-14 — Version 4.29:** Recorded PR #42 merge at `ba92b8b53da8457caa18bb454f53c0d51ca6687a`. Started PR #44 on `phase1/17-status-screen-renderer`, extracting loading/error composition into `src/ui/StatusScreenRenderer.h`; added strict C++11 host coverage and a runner for layout, colours, null-target and transition-canvas behaviour. The local compatibility-harness compile/test passed. Repository host runners, Arduino IDE Verify, upload/boot and physical Waveshare regression remain pending; no PR #44 build or memory metrics are claimed.
