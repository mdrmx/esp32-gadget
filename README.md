# ESP32 Gadget

A Weather-first ESP32-S3 touch-device project, being incrementally refactored into a small application platform for the Waveshare ESP32-S3 Touch LCD 2.

## Current refactor progress

> **Progress snapshot:** Phase 1 rendering/UI extraction PRs #27–#51 are merged; Phase 2 connectivity PRs #52–#58 are merged; Phase 3 scroll-state/range/selector work PRs #59–#61 is complete. Phase 4 lifecycle PR #62, application-launcher PR #63 and platform-clock refinement PR #64 are merged into `main`; PR #64 merge commit is `c2d2f77ab5590c2cba58c43685c1a109c3c586b2` and PR #63 merge commit is `0af82bf94d2419f8d59be9aecb1d13f67e5d10c` (source head `ca6ba974e5e473e91b1c6a0bc7c1b622f0e9e035`). The launcher provides icon-only app selection, bottom-edge swipe navigation, launcher-first boot and platform-owned background Wi-Fi. The final icon-grid and four-colour paper/ink visual revision has passed the user's full hardware regression (2026-11-27). The platform-clock refinement was merged in [PR #64](https://github.com/mdrmx/esp32-gadget/pull/64) at `c2d2f77ab5590c2cba58c43685c1a109c3c586b2`. TimeAPI synchronisation and one-second shared-header clock redraw now run in the platform loop, independently of the active application. All four focused strict C++11 host suites passed, and the user confirmed the cold-boot, offline-to-online recovery, launcher/Weather switching and Wi-Fi-settings hardware regression passed (2026-11-27). The next Weather UI cleanup is being developed on `phase4/03-weather-header-current-conditions`: the shared header now identifies the app (`Weather`), the resolved place name moves into the Current Conditions body, and the left side now stacks a 70 px provider icon, transparency-cropped to its visible artwork, followed by condition description, temperature and feels-like reading above the unchanged right-side scrollable metric cards. Metric card values now share a consistent font size; card order begins Rain Chance, Wind, then UV Index, and the three focused metric-card/viewport/catalog host suites passed locally. The latest selector refinement gives the List/Graph highlight a uniform one-pixel inset within its track, and its host test passed. The scrolling List canvas now starts below the selector at y=72; partial card movement is retained but clipped at the viewport edge, preventing rows from overwriting the selector. The updated List viewport and Graph renderer tests have now passed. Forecast List/Graph condition icons are direct vector drawings without transparent bitmap padding. The Weather launcher bitmap is restored to its original 32×32 source frame and original rendering size. The graph now shows seven hourly points at 46 px spacing with matching 22 px outer edge padding, keeps only the top and bottom gridlines, uses a two-pixel temperature stroke, and draws its condition icons at 120% scale slightly closer to the plot. The updated launcher, Forecast List item and graph renderer suites remain to be rerun. Arduino Verify/upload and physical regression remain pending. Weather remains the only registered app, so independent multi-app switching is not yet validated. Runtime heap/largest-block/PSRAM profiling remains outstanding. See [PLATFORM_REFACTOR_ROADMAP.md](PLATFORM_REFACTOR_ROADMAP.md) for the canonical handover.

### Completed

