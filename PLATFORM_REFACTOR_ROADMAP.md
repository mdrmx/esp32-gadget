# ESP32 Gadget — Platform Refactor Roadmap & Handover

**Document status:** Working roadmap — agreed  
**Version:** 1.2  
**Last reviewed:** 2026-10-31  
**Repository:** mdrmx/esp32-gadget  
**Default branch:** main  
**Current roadmap status:** Phase 0 — Steps 1–2 complete; remaining audit work in progress

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

**Status: IN PROGRESS — Steps 1–2 complete**

No broad refactor should begin until this review is complete.

**Phase 0 progress:**

- [x] **Step 1 — Source inventory:** functions, file-scope state, custom structures, direct dependencies and major coupling relationships documented in [docs/PHASE0_STEP1_CODE_INVENTORY.md](docs/PHASE0_STEP1_CODE_INVENTORY.md). Inventory snapshot: `weatherDashboard.ino` at main commit `55602880ff11e2f4091234091c25b5d23afd22dd`.
- [x] **Step 2 — Runtime data-flow map:** startup, loop, periodic refresh, touch, rendering, network and location/time paths documented in [docs/PHASE0_STEP2_RUNTIME_DATA_FLOW.md](docs/PHASE0_STEP2_RUNTIME_DATA_FLOW.md). Static trace based on `main` commit `9721eb688c7b96ecb55e7ee00c61096e08c8fd5f`.
- [ ] **Remaining audit —** build/toolchain verification, blocking-operation review, memory measurements, documentation reconciliation, regression checklist and target module boundaries.

Tasks:

- [x] Inventory all functions, globals, shared state and dependencies in weatherDashboard.ino. See [the Step 1 inventory](docs/PHASE0_STEP1_CODE_INVENTORY.md).
- [x] Map how setup(), loop(), touch handling, rendering, network requests and periodic updates interact. See [the Step 2 runtime map](docs/PHASE0_STEP2_RUNTIME_DATA_FLOW.md).
- Identify all direct Wi-Fi calls, HTTP/TLS clients, API requests and blocking waits.
- Identify current data models, location/time ownership and persistence.
- Map touch/gesture/drag state, metric scrolling, forecast scrolling and graph scrolling.
- Inventory Arduino_GFX canvases, heap/PSRAM use, buffer lifetimes and allocation fallbacks.
- Confirm board package, toolchain, build process, library versions and available tests.
- Check documentation against actual firmware, including the demo-mode discrepancy noted above.
- Identify code that is genuinely duplicated versus code that only looks similar.
- Record a reproducible build and hardware regression checklist.
- Capture useful baseline memory and responsiveness measurements where practical.
- Produce an agreed module-boundary plan and resolve high-risk technical questions before Phase 1.

**Deliverables:**

1. Codebase / dependency inventory.
2. Current architecture and data-flow description.
3. Memory and blocking-operation review.
4. Regression checklist and baseline build instructions.
5. Finalised module boundaries and a refined implementation sequence.

**Acceptance criteria:**

- Existing firmware build path is known and reproducible.
- Current weather and forecast behaviour is documented.
- Critical touch and scrolling behaviours have explicit test steps.
- Network and location ownership is understood.
- Memory-sensitive areas and refactor risks are identified.
- Proposed structure is validated against the actual toolchain.

### Phase 1 — Project structure and shared foundations

**Status: NOT STARTED**

Tasks:

- Split the monolithic sketch into logical modules using the structure validated in Phase 0.
- Separate platform / hardware responsibilities from Weather application logic.
- Establish clear ownership of shared state and resources.
- Introduce common data structures and service boundaries where justified.
- Keep the existing weather application as the first and only app during this phase.

**Acceptance criteria:**

- Firmware builds and runs on the existing hardware.
- No intended functional or visual changes.
- Existing weather, forecast, graph, scrolling and touch behaviour pass the regression checklist.
- No credentials enter tracked source files.
- Memory use is measured against the Phase 0 baseline.

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

When resuming work after a context reset:

1. Read this file first.
2. Check the live repository, current main commit, relevant feature branches and open PRs. This file may not yet reflect work completed in a later session.
3. Check the current firmware and related documentation rather than assuming the baseline described here is unchanged.
4. Identify the latest completed phase and the next incomplete acceptance criteria.
5. Confirm which implementation decisions have since been made in code, PR discussion or user instruction.
6. Report the current state and proposed next small task before making changes.
7. Continue from the existing branch / PR where appropriate; do not overwrite work or create a duplicate branch.
8. Preserve the user's established workflow: complete, copy-paste-ready code when code is requested; hardware regression checks; feature branches and PRs; no unrequested merge.

**Source of truth:** the live repository is authoritative for what is implemented. This document is authoritative for the agreed direction and staged plan. If they differ, explain the difference and update the roadmap deliberately; do not silently assume either one is current.

## 11. Roadmap status

| Phase | Description | Status |
|---|---|---|
| 0 | Codebase audit and baseline | In progress — Steps 1–2 complete |
| 1 | Project structure and shared foundations | Not started |
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