- [x] **PR #46 — Weather metric-card renderer:** merged at `f760713ac496fd3db57f55ccd7763f6b98fb92cd`; focused host tests and user-confirmed Waveshare hardware regression passed.
- [x] **[PR #47 — Weather conditions renderer](https://github.com/mdrmx/esp32-gadget/pull/47):** merged at `f983535c52adeaa3cf9e303960bb8c92af5545a6`; strict C++11 host tests, Arduino Verify, upload/boot and physical Waveshare validation passed.

### Completed on main

- [x] **[PR #48 — Weather asset-cache boundary](https://github.com/mdrmx/esp32-gadget/pull/48):** merged into `main` at `e59a8e508670959e99115c33dbff38c200503e9a`. Both strict C++11 host suites passed, Arduino Verify passed, firmware uploaded and booted, and the complete physical Waveshare regression passed (user-confirmed 2026-11-16).
- [x] **[PR #49 — Forecast graph renderer](https://github.com/mdrmx/esp32-gadget/pull/49):** merged into `main` at `90311b142993465c37c6460b2a79ff56ac136c25`. Strict C++11 host tests, Arduino IDE Verify, firmware upload/boot and full physical Waveshare graph/List/Weather regression passed (user-confirmed 2026-11-17). Verified build: 1,166,767 bytes program storage (37% of the 3 MiB app partition), 97,444 bytes dynamic/global memory (29%), leaving 230,236 bytes. `ForecastGraphRenderer` owns graph visual composition; Weather retains data, scrolling, interaction and canvas lifecycle.
- [x] **[PR #50 — Weather metrics viewport renderer](https://github.com/mdrmx/esp32-gadget/pull/50):** merged into `main` at `9bcddc503ab6a0d1eb15b1d32ecba9c255db80ca`. The checked-in strict C++11 runner passed; Arduino Verify passed with 1,167,163 bytes program storage, 97,444 bytes dynamic/global memory and 230,236 bytes remaining. The user confirmed upload/boot and complete physical metric viewport regression, with all existing interactions preserved. `WeatherMetricsViewportRenderer` owns metric viewport row selection, placement, background fill and card delegation; Weather retains formatting, canvas lifecycle, scroll/animation and composition.

### Latest merged — PR #51

- **[PR #51 — Forecast List viewport renderer](https://github.com/mdrmx/esp32-gadget/pull/51):** merged into `main` at `1763b30cc77a9ab0e04919ae5e4cc2f1ce4e7276` and fully validated after merge. `ForecastListViewportRenderer` owns List-mode visible-row selection, positioning, neighbouring-row clipping and the settled direct fallback, reusing `ForecastListItemRenderer`. Weather retains forecast data/formatting, List/Graph selection, scroll and gesture policy, canvas lifecycle, Graph composition, controls and transitions. The strict C++11 host runner passed; Arduino IDE Verify passed; the user confirmed firmware upload/boot and the complete physical Waveshare regression, with all existing interactions preserved. Latest Verify build: 1,167,139 bytes program storage (37%), 97,444 bytes global dynamic memory (29%), 230,236 bytes remaining. The PR description's unchecked-at-merge boxes are superseded by these post-merge results.

### Phase 1 sequence

- [x] Shared text renderer
- [x] Header renderer
- [x] Loading / error status-screen renderer
- [x] Forecast list-item renderer
- [x] Weather metric-card renderer
- [x] Current Weather conditions composition
- [x] Weather icon asset-cache ownership and pre-render preparation
- [x] Forecast graph renderer — extracted, fully validated and merged into `main` in PR #49
- [x] Weather metrics viewport renderer — extracted and fully validated in merged PR #50
- [x] Forecast List viewport renderer — extracted, host-tested, firmware-verified and physically validated after merge in PR #51

### PR #50 validation record (complete; merged)

- [x] Checked-in strict C++11 repository runner: `sh scripts/test-weather-metrics-viewport-renderer.sh` → `WeatherMetricsViewportRenderer tests passed.`
- [x] Arduino IDE Verify passed using ESP32 core 3.3.12 and the recorded Waveshare ESP32-S3 16 MB / OPI PSRAM FQBN.
- [x] Verify build: 1,167,163 bytes program storage (37% of 3 MiB app partition), 97,444 bytes dynamic/global memory (29%), leaving 230,236 bytes.
- [x] User confirmed upload/boot and physical metric viewport regression: metric ordering/formatting, partial-card clipping, drag/overscroll/snap, canvas/direct fallback and tab transitions; all existing interactions remained intact.
- [x] Merged into `main` at `9bcddc503ab6a0d1eb15b1d32ecba9c255db80ca`.

### PR #51 validation record (complete; merged)

- [x] Checked-in strict C++11 repository runner: `sh scripts/test-forecast-list-viewport-renderer.sh` → `ForecastListViewportRenderer tests passed.` (user-confirmed 2026-11-25).
- [x] The companion Weather metrics viewport runner also passes on merged `main`: `sh scripts/test-weather-metrics-viewport-renderer.sh` → `WeatherMetricsViewportRenderer tests passed.`
- [x] Arduino IDE Verify passed using ESP32 core 3.3.12, esptool 5.3.1 and the recorded Waveshare ESP32-S3 16 MB / OPI PSRAM FQBN.
- [x] Verify build: 1,167,139 bytes program storage (37% of 3,145,728 bytes); 97,444 bytes global dynamic memory (29%), leaving 230,236 bytes.
- [x] ESP32-S3 firmware image and merged 16 MB flash image generated successfully. The Verify log reused cached object files; it completed the link and image-generation stages.
- [x] User confirmed firmware upload/boot and physical Waveshare regression passed, including Forecast List row order/layout, partial-edge clipping, scroll/overscroll/snap, List/Graph and Weather/Forecast switching, direct fallback, controls, graph, header/footer, touch/gesture behaviour and tab transitions; all existing interactions remained intact (2026-11-25).
- [x] Merged into `main` at `1763b30cc77a9ab0e04919ae5e4cc2f1ce4e7276`.

**Validation note:** the PR description left several checks unchecked at merge. The post-merge results above supersede those unchecked boxes. Program-storage/global-memory figures are build output, not a new runtime heap/PSRAM snapshot.

### PR #49 validation record (all checks passed)

- [x] Focused strict C++11 host test passes (`sh scripts/test-forecast-graph-renderer.sh`; user-confirmed 2026-11-17).
- [x] Arduino IDE Verify passed on 2026-11-17 using the recorded Waveshare ESP32-S3 configuration (ESP32 core 3.3.12 / esptool 5.3.1); build: 1,166,767 bytes program storage and 97,444 bytes dynamic/global memory, leaving 230,236 bytes.
- [x] Firmware uploaded and successful boot confirmed by the user on 2026-11-17.
- [x] Physical graph/List/Weather, scroll, touch, gesture and tab-transition regression passed, user-confirmed 2026-11-17.
- [x] User confirmed existing Weather/Forecast interactions remain working; build / memory figures are recorded against the verified firmware build.

For the detailed module boundaries, regression procedures and hardware baseline, see the [Platform Refactor Roadmap](PLATFORM_REFACTOR_ROADMAP.md) and [Phase 0 regression checklist](docs/PHASE0_REGRESSION_CHECKLIST.md).


## Phase 2 — Shared connectivity (PRs #52–#58 merged)

The first implementation slice was developed on [`phase2/01-connectivity-manager-wifi-backend`](https://github.com/mdrmx/esp32-gadget/tree/phase2/01-connectivity-manager-wifi-backend) and merged into `main` in [PR #52](https://github.com/mdrmx/esp32-gadget/pull/52) at `bc1948019c31ce466ea97be34f205373d05c7954`.

- Adds a transport-neutral `ConnectivityBackend` contract and `NetworkType`.
- Adds an ESP32 `WiFiBackend` and a `ConnectivityManager` with non-blocking start/update calls, a 20-second connection timeout, link-loss detection and awareness of backend/driver background reconnection. Wi-Fi profiles with null or empty passwords retain `nullptr` for open-network association.
- Adds strict C++11 host runners:
  - `sh scripts/test-connectivity-manager.sh`
  - `sh scripts/test-wifi-backend.sh`

Both focused strict C++11 host suites passed. Arduino IDE Verify passed using ESP32 core 3.3.12 / esptool 5.3.1; the ESP32-S3 application image and merged 16 MB flash image were generated. Build footprint: 1,168,551 bytes program storage (37%), 97,476 bytes global dynamic memory (29%), leaving 230,204 bytes. The upload log confirms bootloader, partition table and firmware were written and hash-verified, followed by hard reset via RTS. The user confirmed the physical tests passed.

The second slice was merged from [`phase2/02-nonblocking-weather-startup`](https://github.com/mdrmx/esp32-gadget/tree/phase2/02-nonblocking-weather-startup) in [PR #53](https://github.com/mdrmx/esp32-gadget/pull/53):

- `WeatherApp` now advances through connection, location, current-weather, forecast, asset-preparation and ready states from its regular `update()` call.
- Periodic refresh uses a separate state sequence and waits for a pending connection without blocking the application loop.
- Removed `connectWiFi()` and its synchronous 20-second polling loop; fetch adapters now consume connectivity state rather than independently starting Wi-Fi.
- The strict C++11 WeatherApp state/asset-preparation host harness passed.

**Firmware/device validation (2026-11-25):** Arduino IDE Verify passed using ESP32 core 3.3.12 / esptool 5.3.1 and the Waveshare ESP32-S3 16 MB / OPI PSRAM FQBN. Build: 1,169,075 bytes program storage, 97,492 bytes global dynamic memory and 230,188 bytes remaining. The 1,171,456-byte application image was written and hash-verified; bootloader, partition and boot-app writes were verified and the board hard-reset via RTS. The user confirmed all physical regression tests passed. Runtime free-heap/largest-block/PSRAM sampling remains outstanding and is not inferred from the build figures.

**Remaining limitation:** the connection wait itself no longer blocks touch/input scheduling. Google Wi-Fi geolocation scanning, due TimeService/TimeAPI sync requests, current/forecast HTTP requests and Weather icon asset preparation remain synchronous when their hooks/stages run. PRs #52–#57 are merged, including the scan/selection/password UI (PR #55), retry/recovery policy (PR #56) and platform-neutral Google Wi-Fi scanner plus ESP32 backend (PR #57). PR #57 deliberately does not make geolocation scanning asynchronous. Draft PR #58 is the current HTTP transport boundary refinement; Arduino/device validation remains pending.

**Remaining Phase 2 work:** perform a separate TLS certificate-validation review and capture runtime heap/largest-block/PSRAM measurements. Cellular transport support remains undecided until modem and network mode are selected.

**Saved-profile availability freshness (merged PR #56):** The Profiles screen refreshes availability immediately on entry and every 30 seconds while visible. Failed scans retry after 5 seconds, and scans remain asynchronous/non-overlapping. Link loss/recovery while Profiles remains open forces an immediate refresh. `WiFiAvailabilitySnapshot` retains the last successful SSID set while scanning or after scan failure, so badges do not switch to CHECKING/UNKNOWN mid-refresh; they update only after a successful scan completes. The user confirmed all three strict C++11 connectivity/availability suites and the full physical regression passed. The final Verify/upload figures for PR #56 are recorded above.

See [Phase 2 connectivity design](docs/PHASE2_CONNECTIVITY_MANAGER.md) and the [canonical roadmap](PLATFORM_REFACTOR_ROADMAP.md).


## Phase 2 — Google Wi-Fi geolocation scan capability (PR #57)

PR #57 extracts Google's access-point scan from `LocationService`. `WiFiGeolocationScanner` owns platform-neutral BSSID eligibility, deduplication and strongest-15 selection; `Esp32WiFiGeolocationScanBackend` owns the synchronous ESP32 scan/BSSID/RSSI/channel/cleanup calls. The sketch injects the scanner into `LocationService`, which no longer includes `WiFi.h` or calls the Wi-Fi driver directly. Google request construction and Google → IP → configured-coordinate fallback are preserved. The user reran the strict C++11 scanner runner after the final in-place workspace-buffer refinement and confirmed `WiFiGeolocationScanner tests passed.` Arduino IDE Verify/upload and general physical regression passed on 2026-11-26. Google/IP/configured-coordinate provider-failure paths were deliberately not fault-injected by user decision; they remain unverified and are explicitly non-blocking for PR #57.

## Phase 2 — Persistent Wi-Fi profile store (merged PR #54)

The storage slice was merged into `main` in [PR #54](https://github.com/mdrmx/esp32-gadget/pull/54) at `4ebd4a5469f20677ee8759fc1bc3f2dae531be2b`.

- Adds `WiFiProfileStore`: five fixed profile slots, versioned binary schema, CRC32 validation, preferred profile selection, same-SSID credential replacement and forget semantics.
- Adds `PreferencesWiFiProfileStorage`, using one NVS blob key rather than multiple partially updated keys.
- On first boot only, imports the legacy `secrets.h` profile when the NVS key is absent. Corrupt or unsupported stored records are not overwritten; an intentionally empty valid store does not restore the legacy profile.
- Copies the selected profile into device-lifetime RAM before configuring `WiFiBackend`, whose interface borrows credential pointers.

**Validation record:** the user ran `sh scripts/test-wifi-profile-store.sh` before and after the final review fixes; both strict C++11 suites passed on the final PR head. Arduino IDE Verify and upload/hash verification passed on the Waveshare ESP32-S3 16 MB / OPI PSRAM target with ESP32 core 3.3.12. The recorded build used 1,172,523 bytes program storage (37%) and 98,092 bytes global dynamic memory (29%), leaving 229,588 bytes; no new footprint figures were supplied for the final review-fixed firmware. Hardware validation confirmed initial connection, reset/reconnect and preservation of all existing interactions. For the NVS source-isolation test, `secrets.h` was temporarily changed to deliberately invalid Wi-Fi credentials; Google Wi-Fi positioning and OpenWeather reverse geocoding still succeeded after the device reported `[WIFI] Loaded the saved preferred network profile.` The original `secrets.h` was restored and normal firmware uploaded again. Following the final review fixes, the latest firmware startup again reported `[WIFI] Loaded the saved preferred network profile.` All five inline review threads were answered and resolved. PR #54 was merged into `main` at `4ebd4a5469f20677ee8759fc1bc3f2dae531be2b`.

**Security limitation:** this code does not enable NVS or ESP32 flash encryption. Credential confidentiality at rest depends on the board's flash-encryption configuration. Network scan/selection/password-entry UI is the next separate slice.

## Phase 2 — On-device Wi-Fi management (merged PR #55)

Merged into `main` in [PR #55](https://github.com/mdrmx/esp32-gadget/pull/55) at `4ba2017a02eb6e614b94c0f3df79e7540ed81e12`. The user confirmed the final revision and hardware validation before merge.

The Wi-Fi manager adds saved-profile selection/connect controls; asynchronous scan results (up to 24 entries, three rows visible) with larger SSID text, lock indicators, four signal bars and touch scrolling; scan-derived Available / Unavailable tags on saved profiles; a masked-by-default password keyboard with Show / Hide and 180 ms key feedback; open-network selection; two-step Forget confirmation; and a header Wi-Fi signal icon with a five-sample averaged RSSI level or crossed-out disconnected state. Saved Profiles and Available Networks share 43 px row slots, 41 px cards and the same finger-following scroll, resisted edge elasticity, clipping and eased settle animation. The preferred/active saved profile is outlined, and its availability badge is inset 8 px from the card edge. The header Wi-Fi target opens settings from Weather/Forecast and closes the modal. A connection change updates the NVS preferred profile only after successful association; failed changes attempt to restore the previous preferred network.

The scan state machine and bounded result container are isolated in `WiFiNetworkScanner.h`; the ESP32 adapter is in `Esp32WiFiScanBackend.h`. The settings renderer is in `WiFiSettingsRenderer.h`. Weather's staged fetch/render workflow pauses while settings are open, while the device loop, clock and connectivity manager continue to update.

The scanner and RSSI-meter host tests passed under strict C++11 warnings. The preceding Wi-Fi UI revision passed Arduino IDE Verify/upload on ESP32 core 3.3.12 / esptool 5.3.1 (1,195,063 bytes program storage; 99,844 bytes global dynamic memory; 227,836 bytes remaining). This UI supports open and WPA personal security only; enterprise/WEP/WAPI/hidden networks remain unsupported. NVS/flash encryption is not enabled by this code.

**Availability snapshot follow-up (merged PR #56):** retained scan results and in-place status refresh are merged. All three strict C++11 connectivity/availability suites, final Arduino Verify/upload and full physical regression are user-confirmed passed. Final build figures: 1,202,271 program bytes, 100,740 global dynamic bytes and 226,940 bytes remaining; application hash verified and board hard-reset via RTS.

**Phase 2 close-out:** PRs #52–#58 are merged. PR #57 extracts Google Wi-Fi geolocation scanning behind a testable platform capability; its scanner suite, Arduino Verify/upload and physical regression passed, with provider-failure fault injection deliberately deferred. PR #58 replaces the `WiFiClientSecure` alias with `NetworkClientSecure`, routes Weather icon-preparation connectivity checks through `ConnectivityManager`, and passed its source-boundary test, Arduino Verify and user-confirmed physical regression. Phase 2 is complete; TLS certificate verification and runtime heap/largest-block/PSRAM profiling remain separate follow-ups.


## Phase 2 — Network HTTP transport boundary (merged PR #58)

Merged at `807c850afb1afb1588a874cb2e0acdc25032c3b3`, the transport refinement uses ESP32 `NetworkClientSecure` in `HttpRequestClient` and removes the final Weather app-level `WiFi.status()` connection check. Wi-Fi-only scanning, RSSI reporting and network-settings UI remain platform-specific by design. This is a source-boundary refinement only: requests remain synchronous, the existing insecure-TLS compatibility policy is unchanged, and neither PPP modem support nor AT-command HTTP is implemented. The structural guard `sh scripts/test-network-transport-boundary.sh` passed on the full branch checkout. Arduino IDE Verify passed on ESP32 core 3.3.12 / esptool 5.3.1 and the user confirmed all physical regression tests passed on 2026-11-26. Compile-time footprint: 1,203,415 program bytes, 100,748 global dynamic bytes and 226,932 bytes remaining. Runtime heap/PSRAM profiling remains outstanding.

## Phase 3 — Weather scroll-state and shared range boundaries
The current follow-up branch `phase3/03-forecast-mode-selector-renderer` extracts the fixed Forecast List/Graph segmented selector from `weatherDashboard.ino` into `ForecastModeSelectorRenderer`. It owns only selector geometry and selected/unselected presentation; Weather retains mode state, hit-testing and gesture policy. The wider UI audit found existing card/list/header/status/text renderers already in place and no second independent consumer that would justify generalising the Wi-Fi manager's local button drawing.


[PR #59](https://github.com/mdrmx/esp32-gadget/pull/59) is merged at `429d4c1662243ffa1019dfe096763331711e3889`. It moved canonical metric, Forecast List and Forecast Graph scroll positions and metric/list pixel-to-viewport mapping into `WeatherScrollState`. Arduino Verify, upload/hash verification and user-confirmed physical regression passed; all existing interactions were preserved. Its scroll-state and view-state host tests passed under strict C++11 with GCC and Clang in the source-layout fixture. Build footprint: 1,203,491 bytes program storage and 100,748 bytes global dynamic memory (226,932 bytes remaining).

PR #60 on `phase3/02-shared-scroll-range-calculation` centralises the common range calculation used by metric cards, Forecast List and Forecast Graph. `WeatherScrollState::maximumScrollPixels(itemCount, visibleCount, pixelsPerItem)` derives maximum pixel range from the common item-offset calculation, and the Weather sketch uses the same `maximumOffset` logic for all three views. Separate per-view offsets, drag lifecycles, gesture arbitration, redraw throttling, snapping, graph hold/quick-swipe/point-selection policy and rendering remain unchanged. The full-repository `sh scripts/test-weather-scroll-state.sh` runner passed. Arduino IDE Verify, upload and flash hash verification passed on the Waveshare ESP32-S3 Touch LCD 2 with ESP32 core 3.3.12 / esptool 5.3.1. Build footprint: 1,203,471 bytes program storage (38% of 3,145,728 bytes), 100,748 bytes global dynamic memory (30% of 327,680 bytes), 226,932 bytes remaining. The user confirmed all hardware and physical regression tests passed. Both inline review threads were resolved before merge. PR #60 was merged into `main` at `4b70e154698f7266e43b0029b238484fcf607b76`. Runtime heap/largest-block/PSRAM profiling remains a separate outstanding measurement.


### PR #61 — Forecast mode selector renderer (validation complete)

phase3/03-forecast-mode-selector-renderer extracts the Forecast List/Graph selector's fixed geometry and selected/unselected presentation into ForecastModeSelectorRenderer. Weather retains mode state, hit-testing, touch ownership and gesture policy. Review feedback was addressed by replacing anonymous-enum geometry with typed static constexpr members and passing Mode::Graph directly in the two graph-only redraw paths; the general mode-dependent helper remains for initial List/Graph composition. Selector redraw remains after the relevant forecast canvas flush.

Validation on 2026-11-26: sh scripts/test-forecast-mode-selector-renderer.sh passed in the full repository checkout; Arduino IDE Verify, upload and flash hash verification passed on the Waveshare ESP32-S3 Touch LCD 2 using ESP32 core 3.3.12 / esptool 5.3.1. Build footprint: 1,203,887 bytes program storage, 100,764 bytes global dynamic memory and 226,916 bytes remaining. The user confirmed the full physical regression passed, including all touch gestures, tab switching, buttons, scrolling and selector redraw behaviour. A transient touch issue cleared after a board reboot/reflash. Temporary runtime diagnostics used during investigation were removed. PR #61 merged into `main` at `451f3d158a892b00513fca462c24fa3ba3e2bc8b`. Runtime heap/largest-block/PSRAM profiling remains outstanding.


## Phase 4 — Application lifecycle manager

Phase 4's lifecycle foundation is merged in [PR #62](https://github.com/mdrmx/esp32-gadget/pull/62) at `bf4e0120d3be330f5b4113aa5e80bb3ff3eb9f14`. It adds the hardware-neutral `Application` contract and fixed-capacity `ApplicationManager`, with Weather as the first managed application. The manager stores up to four non-owning app references without heap allocation and dispatches updates only to the active app. Shared display, touch, connectivity, location, time and storage services remain platform-owned. The strict C++11 lifecycle host runner, Arduino IDE compile/upload and user-confirmed physical Weather regression passed for PR #62.

The launcher implementation is merged in [PR #63](https://github.com/mdrmx/esp32-gadget/pull/63) at `0af82bf94d2419f8d59be9aecb1d13f67e5d10c` (source branch `phase4/02-application-launcher`, head `ca6ba974e5e473e91b1c6a0bc7c1b622f0e9e035`). It adds `ApplicationLauncherController`, `ApplicationLauncherRenderer`, manager registration enumeration and a quick swipe-up gesture from a centred 64 px band at the bottom edge. The device boots into the icon-only launcher before platform-owned Wi-Fi association begins; the clock and Wi-Fi signal remain live while the launcher is open. Opening the launcher suspends the active app; selecting its icon resumes it. Weather observes shared connectivity and retains workflow/cache state while suspended, including offline resume paths. The final icon-grid, paper-and-ink palette, shared-header and card-outline revision passed the user's full hardware regression on 2026-11-27. Weather is still the only registered app, so independent multi-app switching is not yet validated. The expanded strict C++11 host suites for the final launcher revision have not yet been confirmed; see `docs/PHASE4_APPLICATION_MANAGER.md`.
